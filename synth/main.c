/**
 * @file main.c
 * @brief Main entry point for the C Synthesizer application.
 *
 * Initializes shared data for two synthesizer waves, mutexes, the audio system
 * (PortAudio), and the graphical user interface (GTK+). Runs the GTK main
 * loop and handles cleanup on exit.
 */

 #include <gtk/gtk.h>
 #include <pthread.h>
 #include <stdio.h>
 #include <stdlib.h> // For exit, EXIT_FAILURE
 #include <string.h> // For strerror
 #include <errno.h>  // For errno (though strerror handles most pthread errors)
 
 // Include definitions and public function declarations from other modules
 #include "synth_data.h" // Shared data structure definition
 #include "gui.h"        // GUI creation function
 #include "audio.h"      // Audio lifecycle functions
 
 // --- Global Shared Data Instance Definition ---
 /**
  * @var g_synth_data
  * @brief Global instance of the shared synthesizer data structure.
  *
  * This structure holds all parameters (frequency, amplitude, ADSR settings, etc.)
  * and state (waveform phase, envelope stage) for **two independent waves**,
  * shared between the GUI and audio threads. Access to this structure is
  * protected by the mutex contained within it.
  * It is declared as `extern` in gui.c and audio.c.
  */
 SharedSynthData g_synth_data;
 
 // --- Forward Declarations for main.c static functions ---
 
 /**
  * @brief GTK application activation callback.
  *
  * This function is called by GTK when the application is activated
  * (either started for the first time or activated while already running).
  * It's responsible for creating the main GUI window and starting the audio stream.
  *
  * @param app The GtkApplication instance.
  * @param user_data User data passed during signal connection (unused here).
  */
 static void activate(GtkApplication *app, gpointer user_data);
 
 
 // --- Main Application Entry Point ---
 
 /**
  * @brief Main function and entry point of the synthesizer application.
  *
  * Orchestrates the application lifecycle:
  * 1. Initializes the global shared data (`g_synth_data`) with default values for **both waves**.
  * 2. Initializes the mutex within `g_synth_data`.
  * 3. Initializes the PortAudio library and audio state.
  * 4. Creates and configures the GtkApplication instance.
  * 5. Connects the GTK 'activate' signal to the local `activate` function.
  * 6. Runs the GTK main event loop (`g_application_run`), which blocks until the application quits.
  * 7. Performs cleanup after the GTK loop exits: stops audio, terminates PortAudio, destroys the mutex.
  *
  * @param argc Number of command-line arguments.
  * @param argv Array of command-line argument strings.
  * @return Application exit status (0 on success, non-zero on failure).
  */
 int main(int argc, char **argv) {
     GtkApplication *app;
     int status = 0; // Default exit status to success
     int mutex_ret;
     PaError pa_err;
 
     // --- 1. Initialize Global Data Defaults for Both Waves ---
     // Use designated initializers (C99+) for clarity
     g_synth_data = (SharedSynthData) {
         // Wave 1 Defaults
         .frequency = 440.0,        // A4 pitch
         .amplitude = 0.5,
         .waveform = WAVE_SINE,
         .attackTime = 0.01,
         .decayTime = 0.1,
         .sustainLevel = 0.7,
         .releaseTime = 0.3,
         .phase = 0.0,
         .note_active = 0,
         .currentStage = ENV_IDLE,
         .timeInStage = 0.0,
         .lastEnvValue = 0.0,
 
         // Wave 2 Defaults (Slightly different settings)
         .frequency2 = 440.0 * 3.0 / 2.0, // Perfect 5th above A4 (E5) approx 660 Hz
         .amplitude2 = 0.3,          // Lower amplitude than wave 1
         .waveform2 = WAVE_SQUARE,   // Different waveform
         .attackTime2 = 0.05,        // Slower attack than wave 1
         .decayTime2 = 0.2,
         .sustainLevel2 = 0.5,
         .releaseTime2 = 0.5,        // Longer release than wave 1
         .phase2 = 0.0,
         .note_active2 = 0,
         .currentStage2 = ENV_IDLE,
         .timeInStage2 = 0.0,
         .lastEnvValue2 = 0.0,
 
         // Common Defaults
         .sampleRate = 44100.0,     // Standard CD quality sample rate
         .waveform_drawing_area = NULL // GUI sets this later
 
         // Mutex field requires explicit initialization below
     };
     printf("Initialized synth data defaults for both waves.\n");
 
 
     // --- 2. Initialize Mutex ---
     pthread_mutexattr_t mutex_attr;
     pthread_mutexattr_init(&mutex_attr);
     // Set mutex type if needed (e.g., PTHREAD_MUTEX_RECURSIVE), default is usually fine.
     // pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
     mutex_ret = pthread_mutex_init(&g_synth_data.mutex, &mutex_attr);
     pthread_mutexattr_destroy(&mutex_attr); // Destroy attributes object after init
     if (mutex_ret != 0) {
         fprintf(stderr, "Mutex initialization failed: %s\n", strerror(mutex_ret));
         return EXIT_FAILURE; // Exit if mutex cannot be created
     }
     printf("Initialized mutex.\n");
 
     // --- 3. Initialize PortAudio & Audio State ---
     // initialize_audio now also sets initial envelope states for both waves
     pa_err = initialize_audio(&g_synth_data); // Call function from audio module
     if (pa_err != paNoError) {
         fprintf(stderr, "Failed to initialize PortAudio (Error %d: %s). Exiting.\n", pa_err, Pa_GetErrorText(pa_err));
         pthread_mutex_destroy(&g_synth_data.mutex); // Clean up already initialized mutex
         return EXIT_FAILURE; // Exit if audio system fails to initialize
     }
 
     // --- 4. Create and Configure GTK Application ---
     // Use default flags, includes handling command line args like --version
     // Using a more specific application ID is good practice
     app = gtk_application_new("com.example.csynth.dualwave", G_APPLICATION_DEFAULT_FLAGS);
      if (app == NULL) {
          fprintf(stderr, "Error: Failed to create GTK application\n");
          // Cleanup previously initialized resources
          terminate_audio();
          pthread_mutex_destroy(&g_synth_data.mutex);
          return EXIT_FAILURE;
     }
     printf("Created GTK application instance.\n");
 
     // Connect the 'activate' signal (emitted on startup) to our local activate function
     g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
 
     // --- 5. Run GTK Application Main Loop ---
     printf("Running GTK application main loop...\n");
     // g_application_run blocks here until the application quits (e.g., main window closed)
     status = g_application_run(G_APPLICATION(app), argc, argv);
     g_object_unref(app); // Release application object reference count
     printf("GTK main loop finished.\n");
 
     // --- 6. Final Cleanup (After GTK main loop exits) ---
 
     // Ensure audio stream is stopped before terminating PortAudio.
     // stop_audio() is safe to call even if already stopped.
     printf("Ensuring audio stream is stopped...\n");
     stop_audio(); // Call function from audio module
 
     // Terminate the PortAudio system fully.
     printf("Terminating audio system...\n");
     terminate_audio(); // Call function from audio module
 
     // Destroy the mutex.
     printf("Destroying mutex...\n");
     mutex_ret = pthread_mutex_destroy(&g_synth_data.mutex);
     if (mutex_ret != 0) {
         // Log error, but proceed with exit as not much else can be done :/
         fprintf(stderr, "Warning: Mutex destruction failed: %s\n", strerror(mutex_ret));
     }
 
     printf("Exiting application with status %d.\n", status);
     return status; // Return the status code from g_application_run
 }
 
 /**
  * @brief GTK application activation callback.
  *
  * This static function is connected to the GtkApplication's "activate" signal.
  * It performs the setup required when the application starts:
  * 1. Calls `create_gui()` (from gui.c) to build and show the main window.
  * 2. Calls `start_audio()` (from audio.c) to initialize and start the audio stream.
  * Includes error handling in case the audio stream fails to start.
  *
  * @param[in] app The GtkApplication instance being activated.
  * @param[in] user_data User data passed via g_signal_connect (unused here).
  */
 static void activate(GtkApplication *app, gpointer user_data) {
     PaError pa_err;
     printf("GTK Application activating...\n");
 
     // --- 1. Create the GUI ---
     // This function (defined in gui.c) builds the window, widgets for both waves,
     // connects widget signals, and shows the window.
     create_gui(app); // Call function from gui module
     printf("GUI created.\n");
 
     // --- 2. Start PortAudio Stream ---
     // This should happen after the GUI is visible or ready.
     printf("Starting audio stream...\n");
     pa_err = start_audio(&g_synth_data); // Call function from audio module
     if (pa_err != paNoError) {
         // Critical error if audio cannot start. Show an error dialog and exit.
         fprintf(stderr, "FATAL: Failed to start audio stream in activate(): %s\n", Pa_GetErrorText(pa_err));
 
         // Display a GTK error dialog to the user.
         // Passing NULL as parent makes it transient for the default screen.
         GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE,
                                                    "Failed to start audio stream:\n%s",
                                                    Pa_GetErrorText(pa_err));
         gtk_dialog_run(GTK_DIALOG(dialog)); // Show dialog modally
         gtk_widget_destroy(dialog);         // Destroy dialog after user closes it
 
         // Exit the application forcefully as audio is essential.
         exit(EXIT_FAILURE);
     }
     printf("Audio stream started.\n");
 }