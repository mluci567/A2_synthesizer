/**
 * @file gui.c
 * @brief Implements the GTK+ graphical user interface for the synthesizer.
 *
 * Creates the main window, layouts, sliders, buttons, and drawing area.
 * Connects GTK signals (e.g., slider changes, button toggles) to callback
 * functions that update the shared synthesizer parameters safely using mutexes.
 * Includes logic for visualizing the selected waveform.
 */

 #include <gtk/gtk.h>
 #include <pthread.h>
 #include <math.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <string.h>
 
 #include "gui.h"        // Include the header for this module
 #include "synth_data.h" // Include the shared data structure definition
 
 // --- External Global Shared Data Instance ---
 /**
  * @var g_synth_data
  * @brief External reference to the globally shared synthesizer data structure.
  * @note Defined in main.c, declared here as extern. Accessed by GUI callbacks.
  */
 extern SharedSynthData g_synth_data;
 
 // --- Error Handling Macros ---
 
 /**
  * @def CHECK_PTHREAD_ERR(ret, func_name)
  * @brief Checks the return value of a pthread function and prints an error if non-zero.
  * @param ret The return value from the pthread function.
  * @param func_name A string literal naming the function call being checked (within GUI context).
  */
 #define CHECK_PTHREAD_ERR(ret, func_name) \
     if (ret != 0) { \
         fprintf(stderr, "PThread Error in %s (GUI): %s\n", func_name, strerror(ret)); \
     }
 
 /**
  * @def CHECK_GTK_WIDGET(widget, name)
  * @brief Checks if a GTK widget was created successfully. Exits application on failure.
  * @param widget The pointer to the GTK widget.
  * @param name A string literal naming the widget type for the error message.
  */
 #define CHECK_GTK_WIDGET(widget, name) \
     if (widget == NULL) { \
         fprintf(stderr, "Error: Failed to create GTK widget '%s'\n", name); \
         exit(EXIT_FAILURE); \
     }
 
 // --- Forward Declarations of Static Callbacks ---
 // (Brief comments here, detailed documentation before definition)
 
 /// Callback for frequency slider value changes.
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for amplitude slider value changes.
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for waveform combo box selection changes.
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data);
 /// Callback for attack time slider value changes.
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for decay time slider value changes.
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for sustain level slider value changes.
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for release time slider value changes.
 static void on_release_slider_changed(GtkRange *range, gpointer user_data);
 /// Callback for the note on/off toggle button state changes.
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data);
 /// Callback for the drawing area's "draw" signal to render the waveform.
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
 /// Callback connected to the main window's "destroy" signal.
 static void cleanup_on_destroy();
 /// Helper function to calculate current envelope value (used for note-off).
 #ifdef TESTING
 double calculate_current_envelope(const SharedSynthData *data); // Declared in gui.h for tests
 #else
 static double calculate_current_envelope(const SharedSynthData *data);
 #endif
 
 
 // --- Public Function to Create GUI ---
 
 /**
  * @brief Creates and displays the main GUI window and its widgets.
  *
  * Initializes GTK widgets (window, boxes, labels, sliders, combo box, button, drawing area),
  * sets their initial values based on `g_synth_data`, packs them into layouts,
  * connects widget signals to their respective static callback functions,
  * and finally shows the main window.
  *
  * @param app The GtkApplication instance associated with this window.
  */
 void create_gui(GtkApplication *app) {
     GtkWidget *window;
     GtkWidget *main_vbox, *controls_hbox, *left_vbox, *right_vbox;
     GtkWidget *freq_label, *amp_label, *wave_label, *adsr_label;
     GtkWidget *freq_slider, *amp_slider, *waveform_combo;
     GtkWidget *attack_slider, *decay_slider, *sustain_slider, *release_slider;
     GtkWidget *note_button;
     GtkWidget *drawing_area;
 
     // --- Create Window ---
     window = gtk_application_window_new(app);
     CHECK_GTK_WIDGET(window, "GtkApplicationWindow");
     gtk_window_set_title(GTK_WINDOW(window), "C Synth (Modular)");
     gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
     // Connect window destruction to local cleanup function
     g_signal_connect(window, "destroy", G_CALLBACK(cleanup_on_destroy), NULL);
 
     // --- Create Layout Containers ---
     main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(main_vbox, "main_vbox");
     gtk_container_add(GTK_CONTAINER(window), main_vbox);
 
     controls_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     CHECK_GTK_WIDGET(controls_hbox, "controls_hbox");
     gtk_box_pack_start(GTK_BOX(main_vbox), controls_hbox, TRUE, TRUE, 5);
 
     left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(left_vbox, "left_vbox");
     gtk_box_pack_start(GTK_BOX(controls_hbox), left_vbox, TRUE, TRUE, 5);
 
     right_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(right_vbox, "right_vbox");
     gtk_box_pack_start(GTK_BOX(controls_hbox), right_vbox, TRUE, TRUE, 5);
 
     // --- Create Widgets ---
     // Frequency Control
     freq_label = gtk_label_new("Frequency (Hz):"); CHECK_GTK_WIDGET(freq_label, "freq_label");
     gtk_box_pack_start(GTK_BOX(left_vbox), freq_label, FALSE, FALSE, 2);
     freq_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20.0, 2000.0, 1.0); CHECK_GTK_WIDGET(freq_slider, "freq_slider");
     gtk_range_set_value(GTK_RANGE(freq_slider), g_synth_data.frequency); // Set initial value
     gtk_scale_set_draw_value(GTK_SCALE(freq_slider), TRUE); // Show value on slider
     gtk_box_pack_start(GTK_BOX(left_vbox), freq_slider, FALSE, FALSE, 5);
 
     // Amplitude Control
     amp_label = gtk_label_new("Amplitude:"); CHECK_GTK_WIDGET(amp_label, "amp_label");
     gtk_box_pack_start(GTK_BOX(left_vbox), amp_label, FALSE, FALSE, 2);
     amp_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(amp_slider, "amp_slider");
     gtk_range_set_value(GTK_RANGE(amp_slider), g_synth_data.amplitude);
     gtk_scale_set_draw_value(GTK_SCALE(amp_slider), TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox), amp_slider, FALSE, FALSE, 5);
 
     // Waveform Selection
     wave_label = gtk_label_new("Waveform:"); CHECK_GTK_WIDGET(wave_label, "wave_label");
     gtk_box_pack_start(GTK_BOX(left_vbox), wave_label, FALSE, FALSE, 2);
     const char *waveforms[] = {"Sine", "Square", "Sawtooth", "Triangle"};
     waveform_combo = gtk_combo_box_text_new(); CHECK_GTK_WIDGET(waveform_combo, "waveform_combo");
     for (int i = 0; i < G_N_ELEMENTS(waveforms); i++) {
         gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(waveform_combo), waveforms[i]);
     }
     gtk_combo_box_set_active(GTK_COMBO_BOX(waveform_combo), g_synth_data.waveform); // Set initial selection
     gtk_box_pack_start(GTK_BOX(left_vbox), waveform_combo, FALSE, FALSE, 5);
 
      // ADSR Controls (Right Side)
     adsr_label = gtk_label_new("ADSR Envelope (sec/level):"); CHECK_GTK_WIDGET(adsr_label, "adsr_label");
     gtk_box_pack_start(GTK_BOX(right_vbox), adsr_label, FALSE, FALSE, 2);
     // Attack Slider
     attack_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(attack_slider, "attack_slider");
     gtk_range_set_value(GTK_RANGE(attack_slider), g_synth_data.attackTime); gtk_scale_set_draw_value(GTK_SCALE(attack_slider), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox), attack_slider, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox), gtk_label_new("Attack"), FALSE, FALSE, 0);
     // Decay Slider
     decay_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(decay_slider, "decay_slider");
     gtk_range_set_value(GTK_RANGE(decay_slider), g_synth_data.decayTime); gtk_scale_set_draw_value(GTK_SCALE(decay_slider), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox), decay_slider, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox), gtk_label_new("Decay"), FALSE, FALSE, 0);
     // Sustain Slider
     sustain_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(sustain_slider, "sustain_slider");
     gtk_range_set_value(GTK_RANGE(sustain_slider), g_synth_data.sustainLevel); gtk_scale_set_draw_value(GTK_SCALE(sustain_slider), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox), sustain_slider, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox), gtk_label_new("Sustain"), FALSE, FALSE, 0);
     // Release Slider
     release_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.01); CHECK_GTK_WIDGET(release_slider, "release_slider");
     gtk_range_set_value(GTK_RANGE(release_slider), g_synth_data.releaseTime); gtk_scale_set_draw_value(GTK_SCALE(release_slider), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox), release_slider, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox), gtk_label_new("Release"), FALSE, FALSE, 0);
 
     // Note On/Off Button
     note_button = gtk_toggle_button_new_with_label("Note On/Off"); CHECK_GTK_WIDGET(note_button, "note_button");
     gtk_box_pack_start(GTK_BOX(left_vbox), note_button, FALSE, FALSE, 10);
 
     // Waveform Drawing Area
     drawing_area = gtk_drawing_area_new(); CHECK_GTK_WIDGET(drawing_area, "drawing_area");
     gtk_widget_set_size_request(drawing_area, 400, 150); // Request initial size
     gtk_box_pack_start(GTK_BOX(main_vbox), drawing_area, TRUE, TRUE, 5);
     // Store pointer in global struct for easy access in callbacks (like redraw requests)
     g_synth_data.waveform_drawing_area = drawing_area;
 
 
     // --- Connect Signals to Callbacks ---
     g_signal_connect(freq_slider, "value-changed", G_CALLBACK(on_frequency_slider_changed), NULL);
     g_signal_connect(amp_slider, "value-changed", G_CALLBACK(on_amplitude_slider_changed), NULL);
     g_signal_connect(waveform_combo, "changed", G_CALLBACK(on_waveform_combo_changed), NULL);
     g_signal_connect(attack_slider, "value-changed", G_CALLBACK(on_attack_slider_changed), NULL);
     g_signal_connect(decay_slider, "value-changed", G_CALLBACK(on_decay_slider_changed), NULL);
     g_signal_connect(sustain_slider, "value-changed", G_CALLBACK(on_sustain_slider_changed), NULL);
     g_signal_connect(release_slider, "value-changed", G_CALLBACK(on_release_slider_changed), NULL);
     g_signal_connect(note_button, "toggled", G_CALLBACK(on_note_on_button_toggled), NULL);
     g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), NULL);
 
 
     // --- Show Window and Widgets ---
     gtk_widget_show_all(window); // Show the window and all contained widgets
 }
 
 // --- Static GTK Callback Implementations ---
 
 /**
  * @brief Calculates the current envelope value based on the ADSR state.
  *
  * This helper function determines the envelope multiplier based on the
  * current ADSR stage (Attack, Decay, Sustain) and the time elapsed within that stage.
  * It is used primarily by the `on_note_on_button_toggled` callback to capture
  * the envelope value precisely when a note-off event occurs.
  *
  * @param[in] data Pointer to the shared synthesizer data structure containing current ADSR state and parameters.
  * @return The calculated envelope multiplier (amplitude factor) clamped between 0.0 and master amplitude.
  * Returns 0.0 if the current stage is ENV_IDLE or ENV_RELEASE, as this function
  * calculates the value *before* a potential transition to RELEASE.
  *
  * @note This function is conditionally non-static for unit testing purposes.
  */
 #ifdef TESTING
 double calculate_current_envelope(const SharedSynthData *data) {
 #else
 static double calculate_current_envelope(const SharedSynthData *data) {
 #endif
     double env_multiplier = 0.0;
     // Read necessary values from the shared data structure
     EnvelopeStage currentStage = data->currentStage;
     double timeInStage = data->timeInStage;
     double attackTime = data->attackTime;
     double decayTime = data->decayTime;
     double sustainLevel = data->sustainLevel;
     double amplitude = data->amplitude;
 
     // Calculate multiplier based on the current stage
     switch(currentStage) {
         case ENV_ATTACK:
             if (attackTime <= 0.0) { // Instant attack
                 env_multiplier = amplitude;
             } else { // Linear attack ramp
                 env_multiplier = amplitude * fmin(1.0, (timeInStage / attackTime));
             }
             break;
         case ENV_DECAY:
              if (decayTime <= 0.0 || sustainLevel >= 1.0) { // Instant decay or max sustain
                 env_multiplier = amplitude * sustainLevel;
              } else { // Linear decay ramp
                 double decay_factor = fmin(1.0, timeInStage / decayTime);
                 env_multiplier = amplitude * (1.0 - (1.0 - sustainLevel) * decay_factor);
              }
              // Clamp lower bound to sustain level during decay phase
              if (env_multiplier < amplitude * sustainLevel) {
                   env_multiplier = amplitude * sustainLevel;
              }
             break;
         case ENV_SUSTAIN:
             // Hold at sustain level
             env_multiplier = amplitude * sustainLevel;
             break;
         // This function calculates the value *before* release starts.
         // If called when already in RELEASE or IDLE, the value is effectively 0.
         case ENV_RELEASE:
         case ENV_IDLE:
         default:
             env_multiplier = 0.0;
             break;
     }
     // Return the calculated value, clamped between 0 and master amplitude
     return fmax(0.0, fmin(amplitude, env_multiplier));
 }
 
 
 /**
  * @brief Callback for the frequency slider's "value-changed" signal.
  * Updates the shared `g_synth_data.frequency` value safely using a mutex.
  * Requests a redraw of the waveform visualization area.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     // Lock mutex before accessing shared data
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_frequency_slider_changed lock");
     if (ret_lock == 0) {
         // Update frequency in shared data
         g_synth_data.frequency = gtk_range_get_value(range);
         // Unlock mutex
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_frequency_slider_changed unlock");
     }
     // Request redraw of the waveform display
     if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for the amplitude slider's "value-changed" signal.
  * Updates the shared `g_synth_data.amplitude` value safely using a mutex.
  * Requests a redraw of the waveform visualization area.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_amplitude_slider_changed lock");
      if (ret_lock == 0) {
         g_synth_data.amplitude = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_amplitude_slider_changed unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for the waveform combo box's "changed" signal.
  * Updates the shared `g_synth_data.waveform` value safely using a mutex.
  * Requests a redraw of the waveform visualization area.
  * @param widget The GtkComboBox widget that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_waveform_combo_changed lock");
      if (ret_lock == 0) {
         // Update waveform type based on the active index
         g_synth_data.waveform = (WaveformType)gtk_combo_box_get_active(widget);
          ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
          CHECK_PTHREAD_ERR(ret_unlock, "on_waveform_combo_changed unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for the attack time slider's "value-changed" signal.
  * Updates the shared `g_synth_data.attackTime` value safely using a mutex.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_attack_slider_changed lock");
      if (ret_lock == 0) {
         g_synth_data.attackTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_attack_slider_changed unlock");
     }
 }
 
 /**
  * @brief Callback for the decay time slider's "value-changed" signal.
  * Updates the shared `g_synth_data.decayTime` value safely using a mutex.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
      ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
      CHECK_PTHREAD_ERR(ret_lock, "on_decay_slider_changed lock");
      if (ret_lock == 0) {
         g_synth_data.decayTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_decay_slider_changed unlock");
     }
 }
 
 /**
  * @brief Callback for the sustain level slider's "value-changed" signal.
  * Updates the shared `g_synth_data.sustainLevel` value safely using a mutex.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_sustain_slider_changed lock");
      if (ret_lock == 0) {
         g_synth_data.sustainLevel = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_sustain_slider_changed unlock");
     }
 }
 
 /**
  * @brief Callback for the release time slider's "value-changed" signal.
  * Updates the shared `g_synth_data.releaseTime` value safely using a mutex.
  * @param range The GtkRange widget (slider) that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_release_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_release_slider_changed lock");
      if (ret_lock == 0) {
         g_synth_data.releaseTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "on_release_slider_changed unlock");
     }
 }
 
 /**
  * @brief Callback for the Note On/Off toggle button's "toggled" signal.
  * Handles note-on and note-off events by updating the ADSR envelope state
  * in the shared `g_synth_data` structure.
  * For note-on: Sets stage to ATTACK, resets phase and timeInStage.
  * For note-off: Calculates the current envelope level, stores it in `lastEnvValue`,
  * sets stage to RELEASE, and resets timeInStage.
  * Uses a mutex to ensure thread-safe updates.
  * @param button The GtkToggleButton widget that emitted the signal.
  * @param user_data User data passed during signal connection (unused).
  */
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data) {
     int ret_lock, ret_unlock;
     gboolean is_active = gtk_toggle_button_get_active(button); // Check button state
 
     // Lock mutex before accessing/modifying shared ADSR state
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_note_on_button_toggled lock");
      if (ret_lock == 0) {
          // --- NOTE ON ---
          // If button is pressed (active) and synth is currently idle:
          if (is_active && g_synth_data.currentStage == ENV_IDLE) {
              g_synth_data.note_active = 1;     // Mark note as conceptually "down"
              g_synth_data.currentStage = ENV_ATTACK; // Start attack phase
              g_synth_data.timeInStage = 0.0;   // Reset time in stage
              g_synth_data.phase = 0.0;         // Reset waveform phase
              g_synth_data.lastEnvValue = 0.0;  // Clear previous release value
              printf("GUI: Note ON -> ATTACK\n");
          }
          // --- NOTE OFF ---
          // If button is released (inactive) and synth is playing (not idle or already releasing):
          else if (!is_active && (g_synth_data.currentStage == ENV_ATTACK ||
                                  g_synth_data.currentStage == ENV_DECAY ||
                                  g_synth_data.currentStage == ENV_SUSTAIN))
          {
              // Calculate the envelope value *at this exact moment* before changing stage
              g_synth_data.lastEnvValue = calculate_current_envelope(&g_synth_data);
 
              // Transition to RELEASE stage
              g_synth_data.currentStage = ENV_RELEASE;
              g_synth_data.timeInStage = 0.0; // Reset time for release phase
              // Note: g_synth_data.note_active remains 1 until audio thread finishes release
              printf("GUI: Note OFF -> RELEASE (from value %.4f)\n", g_synth_data.lastEnvValue);
          }
          // No action needed if button state changes without valid ADSR transition
          // (e.g., pressing Note On when already playing, pressing Note Off when already idle/releasing)
 
          // Unlock mutex
          ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
          CHECK_PTHREAD_ERR(ret_unlock, "on_note_on_button_toggled unlock");
      }
 }
 
 
 /**
  * @brief Callback for the drawing area's "draw" signal.
  * Renders a visualization of the currently selected waveform (without envelope)
  * onto the drawing area widget using Cairo graphics.
  * Reads necessary parameters (frequency, amplitude, waveform, sample rate)
  * safely from `g_synth_data` using a mutex.
  * @param widget The GtkDrawingArea widget that needs redrawing.
  * @param cr The Cairo drawing context.
  * @param user_data User data passed during signal connection (unused).
  * @return FALSE to indicate the event was handled and default handlers shouldn't run.
  */
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
     guint width, height;
     double line_width = 1.5;
     int num_samples_to_draw;
     double sample_x_step;
     double local_freq, local_amp, local_sampleRate; // Local copies of parameters
     WaveformType local_wave;
     double current_phase = 0.0; // Phase for drawing, independent of audio thread
     int ret_lock, ret_unlock;
 
     // Get drawing area dimensions
     width = gtk_widget_get_allocated_width(widget);
     height = gtk_widget_get_allocated_height(widget);
     if (width <= 0 || height <= 0) return FALSE; // Nothing to draw
 
     // Set background and line properties
     cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); // Dark background
     cairo_paint(cr);
     cairo_set_source_rgb(cr, 0.3, 0.7, 1.0); // Blue line color
     cairo_set_line_width(cr, line_width);
 
     // --- Safely read parameters needed for drawing ---
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "on_draw_event lock");
     if (ret_lock != 0) return FALSE; // Cannot draw without parameters
     local_freq = g_synth_data.frequency;
     local_amp = g_synth_data.amplitude; // Use master amplitude for visualization scaling
     local_wave = g_synth_data.waveform;
     local_sampleRate = g_synth_data.sampleRate;
     ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_unlock, "on_draw_event unlock");
     // If unlock fails, we might draw with stale data, but proceed anyway for this non-critical task
     if(ret_lock == 0 && ret_unlock != 0) return FALSE;
 
 
     // Avoid division by zero if sample rate is invalid
     if (local_sampleRate <= 0) return FALSE;
 
     // --- Generate and draw waveform points ---
     num_samples_to_draw = width; // Draw one point per horizontal pixel
     sample_x_step = (double)width / num_samples_to_draw;
     cairo_move_to(cr, 0, height / 2.0); // Start drawing from middle-left
 
     // Calculate phase increment per drawing step
     double phase_increment = 2.0 * M_PI * local_freq / local_sampleRate;
 
     for (int i = 0; i < num_samples_to_draw; ++i) {
         float sample = 0.0f;
         double x_pos = i * sample_x_step;
         double y_pos;
 
         // Generate one sample of the selected waveform (using local amplitude)
         switch(local_wave) {
              case WAVE_SINE:     sample = (float)(local_amp * sin(current_phase)); break;
              case WAVE_SQUARE:   sample = (float)(local_amp * (sin(current_phase) >= 0.0 ? 1.0 : -1.0)); break;
              case WAVE_SAWTOOTH: sample = (float)(local_amp * (2.0 * (fmod(current_phase, 2.0 * M_PI) / (2.0 * M_PI)) - 1.0)); break;
              case WAVE_TRIANGLE: sample = (float)(local_amp * (2.0 / M_PI) * asin(sin(current_phase))); break;
         }
 
         // Update drawing phase
         current_phase += phase_increment;
         current_phase = fmod(current_phase, 2.0 * M_PI); // Wrap phase
         if (current_phase < 0.0) current_phase += 2.0 * M_PI;
 
         // Calculate Y position on the drawing area
         y_pos = (height / 2.0) - (sample * (height / 2.0) * 0.9); // Scale (-1..1 -> ~0..height), center vertically
         y_pos = fmax(0.0, fmin(height, y_pos)); // Clamp Y coordinate to drawing area bounds
         cairo_line_to(cr, x_pos, y_pos); // Draw line segment to the new point
     }
     cairo_stroke(cr); // Render the path drawn by cairo_line_to calls
     return FALSE; // Event handled
 }
 
 /**
  * @brief Callback function connected to the main window's "destroy" signal.
  *
  * This function is called when the main window is closed. Currently, it only
  * prints a confirmation message. Actual audio cleanup (stopping stream,
  * terminating PortAudio) is handled in main.c after the GTK main loop exits.
  *
  * @note This function could potentially trigger application shutdown or audio cleanup
  * if a different application lifecycle management strategy were used.
  */
 static void cleanup_on_destroy() {
     printf("GUI: Window destroyed signal received.\n");
     // main.c handles audio cleanup after gtk_main exits, nothing audio-related needed here.
 } 