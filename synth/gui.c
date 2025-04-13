/**
 * @file gui.c
 * @brief Implements the GTK+ graphical user interface for the synthesizer.
 *
 * Creates the main window, layouts, sliders, buttons, and drawing area for
 * controlling two independent synthesizer waves.
 * Connects GTK signals (e.g., slider changes, button toggles) to callback
 * functions that update the shared synthesizer parameters safely using mutexes.
 * Includes logic for visualizing both waveforms.
 */

 #include <gtk/gtk.h>
 #include <pthread.h>
 #include <math.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <string.h>
 
 #include "gui.h"        
 #include "synth_data.h" 
 
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
 
 // Wave 1 Callbacks
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data);
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data);
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data);
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data);
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data);
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data);
 static void on_release_slider_changed(GtkRange *range, gpointer user_data);
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data);
 
 // Wave 2 Callbacks
 static void on_frequency_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_amplitude_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_waveform_combo_changed_wave2(GtkComboBox *widget, gpointer user_data);
 static void on_attack_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_decay_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_sustain_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_release_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_note_on_button_toggled_wave2(GtkToggleButton *button, gpointer user_data);
 
 // Common Callbacks & Helpers
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
 static void cleanup_on_destroy();
 
 // Envelope Calculation Helpers
 #ifdef TESTING
 // Declared in gui.h for tests
 double calculate_current_envelope(const SharedSynthData *data);
 double calculate_current_envelope_wave2(const SharedSynthData *data);
 #else
 /** @brief Calculates current envelope value for Wave 1. */
 static double calculate_current_envelope(const SharedSynthData *data);
 /** @brief Calculates current envelope value for Wave 2. */
 static double calculate_current_envelope_wave2(const SharedSynthData *data);
 #endif
 
 
 // --- Public Function to Create GUI ---
 
 /**
  * @brief Creates and displays the main GUI window and its widgets for two waves.
  *
  * Initializes GTK widgets for both synthesizer waves (sliders, combo boxes, buttons, labels),
  * sets their initial values, packs them into layouts, connects widget signals to their
  * respective static callback functions (wave 1 and wave 2 specific), adds a drawing area,
  * and finally shows the main window.
  *
  * @param app The GtkApplication instance associated with this window.
  */
 void create_gui(GtkApplication *app) {
     GtkWidget *window;
     GtkWidget *main_vbox;
     GtkWidget *controls_hbox1, *left_vbox1, *right_vbox1; // Layout for Wave 1
     GtkWidget *controls_hbox2, *left_vbox2, *right_vbox2; // Layout for Wave 2
     GtkWidget *separator;
     GtkWidget *drawing_area;
 
     // --- Widgets for Wave 1 ---
     GtkWidget *wave1_label, *freq_label1, *amp_label1, *wave_combo_label1, *adsr_label1;
     GtkWidget *freq_slider1, *amp_slider1, *waveform_combo1;
     GtkWidget *attack_slider1, *decay_slider1, *sustain_slider1, *release_slider1;
     GtkWidget *note_button1;
 
     // --- Widgets for Wave 2 ---
     GtkWidget *wave2_label, *freq_label2, *amp_label2, *wave_combo_label2, *adsr_label2;
     GtkWidget *freq_slider2, *amp_slider2, *waveform_combo2;
     GtkWidget *attack_slider2, *decay_slider2, *sustain_slider2, *release_slider2;
     GtkWidget *note_button2;
 
 
     // --- Create Window ---
     window = gtk_application_window_new(app);
     CHECK_GTK_WIDGET(window, "GtkApplicationWindow");
     gtk_window_set_title(GTK_WINDOW(window), "C Synth - Dual Wave");
     gtk_window_set_default_size(GTK_WINDOW(window), 700, 650); // Increased size
     g_signal_connect(window, "destroy", G_CALLBACK(cleanup_on_destroy), NULL);
 
     // --- Create Main Layout Container ---
     main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); // Increased spacing
     CHECK_GTK_WIDGET(main_vbox, "main_vbox");
     gtk_container_add(GTK_CONTAINER(window), main_vbox);
 
     // ==================== WAVE 1 CONTROLS ====================
     wave1_label = gtk_label_new("--- Wave 1 Controls ---"); CHECK_GTK_WIDGET(wave1_label,"wave1_label");
     gtk_box_pack_start(GTK_BOX(main_vbox), wave1_label, FALSE, FALSE, 5);
 
     controls_hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     CHECK_GTK_WIDGET(controls_hbox1, "controls_hbox1");
     gtk_box_pack_start(GTK_BOX(main_vbox), controls_hbox1, FALSE, FALSE, 5); // Pack non-expand
 
     left_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(left_vbox1, "left_vbox1");
     gtk_box_pack_start(GTK_BOX(controls_hbox1), left_vbox1, TRUE, TRUE, 5);
 
     right_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(right_vbox1, "right_vbox1");
     gtk_box_pack_start(GTK_BOX(controls_hbox1), right_vbox1, TRUE, TRUE, 5);
 
     // Frequency Control (Wave 1)
     freq_label1 = gtk_label_new("Frequency (Hz):"); CHECK_GTK_WIDGET(freq_label1, "freq_label1");
     gtk_box_pack_start(GTK_BOX(left_vbox1), freq_label1, FALSE, FALSE, 2);
     freq_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20.0, 2000.0, 1.0); CHECK_GTK_WIDGET(freq_slider1, "freq_slider1");
     gtk_range_set_value(GTK_RANGE(freq_slider1), g_synth_data.frequency); // Set initial value
     gtk_scale_set_draw_value(GTK_SCALE(freq_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox1), freq_slider1, FALSE, FALSE, 5);
 
     // Amplitude Control (Wave 1)
     amp_label1 = gtk_label_new("Amplitude:"); CHECK_GTK_WIDGET(amp_label1, "amp_label1");
     gtk_box_pack_start(GTK_BOX(left_vbox1), amp_label1, FALSE, FALSE, 2);
     amp_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(amp_slider1, "amp_slider1");
     gtk_range_set_value(GTK_RANGE(amp_slider1), g_synth_data.amplitude);
     gtk_scale_set_draw_value(GTK_SCALE(amp_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox1), amp_slider1, FALSE, FALSE, 5);
 
     // Waveform Selection (Wave 1)
     wave_combo_label1 = gtk_label_new("Waveform:"); CHECK_GTK_WIDGET(wave_combo_label1, "wave_combo_label1");
     gtk_box_pack_start(GTK_BOX(left_vbox1), wave_combo_label1, FALSE, FALSE, 2);
     const char *waveforms[] = {"Sine", "Square", "Sawtooth", "Triangle"}; // Shared list
     waveform_combo1 = gtk_combo_box_text_new(); CHECK_GTK_WIDGET(waveform_combo1, "waveform_combo1");
     for (int i = 0; i < G_N_ELEMENTS(waveforms); i++) {
         gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(waveform_combo1), waveforms[i]);
     }
     gtk_combo_box_set_active(GTK_COMBO_BOX(waveform_combo1), g_synth_data.waveform); // Set initial selection
     gtk_box_pack_start(GTK_BOX(left_vbox1), waveform_combo1, FALSE, FALSE, 5);
 
     // ADSR Controls (Wave 1 - Right Side)
     adsr_label1 = gtk_label_new("ADSR Envelope (sec/level):"); CHECK_GTK_WIDGET(adsr_label1, "adsr_label1");
     gtk_box_pack_start(GTK_BOX(right_vbox1), adsr_label1, FALSE, FALSE, 2);
     // Attack Slider 1
     attack_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(attack_slider1, "attack_slider1");
     gtk_range_set_value(GTK_RANGE(attack_slider1), g_synth_data.attackTime); gtk_scale_set_draw_value(GTK_SCALE(attack_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox1), attack_slider1, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Attack"), FALSE, FALSE, 0);
     // Decay Slider 1
     decay_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(decay_slider1, "decay_slider1");
     gtk_range_set_value(GTK_RANGE(decay_slider1), g_synth_data.decayTime); gtk_scale_set_draw_value(GTK_SCALE(decay_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox1), decay_slider1, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Decay"), FALSE, FALSE, 0);
     // Sustain Slider 1
     sustain_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(sustain_slider1, "sustain_slider1");
     gtk_range_set_value(GTK_RANGE(sustain_slider1), g_synth_data.sustainLevel); gtk_scale_set_draw_value(GTK_SCALE(sustain_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox1), sustain_slider1, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Sustain"), FALSE, FALSE, 0);
     // Release Slider 1
     release_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.01); CHECK_GTK_WIDGET(release_slider1, "release_slider1");
     gtk_range_set_value(GTK_RANGE(release_slider1), g_synth_data.releaseTime); gtk_scale_set_draw_value(GTK_SCALE(release_slider1), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox1), release_slider1, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Release"), FALSE, FALSE, 0);
 
     // Note On/Off Button (Wave 1)
     note_button1 = gtk_toggle_button_new_with_label("Note On/Off (Wave 1)"); CHECK_GTK_WIDGET(note_button1, "note_button1");
     gtk_box_pack_start(GTK_BOX(left_vbox1), note_button1, FALSE, FALSE, 10);
 
 
     // --- Separator ---
     separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL); CHECK_GTK_WIDGET(separator, "separator");
     gtk_box_pack_start(GTK_BOX(main_vbox), separator, FALSE, FALSE, 10);
 
 
     // ==================== WAVE 2 CONTROLS ====================
     wave2_label = gtk_label_new("--- Wave 2 Controls ---"); CHECK_GTK_WIDGET(wave2_label,"wave2_label");
     gtk_box_pack_start(GTK_BOX(main_vbox), wave2_label, FALSE, FALSE, 5);
 
     controls_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     CHECK_GTK_WIDGET(controls_hbox2, "controls_hbox2");
     gtk_box_pack_start(GTK_BOX(main_vbox), controls_hbox2, FALSE, FALSE, 5); // Pack non-expand
 
     left_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(left_vbox2, "left_vbox2");
     gtk_box_pack_start(GTK_BOX(controls_hbox2), left_vbox2, TRUE, TRUE, 5);
 
     right_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     CHECK_GTK_WIDGET(right_vbox2, "right_vbox2");
     gtk_box_pack_start(GTK_BOX(controls_hbox2), right_vbox2, TRUE, TRUE, 5);
 
     // Frequency Control (Wave 2)
     freq_label2 = gtk_label_new("Frequency (Hz):"); CHECK_GTK_WIDGET(freq_label2, "freq_label2");
     gtk_box_pack_start(GTK_BOX(left_vbox2), freq_label2, FALSE, FALSE, 2);
     freq_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20.0, 2000.0, 1.0); CHECK_GTK_WIDGET(freq_slider2, "freq_slider2");
     gtk_range_set_value(GTK_RANGE(freq_slider2), g_synth_data.frequency2); // Use wave 2 initial value
     gtk_scale_set_draw_value(GTK_SCALE(freq_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox2), freq_slider2, FALSE, FALSE, 5);
 
     // Amplitude Control (Wave 2)
     amp_label2 = gtk_label_new("Amplitude:"); CHECK_GTK_WIDGET(amp_label2, "amp_label2");
     gtk_box_pack_start(GTK_BOX(left_vbox2), amp_label2, FALSE, FALSE, 2);
     amp_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(amp_slider2, "amp_slider2");
     gtk_range_set_value(GTK_RANGE(amp_slider2), g_synth_data.amplitude2);
     gtk_scale_set_draw_value(GTK_SCALE(amp_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox2), amp_slider2, FALSE, FALSE, 5);
 
     // Waveform Selection (Wave 2)
     wave_combo_label2 = gtk_label_new("Waveform:"); CHECK_GTK_WIDGET(wave_combo_label2, "wave_combo_label2");
     gtk_box_pack_start(GTK_BOX(left_vbox2), wave_combo_label2, FALSE, FALSE, 2);
     waveform_combo2 = gtk_combo_box_text_new(); CHECK_GTK_WIDGET(waveform_combo2, "waveform_combo2");
     for (int i = 0; i < G_N_ELEMENTS(waveforms); i++) {
         gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(waveform_combo2), waveforms[i]);
     }
     gtk_combo_box_set_active(GTK_COMBO_BOX(waveform_combo2), g_synth_data.waveform2); // Use wave 2 initial value
     gtk_box_pack_start(GTK_BOX(left_vbox2), waveform_combo2, FALSE, FALSE, 5);
 
     // ADSR Controls (Wave 2 - Right Side)
     adsr_label2 = gtk_label_new("ADSR Envelope (sec/level):"); CHECK_GTK_WIDGET(adsr_label2, "adsr_label2");
     gtk_box_pack_start(GTK_BOX(right_vbox2), adsr_label2, FALSE, FALSE, 2);
     // Attack Slider 2
     attack_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(attack_slider2, "attack_slider2");
     gtk_range_set_value(GTK_RANGE(attack_slider2), g_synth_data.attackTime2); gtk_scale_set_draw_value(GTK_SCALE(attack_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox2), attack_slider2, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Attack"), FALSE, FALSE, 0);
     // Decay Slider 2
     decay_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); CHECK_GTK_WIDGET(decay_slider2, "decay_slider2");
     gtk_range_set_value(GTK_RANGE(decay_slider2), g_synth_data.decayTime2); gtk_scale_set_draw_value(GTK_SCALE(decay_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox2), decay_slider2, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Decay"), FALSE, FALSE, 0);
     // Sustain Slider 2
     sustain_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); CHECK_GTK_WIDGET(sustain_slider2, "sustain_slider2");
     gtk_range_set_value(GTK_RANGE(sustain_slider2), g_synth_data.sustainLevel2); gtk_scale_set_draw_value(GTK_SCALE(sustain_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox2), sustain_slider2, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Sustain"), FALSE, FALSE, 0);
     // Release Slider 2
     release_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.01); CHECK_GTK_WIDGET(release_slider2, "release_slider2");
     gtk_range_set_value(GTK_RANGE(release_slider2), g_synth_data.releaseTime2); gtk_scale_set_draw_value(GTK_SCALE(release_slider2), TRUE);
     gtk_box_pack_start(GTK_BOX(right_vbox2), release_slider2, FALSE, FALSE, 2); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Release"), FALSE, FALSE, 0);
 
     // Note On/Off Button (Wave 2)
     note_button2 = gtk_toggle_button_new_with_label("Note On/Off (Wave 2)"); CHECK_GTK_WIDGET(note_button2, "note_button2");
     gtk_box_pack_start(GTK_BOX(left_vbox2), note_button2, FALSE, FALSE, 10);
 
 
     // --- Waveform Drawing Area ---
     drawing_area = gtk_drawing_area_new(); CHECK_GTK_WIDGET(drawing_area, "drawing_area");
     gtk_widget_set_size_request(drawing_area, 400, 150); // Request initial size
     gtk_box_pack_start(GTK_BOX(main_vbox), drawing_area, TRUE, TRUE, 5); // Pack at bottom, allow expansion
     g_synth_data.waveform_drawing_area = drawing_area; // Store pointer
 
 
     // --- Connect Signals to Callbacks ---
     // Wave 1
     g_signal_connect(freq_slider1, "value-changed", G_CALLBACK(on_frequency_slider_changed), NULL);
     g_signal_connect(amp_slider1, "value-changed", G_CALLBACK(on_amplitude_slider_changed), NULL);
     g_signal_connect(waveform_combo1, "changed", G_CALLBACK(on_waveform_combo_changed), NULL);
     g_signal_connect(attack_slider1, "value-changed", G_CALLBACK(on_attack_slider_changed), NULL);
     g_signal_connect(decay_slider1, "value-changed", G_CALLBACK(on_decay_slider_changed), NULL);
     g_signal_connect(sustain_slider1, "value-changed", G_CALLBACK(on_sustain_slider_changed), NULL);
     g_signal_connect(release_slider1, "value-changed", G_CALLBACK(on_release_slider_changed), NULL);
     g_signal_connect(note_button1, "toggled", G_CALLBACK(on_note_on_button_toggled), NULL);
 
     // Wave 2
     g_signal_connect(freq_slider2, "value-changed", G_CALLBACK(on_frequency_slider_changed_wave2), NULL);
     g_signal_connect(amp_slider2, "value-changed", G_CALLBACK(on_amplitude_slider_changed_wave2), NULL);
     g_signal_connect(waveform_combo2, "changed", G_CALLBACK(on_waveform_combo_changed_wave2), NULL);
     g_signal_connect(attack_slider2, "value-changed", G_CALLBACK(on_attack_slider_changed_wave2), NULL);
     g_signal_connect(decay_slider2, "value-changed", G_CALLBACK(on_decay_slider_changed_wave2), NULL);
     g_signal_connect(sustain_slider2, "value-changed", G_CALLBACK(on_sustain_slider_changed_wave2), NULL);
     g_signal_connect(release_slider2, "value-changed", G_CALLBACK(on_release_slider_changed_wave2), NULL);
     g_signal_connect(note_button2, "toggled", G_CALLBACK(on_note_on_button_toggled_wave2), NULL);
 
     // Drawing Area
     g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), NULL);
 
 
     // --- Show Window and Widgets ---
     gtk_widget_show_all(window); // Show the window and all contained widgets
 }
 
 // --- Static GTK Callback Implementations ---
 
 // ==================== WAVE 1 CALLBACKS ====================
 
 /**
  * @brief Calculates the current envelope value for Wave 1 based on its ADSR state.
  * Used by the note-off callback to capture the value before transitioning to RELEASE.
  * @param[in] data Pointer to the shared synthesizer data structure.
  * @return The calculated envelope multiplier for Wave 1.
  */
 #ifdef TESTING
 double calculate_current_envelope(const SharedSynthData *data) {
 #else
 static double calculate_current_envelope(const SharedSynthData *data) {
 #endif
     double env_multiplier = 0.0;
     EnvelopeStage currentStage = data->currentStage; // Wave 1 stage
     double timeInStage = data->timeInStage;
     double attackTime = data->attackTime;
     double decayTime = data->decayTime;
     double sustainLevel = data->sustainLevel;
     double amplitude = data->amplitude;
 
     switch(currentStage) {
         case ENV_ATTACK:
             if (attackTime <= 0.0) env_multiplier = amplitude;
             else env_multiplier = amplitude * fmin(1.0, (timeInStage / attackTime));
             break;
         case ENV_DECAY:
              if (decayTime <= 0.0 || sustainLevel >= 1.0) env_multiplier = amplitude * sustainLevel;
              else {
                 double decay_factor = fmin(1.0, timeInStage / decayTime);
                 env_multiplier = amplitude * (1.0 - (1.0 - sustainLevel) * decay_factor);
              }
              if (env_multiplier < amplitude * sustainLevel) env_multiplier = amplitude * sustainLevel;
             break;
         case ENV_SUSTAIN:
             env_multiplier = amplitude * sustainLevel;
             break;
         case ENV_RELEASE: case ENV_IDLE: default: env_multiplier = 0.0; break;
     }
     return fmax(0.0, fmin(amplitude, env_multiplier));
 }
 
 /**
  * @brief Callback for Wave 1 frequency slider. Updates `g_synth_data.frequency`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "freq1 lock");
     if (ret_lock == 0) {
         g_synth_data.frequency = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "freq1 unlock");
     }
     if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 1 amplitude slider. Updates `g_synth_data.amplitude`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
      ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "amp1 lock");
      if (ret_lock == 0) {
         g_synth_data.amplitude = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "amp1 unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 1 waveform combo box. Updates `g_synth_data.waveform`.
  * @param widget The GtkComboBox widget.
  * @param user_data Unused user data.
  */
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "wave1 lock");
      if (ret_lock == 0) {
         g_synth_data.waveform = (WaveformType)gtk_combo_box_get_active(widget);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "wave1 unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 1 attack slider. Updates `g_synth_data.attackTime`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "attack1 lock");
      if (ret_lock == 0) {
         g_synth_data.attackTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "attack1 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 1 decay slider. Updates `g_synth_data.decayTime`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
      ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "decay1 lock");
      if (ret_lock == 0) {
         g_synth_data.decayTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "decay1 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 1 sustain slider. Updates `g_synth_data.sustainLevel`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "sustain1 lock");
      if (ret_lock == 0) {
         g_synth_data.sustainLevel = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "sustain1 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 1 release slider. Updates `g_synth_data.releaseTime`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_release_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "release1 lock");
      if (ret_lock == 0) {
         g_synth_data.releaseTime = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "release1 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 1 Note On/Off button. Manages ADSR state for Wave 1.
  * @param button The GtkToggleButton widget.
  * @param user_data Unused user data.
  */
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data) {
     int ret_lock, ret_unlock;
     gboolean is_active = gtk_toggle_button_get_active(button);
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "note1 lock");
      if (ret_lock == 0) {
          // Note On (Wave 1)
          if (is_active && g_synth_data.currentStage == ENV_IDLE) {
              g_synth_data.note_active = 1;
              g_synth_data.currentStage = ENV_ATTACK;
              g_synth_data.timeInStage = 0.0;
              g_synth_data.phase = 0.0; // Reset phase
              g_synth_data.lastEnvValue = 0.0;
              printf("GUI: Note ON (Wave 1) -> ATTACK\n");
          }
          // Note Off (Wave 1)
          else if (!is_active && (g_synth_data.currentStage == ENV_ATTACK ||
                                  g_synth_data.currentStage == ENV_DECAY ||
                                  g_synth_data.currentStage == ENV_SUSTAIN)) {
              g_synth_data.lastEnvValue = calculate_current_envelope(&g_synth_data); // Calculate for wave 1
              g_synth_data.currentStage = ENV_RELEASE;
              g_synth_data.timeInStage = 0.0;
              printf("GUI: Note OFF (Wave 1) -> RELEASE (from %.4f)\n", g_synth_data.lastEnvValue);
          }
          ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "note1 unlock");
      }
 }
 
 // ==================== WAVE 2 CALLBACKS ====================
 
 /**
  * @brief Calculates the current envelope value for Wave 2 based on its ADSR state.
  * Used by the note-off callback for wave 2.
  * @param[in] data Pointer to the shared synthesizer data structure.
  * @return The calculated envelope multiplier for Wave 2.
  */
 #ifdef TESTING
 double calculate_current_envelope_wave2(const SharedSynthData *data) {
 #else
 static double calculate_current_envelope_wave2(const SharedSynthData *data) {
 #endif
     double env_multiplier = 0.0;
     EnvelopeStage currentStage = data->currentStage2; // Wave 2 stage
     double timeInStage = data->timeInStage2;
     double attackTime = data->attackTime2;
     double decayTime = data->decayTime2;
     double sustainLevel = data->sustainLevel2;
     double amplitude = data->amplitude2;
 
     switch(currentStage) {
         case ENV_ATTACK:
             if (attackTime <= 0.0) env_multiplier = amplitude;
             else env_multiplier = amplitude * fmin(1.0, (timeInStage / attackTime));
             break;
         case ENV_DECAY:
              if (decayTime <= 0.0 || sustainLevel >= 1.0) env_multiplier = amplitude * sustainLevel;
              else {
                 double decay_factor = fmin(1.0, timeInStage / decayTime);
                 env_multiplier = amplitude * (1.0 - (1.0 - sustainLevel) * decay_factor);
              }
              if (env_multiplier < amplitude * sustainLevel) env_multiplier = amplitude * sustainLevel;
             break;
         case ENV_SUSTAIN:
             env_multiplier = amplitude * sustainLevel;
             break;
         case ENV_RELEASE: case ENV_IDLE: default: env_multiplier = 0.0; break;
     }
     return fmax(0.0, fmin(amplitude, env_multiplier));
 }
 
 
 /**
  * @brief Callback for Wave 2 frequency slider. Updates `g_synth_data.frequency2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_frequency_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "freq2 lock");
     if (ret_lock == 0) {
         g_synth_data.frequency2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "freq2 unlock");
     }
     if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 2 amplitude slider. Updates `g_synth_data.amplitude2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_amplitude_slider_changed_wave2(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
      ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "amp2 lock");
      if (ret_lock == 0) {
         g_synth_data.amplitude2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "amp2 unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 2 waveform combo box. Updates `g_synth_data.waveform2`.
  * @param widget The GtkComboBox widget.
  * @param user_data Unused user data.
  */
 static void on_waveform_combo_changed_wave2(GtkComboBox *widget, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "wave2 lock");
      if (ret_lock == 0) {
         g_synth_data.waveform2 = (WaveformType)gtk_combo_box_get_active(widget);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "wave2 unlock");
     }
      if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 
 /**
  * @brief Callback for Wave 2 attack slider. Updates `g_synth_data.attackTime2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_attack_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "attack2 lock");
      if (ret_lock == 0) {
         g_synth_data.attackTime2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "attack2 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 2 decay slider. Updates `g_synth_data.decayTime2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_decay_slider_changed_wave2(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock;
      ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "decay2 lock");
      if (ret_lock == 0) {
         g_synth_data.decayTime2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "decay2 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 2 sustain slider. Updates `g_synth_data.sustainLevel2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_sustain_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "sustain2 lock");
      if (ret_lock == 0) {
         g_synth_data.sustainLevel2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "sustain2 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 2 release slider. Updates `g_synth_data.releaseTime2`.
  * @param range The GtkRange widget.
  * @param user_data Unused user data.
  */
 static void on_release_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "release2 lock");
      if (ret_lock == 0) {
         g_synth_data.releaseTime2 = gtk_range_get_value(range);
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "release2 unlock");
     }
 }
 
 /**
  * @brief Callback for Wave 2 Note On/Off button. Manages ADSR state for Wave 2.
  * @param button The GtkToggleButton widget.
  * @param user_data Unused user data.
  */
 static void on_note_on_button_toggled_wave2(GtkToggleButton *button, gpointer user_data) {
     int ret_lock, ret_unlock;
     gboolean is_active = gtk_toggle_button_get_active(button);
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "note2 lock");
      if (ret_lock == 0) {
          // Note On (Wave 2)
          if (is_active && g_synth_data.currentStage2 == ENV_IDLE) {
              g_synth_data.note_active2 = 1;
              g_synth_data.currentStage2 = ENV_ATTACK;
              g_synth_data.timeInStage2 = 0.0;
              g_synth_data.phase2 = 0.0; // Reset phase
              g_synth_data.lastEnvValue2 = 0.0;
              printf("GUI: Note ON (Wave 2) -> ATTACK\n");
          }
          // Note Off (Wave 2)
          else if (!is_active && (g_synth_data.currentStage2 == ENV_ATTACK ||
                                  g_synth_data.currentStage2 == ENV_DECAY ||
                                  g_synth_data.currentStage2 == ENV_SUSTAIN)) {
              g_synth_data.lastEnvValue2 = calculate_current_envelope_wave2(&g_synth_data); // Calculate for wave 2
              g_synth_data.currentStage2 = ENV_RELEASE;
              g_synth_data.timeInStage2 = 0.0;
              printf("GUI: Note OFF (Wave 2) -> RELEASE (from %.4f)\n", g_synth_data.lastEnvValue2);
          }
          ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "note2 unlock");
      }
 }
 
 
 // ==================== DRAWING CALLBACK ====================
 
 /**
  * @brief Callback for the drawing area's "draw" signal. Renders visualizations
  * of both Wave 1 and Wave 2 using Cairo graphics.
  * Reads necessary parameters safely from `g_synth_data` using a mutex.
  * Draws Wave 1 in blue and Wave 2 in green.
  * @param widget The GtkDrawingArea widget.
  * @param cr The Cairo drawing context.
  * @param user_data Unused user data.
  * @return FALSE to indicate the event was handled.
  */
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
     guint width, height;
     double line_width = 1.5;
     int num_samples_to_draw;
     double sample_x_step;
     // Local copies
     double local_freq1, local_amp1, local_freq2, local_amp2, local_sampleRate;
     WaveformType local_wave1, local_wave2;
     double phase1 = 0.0, phase2 = 0.0; // Independent phases for drawing
     int ret_lock, ret_unlock;
 
     width = gtk_widget_get_allocated_width(widget);
     height = gtk_widget_get_allocated_height(widget);
     if (width <= 0 || height <= 0) return FALSE;
 
     // Set background
     cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); // Dark background
     cairo_paint(cr);
 
     // --- Safely read parameters needed for drawing ---
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "draw lock");
     if (ret_lock != 0) return FALSE;
     // Wave 1
     local_freq1 = g_synth_data.frequency;
     local_amp1 = g_synth_data.amplitude;
     local_wave1 = g_synth_data.waveform;
     // Wave 2
     local_freq2 = g_synth_data.frequency2;
     local_amp2 = g_synth_data.amplitude2;
     local_wave2 = g_synth_data.waveform2;
     // Common
     local_sampleRate = g_synth_data.sampleRate;
     ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "draw unlock");
     if (ret_lock != 0 && ret_unlock != 0) return FALSE; // Exit if lock/unlock failed significantly
 
     if (local_sampleRate <= 0) return FALSE;
 
     // --- Generate and draw waveform points ---
     num_samples_to_draw = width;
     sample_x_step = (double)width / num_samples_to_draw;
 
     // --- Draw Wave 1 (Blue) ---
     cairo_set_source_rgb(cr, 0.3, 0.7, 1.0); // Blue
     cairo_set_line_width(cr, line_width);
     cairo_move_to(cr, 0, height / 2.0);
     double phase_increment1 = 2.0 * M_PI * local_freq1 / local_sampleRate;
 
     for (int i = 0; i < num_samples_to_draw; ++i) {
         float sample = 0.0f;
         double x_pos = i * sample_x_step;
         double y_pos;
         switch(local_wave1) { // Use Wave 1 params
              case WAVE_SINE:     sample = (float)(local_amp1 * sin(phase1)); break;
              case WAVE_SQUARE:   sample = (float)(local_amp1 * (sin(phase1) >= 0.0 ? 1.0 : -1.0)); break;
              case WAVE_SAWTOOTH: sample = (float)(local_amp1 * (2.0 * (fmod(phase1, 2.0 * M_PI) / (2.0 * M_PI)) - 1.0)); break;
              case WAVE_TRIANGLE: sample = (float)(local_amp1 * (2.0 / M_PI) * asin(sin(phase1))); break;
         }
         phase1 = fmod(phase1 + phase_increment1, 2.0 * M_PI); // Update phase 1
         if (phase1 < 0.0) phase1 += 2.0 * M_PI;
         y_pos = (height / 2.0) - (sample * (height / 2.0) * 0.9);
         y_pos = fmax(line_width/2.0, fmin(height - line_width/2.0, y_pos)); // Clamp Y
         cairo_line_to(cr, x_pos, y_pos);
     }
     cairo_stroke(cr); // Render Wave 1 path
 
     // --- Draw Wave 2 (Green) ---
     cairo_set_source_rgb(cr, 0.3, 1.0, 0.7); // Green
     cairo_set_line_width(cr, line_width);
     cairo_move_to(cr, 0, height / 2.0);
     double phase_increment2 = 2.0 * M_PI * local_freq2 / local_sampleRate;
 
     for (int i = 0; i < num_samples_to_draw; ++i) {
         float sample = 0.0f;
         double x_pos = i * sample_x_step;
         double y_pos;
         switch(local_wave2) { // Use Wave 2 params
              case WAVE_SINE:     sample = (float)(local_amp2 * sin(phase2)); break;
              case WAVE_SQUARE:   sample = (float)(local_amp2 * (sin(phase2) >= 0.0 ? 1.0 : -1.0)); break;
              case WAVE_SAWTOOTH: sample = (float)(local_amp2 * (2.0 * (fmod(phase2, 2.0 * M_PI) / (2.0 * M_PI)) - 1.0)); break;
              case WAVE_TRIANGLE: sample = (float)(local_amp2 * (2.0 / M_PI) * asin(sin(phase2))); break;
         }
         phase2 = fmod(phase2 + phase_increment2, 2.0 * M_PI); // Update phase 2
         if (phase2 < 0.0) phase2 += 2.0 * M_PI;
         y_pos = (height / 2.0) - (sample * (height / 2.0) * 0.9);
         y_pos = fmax(line_width/2.0, fmin(height - line_width/2.0, y_pos)); // Clamp Y
         cairo_line_to(cr, x_pos, y_pos);
     }
     cairo_stroke(cr); // Render Wave 2 path
 
     return FALSE; // Event handled
 }
 
 // ==================== CLEANUP CALLBACK ====================
 
 /**
  * @brief Callback function connected to the main window's "destroy" signal.
  * Prints a confirmation message. Actual cleanup is handled in main.c.
  */
 static void cleanup_on_destroy() {
     printf("GUI: Window destroyed signal received.\n");
     // main.c handles audio/mutex cleanup after gtk_main exits.
 }