/**
 * @file gui.c
 * @brief Implements the GTK+ graphical user interface for the synthesizer.
 *
 * Creates the main window, layouts, sliders, buttons, and drawing area for
 * controlling two independent synthesizer waves.
 * Connects GTK signals (e.g., slider changes, button toggles) to callback
 * functions that update the shared synthesizer parameters safely using mutexes.
 * Includes logic for visualizing waveforms and handles preset loading via
 * a ComboBox, delegating save/load logic to functions defined in presets.c.
 * Frequency sliders use logarithmic mapping.
 */

 #include <gtk/gtk.h>
 #include <pthread.h>
 #include <math.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <string.h>
 #include <ctype.h>
 #include <float.h> 
 
 #include <glib/gstdio.h> 
 
 #include "gui.h"
 #include "synth_data.h"
 #include "presets.h" 

 // --- External Global Shared Data Instance ---
 extern SharedSynthData g_synth_data;
 
 // --- Constants ---
 #define PRESET_DIR "presets"
 #define FREQ_MIN 20.0
 #define FREQ_MAX 2000.0
 static const double LOG_FREQ_BASE_RATIO = FREQ_MAX / FREQ_MIN;
 
 // --- Static Global Widgets ---
 static GtkWidget *freq_value_label1 = NULL;
 static GtkWidget *freq_value_label2 = NULL;
 
 // --- Error Handling Macros ---
 #define CHECK_PTHREAD_ERR(ret, func_name) \
     if (ret != 0) { \
         fprintf(stderr, "PThread Error in %s (GUI): %s\n", func_name, strerror(ret)); \
     }
 
 #define CHECK_GTK_WIDGET(widget, name) \
     if (widget == NULL) { \
         fprintf(stderr, "Error: Failed to create GTK widget '%s'\n", name); \
         exit(EXIT_FAILURE); \
     }
 
 // --- Forward Declarations ---
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data);
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data);
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data);
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data);
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data);
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data);
 static void on_release_slider_changed(GtkRange *range, gpointer user_data);
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data);
 static void on_frequency_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_amplitude_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_waveform_combo_changed_wave2(GtkComboBox *widget, gpointer user_data);
 static void on_attack_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_decay_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_sustain_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_release_slider_changed_wave2(GtkRange *range, gpointer user_data);
 static void on_note_on_button_toggled_wave2(GtkToggleButton *button, gpointer user_data);
 static void on_save_preset_clicked(GtkButton *button, gpointer user_data);
 static void on_preset_combo_changed(GtkComboBox *widget, gpointer user_data);
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
 static void cleanup_on_destroy();
 static void update_gui_from_data();
 #ifndef TESTING
 static
 #endif
 double calculate_current_envelope(const SharedSynthData *data); 
 #ifndef TESTING
 static 
 #endif
 double calculate_current_envelope_wave2(const SharedSynthData *data);
 static inline double linear_to_log_freq(double linear_value);
 static inline double log_freq_to_linear(double freq);
 
 
 // --- Log Mapping Helpers ---
 static inline double linear_to_log_freq(double linear_value) {
     linear_value = fmax(0.0, fmin(1.0, linear_value));
     return FREQ_MIN * pow(LOG_FREQ_BASE_RATIO, linear_value);
 }
 
 static inline double log_freq_to_linear(double freq) {
     if (freq <= FREQ_MIN) return 0.0;
     if (freq >= FREQ_MAX) return 1.0;
     return log(freq / FREQ_MIN) / log(LOG_FREQ_BASE_RATIO);
 }
 
 
 // --- Public Function to Create GUI ---
 void create_gui(GtkApplication *app) {
     GtkWidget *window;
     GtkWidget *main_vbox;
     GtkWidget *controls_hbox1, *left_vbox1, *right_vbox1;
     GtkWidget *controls_hbox2, *left_vbox2, *right_vbox2;
     GtkWidget *separator;
     GtkWidget *drawing_area;
     GtkWidget *wave1_label, *freq_label1, *amp_label1, *wave_combo_label1, *adsr_label1;
     GtkWidget *freq_slider1, *amp_slider1, *waveform_combo1;
     GtkWidget *attack_slider1, *decay_slider1, *sustain_slider1, *release_slider1;
     GtkWidget *note_button1;
     GtkWidget *wave2_label, *freq_label2, *amp_label2, *wave_combo_label2, *adsr_label2;
     GtkWidget *freq_slider2, *amp_slider2, *waveform_combo2;
     GtkWidget *attack_slider2, *decay_slider2, *sustain_slider2, *release_slider2;
     GtkWidget *note_button2;
     GtkWidget *preset_hbox;
     GtkWidget *save_button;
     GtkWidget *preset_combo_label;
     GtkWidget *preset_combo;
     GtkWidget *freq_hbox1, *freq_hbox2;
 
     window = gtk_application_window_new(app);
     CHECK_GTK_WIDGET(window, "GtkApplicationWindow");
     gtk_window_set_title(GTK_WINDOW(window), "C Synth - Dual Wave");
     gtk_window_fullscreen(GTK_WINDOW(window));
     g_signal_connect(window, "destroy", G_CALLBACK(cleanup_on_destroy), NULL);
 
     main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
     CHECK_GTK_WIDGET(main_vbox, "main_vbox");
     gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 8);
     gtk_container_add(GTK_CONTAINER(window), main_vbox);
 
     // ==================== WAVE 1 CONTROLS ====================
     wave1_label = gtk_label_new(NULL); gtk_label_set_markup(GTK_LABEL(wave1_label), "<b>--- Wave 1 Controls ---</b>");
     gtk_box_pack_start(GTK_BOX(main_vbox), wave1_label, FALSE, FALSE, 2);
 
     controls_hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     gtk_box_pack_start(GTK_BOX(main_vbox), controls_hbox1, FALSE, FALSE, 3);
 
     // *** MODIFICATION: Pack VBoxes to expand/fill horizontally ***
     left_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
     gtk_box_pack_start(GTK_BOX(controls_hbox1), left_vbox1, TRUE, TRUE, 0); // Expand & Fill
     right_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
     gtk_box_pack_start(GTK_BOX(controls_hbox1), right_vbox1, TRUE, TRUE, 0); // Expand & Fill
 
     // --- Wave 1 Frequency ---
     freq_label1 = gtk_label_new("Frequency (Hz):"); gtk_box_pack_start(GTK_BOX(left_vbox1), freq_label1, FALSE, FALSE, 0);
     freq_hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
     freq_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
     gtk_widget_set_hexpand(freq_slider1, TRUE); // Slider expands horizontally
     gtk_range_set_value(GTK_RANGE(freq_slider1), log_freq_to_linear(g_synth_data.frequency));
     gtk_box_pack_start(GTK_BOX(freq_hbox1), freq_slider1, TRUE, TRUE, 0);
     freq_value_label1 = gtk_label_new(""); CHECK_GTK_WIDGET(freq_value_label1, "freq_value_label1");
     gtk_widget_set_size_request(freq_value_label1, 75, -1);
     gtk_label_set_xalign(GTK_LABEL(freq_value_label1), 0.0);
     gtk_box_pack_start(GTK_BOX(freq_hbox1), freq_value_label1, FALSE, FALSE, 0);
     gtk_box_pack_start(GTK_BOX(left_vbox1), freq_hbox1, FALSE, FALSE, 2); // HBox doesn't expand vertically
 
     // --- Wave 1 Amplitude ---
     amp_label1 = gtk_label_new("Amplitude:"); gtk_box_pack_start(GTK_BOX(left_vbox1), amp_label1, FALSE, FALSE, 0);
     amp_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); gtk_range_set_value(GTK_RANGE(amp_slider1), g_synth_data.amplitude); gtk_scale_set_draw_value(GTK_SCALE(amp_slider1), TRUE);
     gtk_widget_set_hexpand(amp_slider1, TRUE); // Slider expands horizontally
     gtk_box_pack_start(GTK_BOX(left_vbox1), amp_slider1, FALSE, FALSE, 2);
 
     // --- Wave 1 Waveform ---
     wave_combo_label1 = gtk_label_new("Waveform:"); gtk_box_pack_start(GTK_BOX(left_vbox1), wave_combo_label1, FALSE, FALSE, 0);
     const char *waveforms[] = {"Sine", "Square", "Sawtooth", "Triangle"}; waveform_combo1 = gtk_combo_box_text_new(); CHECK_GTK_WIDGET(waveform_combo1, "waveform_combo1"); for (int i = 0; i < G_N_ELEMENTS(waveforms); i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(waveform_combo1), waveforms[i]); gtk_combo_box_set_active(GTK_COMBO_BOX(waveform_combo1), g_synth_data.waveform);
     gtk_box_pack_start(GTK_BOX(left_vbox1), waveform_combo1, FALSE, FALSE, 2); // Combo box doesn't expand
 
     // --- Wave 1 ADSR ---
     adsr_label1 = gtk_label_new("ADSR Envelope (sec/level):"); gtk_box_pack_start(GTK_BOX(right_vbox1), adsr_label1, FALSE, FALSE, 0);
     attack_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); gtk_range_set_value(GTK_RANGE(attack_slider1), g_synth_data.attackTime); gtk_scale_set_draw_value(GTK_SCALE(attack_slider1), TRUE); gtk_widget_set_hexpand(attack_slider1, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox1), attack_slider1, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Attack"), FALSE, FALSE, 0);
     decay_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); gtk_range_set_value(GTK_RANGE(decay_slider1), g_synth_data.decayTime); gtk_scale_set_draw_value(GTK_SCALE(decay_slider1), TRUE); gtk_widget_set_hexpand(decay_slider1, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox1), decay_slider1, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Decay"), FALSE, FALSE, 0);
     sustain_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); gtk_range_set_value(GTK_RANGE(sustain_slider1), g_synth_data.sustainLevel); gtk_scale_set_draw_value(GTK_SCALE(sustain_slider1), TRUE); gtk_widget_set_hexpand(sustain_slider1, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox1), sustain_slider1, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Sustain"), FALSE, FALSE, 0);
     release_slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.01); gtk_range_set_value(GTK_RANGE(release_slider1), g_synth_data.releaseTime); gtk_scale_set_draw_value(GTK_SCALE(release_slider1), TRUE); gtk_widget_set_hexpand(release_slider1, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox1), release_slider1, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox1), gtk_label_new("Release"), FALSE, FALSE, 0);
 
     // --- Wave 1 Note Button ---
     note_button1 = gtk_toggle_button_new_with_label("Note On/Off (Wave 1)");
     gtk_box_pack_start(GTK_BOX(left_vbox1), note_button1, FALSE, FALSE, 3); // Doesn't expand
 
     separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
     gtk_box_pack_start(GTK_BOX(main_vbox), separator, FALSE, FALSE, 5);
 
     // ==================== WAVE 2 CONTROLS ====================
     wave2_label = gtk_label_new(NULL); gtk_label_set_markup(GTK_LABEL(wave2_label), "<b>--- Wave 2 Controls ---</b>");
     gtk_box_pack_start(GTK_BOX(main_vbox), wave2_label, FALSE, FALSE, 2);
 
     controls_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     gtk_box_pack_start(GTK_BOX(main_vbox), controls_hbox2, FALSE, FALSE, 3);
 
     left_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
     gtk_box_pack_start(GTK_BOX(controls_hbox2), left_vbox2, TRUE, TRUE, 0); // Expand & Fill
     right_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
     gtk_box_pack_start(GTK_BOX(controls_hbox2), right_vbox2, TRUE, TRUE, 0); // Expand & Fill
 
     freq_label2 = gtk_label_new("Frequency (Hz):"); gtk_box_pack_start(GTK_BOX(left_vbox2), freq_label2, FALSE, FALSE, 0);
     freq_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
     freq_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
     gtk_widget_set_hexpand(freq_slider2, TRUE);
     gtk_range_set_value(GTK_RANGE(freq_slider2), log_freq_to_linear(g_synth_data.frequency2));
     gtk_box_pack_start(GTK_BOX(freq_hbox2), freq_slider2, TRUE, TRUE, 0);
     freq_value_label2 = gtk_label_new(""); CHECK_GTK_WIDGET(freq_value_label2, "freq_value_label2");
     gtk_widget_set_size_request(freq_value_label2, 75, -1);
     gtk_label_set_xalign(GTK_LABEL(freq_value_label2), 0.0);
     gtk_box_pack_start(GTK_BOX(freq_hbox2), freq_value_label2, FALSE, FALSE, 0);
     gtk_box_pack_start(GTK_BOX(left_vbox2), freq_hbox2, FALSE, FALSE, 2);
 
     amp_label2 = gtk_label_new("Amplitude:"); gtk_box_pack_start(GTK_BOX(left_vbox2), amp_label2, FALSE, FALSE, 0);
     amp_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); gtk_range_set_value(GTK_RANGE(amp_slider2), g_synth_data.amplitude2); gtk_scale_set_draw_value(GTK_SCALE(amp_slider2), TRUE);
     gtk_widget_set_hexpand(amp_slider2, TRUE);
     gtk_box_pack_start(GTK_BOX(left_vbox2), amp_slider2, FALSE, FALSE, 2);
 
     wave_combo_label2 = gtk_label_new("Waveform:"); gtk_box_pack_start(GTK_BOX(left_vbox2), wave_combo_label2, FALSE, FALSE, 0);
     waveform_combo2 = gtk_combo_box_text_new(); CHECK_GTK_WIDGET(waveform_combo2, "waveform_combo2"); for (int i = 0; i < G_N_ELEMENTS(waveforms); i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(waveform_combo2), waveforms[i]); gtk_combo_box_set_active(GTK_COMBO_BOX(waveform_combo2), g_synth_data.waveform2);
     gtk_box_pack_start(GTK_BOX(left_vbox2), waveform_combo2, FALSE, FALSE, 2);
 
     adsr_label2 = gtk_label_new("ADSR Envelope (sec/level):"); gtk_box_pack_start(GTK_BOX(right_vbox2), adsr_label2, FALSE, FALSE, 0);
     attack_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); gtk_range_set_value(GTK_RANGE(attack_slider2), g_synth_data.attackTime2); gtk_scale_set_draw_value(GTK_SCALE(attack_slider2), TRUE); gtk_widget_set_hexpand(attack_slider2, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox2), attack_slider2, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Attack"), FALSE, FALSE, 0);
     decay_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01); gtk_range_set_value(GTK_RANGE(decay_slider2), g_synth_data.decayTime2); gtk_scale_set_draw_value(GTK_SCALE(decay_slider2), TRUE); gtk_widget_set_hexpand(decay_slider2, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox2), decay_slider2, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Decay"), FALSE, FALSE, 0);
     sustain_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01); gtk_range_set_value(GTK_RANGE(sustain_slider2), g_synth_data.sustainLevel2); gtk_scale_set_draw_value(GTK_SCALE(sustain_slider2), TRUE); gtk_widget_set_hexpand(sustain_slider2, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox2), sustain_slider2, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Sustain"), FALSE, FALSE, 0);
     release_slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.01); gtk_range_set_value(GTK_RANGE(release_slider2), g_synth_data.releaseTime2); gtk_scale_set_draw_value(GTK_SCALE(release_slider2), TRUE); gtk_widget_set_hexpand(release_slider2, TRUE); gtk_box_pack_start(GTK_BOX(right_vbox2), release_slider2, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(right_vbox2), gtk_label_new("Release"), FALSE, FALSE, 0);
 
     note_button2 = gtk_toggle_button_new_with_label("Note On/Off (Wave 2)");
     gtk_box_pack_start(GTK_BOX(left_vbox2), note_button2, FALSE, FALSE, 3);
 
     // --- Preset Controls ---
     preset_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
     CHECK_GTK_WIDGET(preset_hbox, "preset_hbox");
     gtk_box_pack_start(GTK_BOX(main_vbox), preset_hbox, FALSE, FALSE, 5);
     save_button = gtk_button_new_with_label("Save Preset As...");
     CHECK_GTK_WIDGET(save_button, "save_button");
     g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_preset_clicked), window);
     gtk_box_pack_start(GTK_BOX(preset_hbox), save_button, FALSE, FALSE, 5);
     preset_combo_label = gtk_label_new("Load Preset:");
     CHECK_GTK_WIDGET(preset_combo_label, "preset_combo_label");
     gtk_box_pack_start(GTK_BOX(preset_hbox), preset_combo_label, FALSE, FALSE, 5);
     preset_combo = gtk_combo_box_text_new();
     CHECK_GTK_WIDGET(preset_combo, "preset_combo");
     gtk_widget_set_hexpand(preset_combo, TRUE); // Combo box expands horizontally
     gtk_box_pack_start(GTK_BOX(preset_hbox), preset_combo, TRUE, TRUE, 5);
     populate_preset_combo(GTK_COMBO_BOX_TEXT(preset_combo));
     g_signal_connect(preset_combo, "changed", G_CALLBACK(on_preset_combo_changed), window);
 
 
     // --- Waveform Drawing Area ---
     drawing_area = gtk_drawing_area_new(); CHECK_GTK_WIDGET(drawing_area, "drawing_area");
     gtk_widget_set_size_request(drawing_area, -1, 300);
     // Pack to expand and fill vertically
     gtk_box_pack_start(GTK_BOX(main_vbox), drawing_area, TRUE, TRUE, 3);
     g_synth_data.waveform_drawing_area = drawing_area;
 
     // --- Assign Widget Pointers ---
     g_synth_data.freq_slider1_widget = GTK_RANGE(freq_slider1);
     g_synth_data.amp_slider1_widget = GTK_RANGE(amp_slider1);
     g_synth_data.waveform_combo1_widget = GTK_COMBO_BOX(waveform_combo1);
     g_synth_data.attack_slider1_widget = GTK_RANGE(attack_slider1);
     g_synth_data.decay_slider1_widget = GTK_RANGE(decay_slider1);
     g_synth_data.sustain_slider1_widget = GTK_RANGE(sustain_slider1);
     g_synth_data.release_slider1_widget = GTK_RANGE(release_slider1);
     g_synth_data.freq_slider2_widget = GTK_RANGE(freq_slider2);
     g_synth_data.amp_slider2_widget = GTK_RANGE(amp_slider2);
     g_synth_data.waveform_combo2_widget = GTK_COMBO_BOX(waveform_combo2);
     g_synth_data.attack_slider2_widget = GTK_RANGE(attack_slider2);
     g_synth_data.decay_slider2_widget = GTK_RANGE(decay_slider2);
     g_synth_data.sustain_slider2_widget = GTK_RANGE(sustain_slider2);
     g_synth_data.release_slider2_widget = GTK_RANGE(release_slider2);
 
     // --- Connect Signals ---
     g_signal_connect(freq_slider1, "value-changed", G_CALLBACK(on_frequency_slider_changed), NULL);
     g_signal_connect(amp_slider1, "value-changed", G_CALLBACK(on_amplitude_slider_changed), NULL);
     g_signal_connect(waveform_combo1, "changed", G_CALLBACK(on_waveform_combo_changed), NULL);
     g_signal_connect(attack_slider1, "value-changed", G_CALLBACK(on_attack_slider_changed), NULL);
     g_signal_connect(decay_slider1, "value-changed", G_CALLBACK(on_decay_slider_changed), NULL);
     g_signal_connect(sustain_slider1, "value-changed", G_CALLBACK(on_sustain_slider_changed), NULL);
     g_signal_connect(release_slider1, "value-changed", G_CALLBACK(on_release_slider_changed), NULL);
     g_signal_connect(note_button1, "toggled", G_CALLBACK(on_note_on_button_toggled), NULL);
     g_signal_connect(freq_slider2, "value-changed", G_CALLBACK(on_frequency_slider_changed_wave2), NULL);
     g_signal_connect(amp_slider2, "value-changed", G_CALLBACK(on_amplitude_slider_changed_wave2), NULL);
     g_signal_connect(waveform_combo2, "changed", G_CALLBACK(on_waveform_combo_changed_wave2), NULL);
     g_signal_connect(attack_slider2, "value-changed", G_CALLBACK(on_attack_slider_changed_wave2), NULL);
     g_signal_connect(decay_slider2, "value-changed", G_CALLBACK(on_decay_slider_changed_wave2), NULL);
     g_signal_connect(sustain_slider2, "value-changed", G_CALLBACK(on_sustain_slider_changed_wave2), NULL);
     g_signal_connect(release_slider2, "value-changed", G_CALLBACK(on_release_slider_changed_wave2), NULL);
     g_signal_connect(note_button2, "toggled", G_CALLBACK(on_note_on_button_toggled_wave2), NULL);
     g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), NULL);
 
     update_gui_from_data();
     gtk_widget_show_all(window);
 }
 
 // --- Static GTK Callback Implementations ---
 
 // ==================== WAVE 1 & 2 CALLBACKS ====================
 #ifndef TESTING
 static
 #endif
 double calculate_current_envelope(const SharedSynthData *data) {
     double env_multiplier = 0.0; EnvelopeStage currentStage = data->currentStage; double timeInStage = data->timeInStage; double attackTime = data->attackTime; double decayTime = data->decayTime; double sustainLevel = data->sustainLevel; double amplitude = data->amplitude;
     if (amplitude < DBL_EPSILON) return 0.0;
     switch(currentStage) { case ENV_ATTACK: if (attackTime <= 0.0) env_multiplier = amplitude; else env_multiplier = amplitude * fmin(1.0, (timeInStage / fmax(DBL_EPSILON, attackTime))); break; case ENV_DECAY: if (decayTime <= 0.0 || sustainLevel >= 1.0) env_multiplier = amplitude * sustainLevel; else { double decay_factor = fmin(1.0, timeInStage / fmax(DBL_EPSILON, decayTime)); env_multiplier = amplitude * (1.0 - (1.0 - sustainLevel) * decay_factor); } if (env_multiplier < amplitude * sustainLevel) env_multiplier = amplitude * sustainLevel; break; case ENV_SUSTAIN: env_multiplier = amplitude * sustainLevel; break; case ENV_RELEASE: case ENV_IDLE: default: env_multiplier = 0.0; break; }
     return fmax(0.0, fmin(amplitude, env_multiplier));
  }
  #ifndef TESTING
  static
  #endif
  double calculate_current_envelope_wave2(const SharedSynthData *data) {
    double env_multiplier = 0.0; EnvelopeStage currentStage = data->currentStage2; double timeInStage = data->timeInStage2; double attackTime = data->attackTime2; double decayTime = data->decayTime2; double sustainLevel = data->sustainLevel2; double amplitude = data->amplitude2;
    if (amplitude < DBL_EPSILON) return 0.0;
    switch(currentStage) { case ENV_ATTACK: if (attackTime <= 0.0) env_multiplier = amplitude; else env_multiplier = amplitude * fmin(1.0, (timeInStage / fmax(DBL_EPSILON, attackTime))); break; case ENV_DECAY: if (decayTime <= 0.0 || sustainLevel >= 1.0) env_multiplier = amplitude * sustainLevel; else { double decay_factor = fmin(1.0, timeInStage / fmax(DBL_EPSILON, decayTime)); env_multiplier = amplitude * (1.0 - (1.0 - sustainLevel) * decay_factor); } if (env_multiplier < amplitude * sustainLevel) env_multiplier = amplitude * sustainLevel; break; case ENV_SUSTAIN: env_multiplier = amplitude * sustainLevel; break; case ENV_RELEASE: case ENV_IDLE: default: env_multiplier = 0.0; break; }
    return fmax(0.0, fmin(amplitude, env_multiplier));
 }
 
 static void on_frequency_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     double linear_val = gtk_range_get_value(range);
     double freq = linear_to_log_freq(linear_val);
     gchar *freq_str = g_strdup_printf("%.1f Hz", freq);
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "freq1 lock");
     if (ret_lock == 0) {
         g_synth_data.frequency = freq;
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "freq1 unlock");
     }
 
     if (freq_value_label1) { gtk_label_set_text(GTK_LABEL(freq_value_label1), freq_str); }
     g_free(freq_str);
 
     if (g_synth_data.waveform_drawing_area) { gtk_widget_queue_draw(g_synth_data.waveform_drawing_area); }
 }
 
 static void on_frequency_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock;
     double linear_val = gtk_range_get_value(range);
     double freq = linear_to_log_freq(linear_val);
     gchar *freq_str = g_strdup_printf("%.1f Hz", freq);
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "freq2 lock");
     if (ret_lock == 0) {
         g_synth_data.frequency2 = freq;
         ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "freq2 unlock");
     }
 
     if (freq_value_label2) { gtk_label_set_text(GTK_LABEL(freq_value_label2), freq_str); }
     g_free(freq_str);
 
     if (g_synth_data.waveform_drawing_area) { gtk_widget_queue_draw(g_synth_data.waveform_drawing_area); }
 }
 static void on_amplitude_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "amp1 lock"); if (ret_lock == 0) { g_synth_data.amplitude = gtk_range_get_value(range); ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "amp1 unlock"); } if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 static void on_amplitude_slider_changed_wave2(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "amp2 lock"); if (ret_lock == 0) { g_synth_data.amplitude2 = gtk_range_get_value(range); ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "amp2 unlock"); } if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 static void on_waveform_combo_changed(GtkComboBox *widget, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "wave1 lock"); if (ret_lock == 0) { g_synth_data.waveform = (WaveformType)gtk_combo_box_get_active(widget); ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "wave1 unlock"); } if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 static void on_waveform_combo_changed_wave2(GtkComboBox *widget, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "wave2 lock"); if (ret_lock == 0) { g_synth_data.waveform2 = (WaveformType)gtk_combo_box_get_active(widget); ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "wave2 unlock"); } if (g_synth_data.waveform_drawing_area) gtk_widget_queue_draw(g_synth_data.waveform_drawing_area);
 }
 static void on_attack_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "attack1 lock"); if (ret_lock == 0) { g_synth_data.attackTime = gtk_range_get_value(range); if (g_synth_data.attackTime < 0) g_synth_data.attackTime = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "attack1 unlock"); }
 }
 static void on_decay_slider_changed(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "decay1 lock"); if (ret_lock == 0) { g_synth_data.decayTime = gtk_range_get_value(range); if (g_synth_data.decayTime < 0) g_synth_data.decayTime = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "decay1 unlock"); }
 }
 static void on_sustain_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "sustain1 lock"); if (ret_lock == 0) { g_synth_data.sustainLevel = gtk_range_get_value(range); if (g_synth_data.sustainLevel < 0.0) g_synth_data.sustainLevel = 0.0; if (g_synth_data.sustainLevel > 1.0) g_synth_data.sustainLevel = 1.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "sustain1 unlock"); }
 }
 static void on_release_slider_changed(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "release1 lock"); if (ret_lock == 0) { g_synth_data.releaseTime = gtk_range_get_value(range); if (g_synth_data.releaseTime < 0) g_synth_data.releaseTime = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "release1 unlock"); }
 }
 static void on_note_on_button_toggled(GtkToggleButton *button, gpointer user_data) {
     int ret_lock, ret_unlock; gboolean is_active = gtk_toggle_button_get_active(button); ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "note1 lock"); if (ret_lock == 0) { if (is_active && g_synth_data.currentStage == ENV_IDLE) { g_synth_data.note_active = 1; g_synth_data.currentStage = ENV_ATTACK; g_synth_data.timeInStage = 0.0; g_synth_data.phase = 0.0; g_synth_data.lastEnvValue = 0.0; printf("GUI: Note ON (Wave 1) -> ATTACK\n"); } else if (!is_active && g_synth_data.currentStage != ENV_IDLE && g_synth_data.currentStage != ENV_RELEASE) { g_synth_data.lastEnvValue = calculate_current_envelope(&g_synth_data); g_synth_data.currentStage = ENV_RELEASE; g_synth_data.timeInStage = 0.0; printf("GUI: Note OFF (Wave 1) -> RELEASE (from %.4f)\n", g_synth_data.lastEnvValue); } ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "note1 unlock"); }
 }
 static void on_attack_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "attack2 lock"); if (ret_lock == 0) { g_synth_data.attackTime2 = gtk_range_get_value(range); if (g_synth_data.attackTime2 < 0) g_synth_data.attackTime2 = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "attack2 unlock"); }
 }
 static void on_decay_slider_changed_wave2(GtkRange *range, gpointer user_data) {
      int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "decay2 lock"); if (ret_lock == 0) { g_synth_data.decayTime2 = gtk_range_get_value(range); if (g_synth_data.decayTime2 < 0) g_synth_data.decayTime2 = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "decay2 unlock"); }
 }
 static void on_sustain_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "sustain2 lock"); if (ret_lock == 0) { g_synth_data.sustainLevel2 = gtk_range_get_value(range); if (g_synth_data.sustainLevel2 < 0.0) g_synth_data.sustainLevel2 = 0.0; if (g_synth_data.sustainLevel2 > 1.0) g_synth_data.sustainLevel2 = 1.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "sustain2 unlock"); }
 }
 static void on_release_slider_changed_wave2(GtkRange *range, gpointer user_data) {
     int ret_lock, ret_unlock; ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "release2 lock"); if (ret_lock == 0) { g_synth_data.releaseTime2 = gtk_range_get_value(range); if (g_synth_data.releaseTime2 < 0) g_synth_data.releaseTime2 = 0.0; ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "release2 unlock"); }
 }
 static void on_note_on_button_toggled_wave2(GtkToggleButton *button, gpointer user_data) {
     int ret_lock, ret_unlock; gboolean is_active = gtk_toggle_button_get_active(button); ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "note2 lock"); if (ret_lock == 0) { if (is_active && g_synth_data.currentStage2 == ENV_IDLE) { g_synth_data.note_active2 = 1; g_synth_data.currentStage2 = ENV_ATTACK; g_synth_data.timeInStage2 = 0.0; g_synth_data.phase2 = 0.0; g_synth_data.lastEnvValue2 = 0.0; printf("GUI: Note ON (Wave 2) -> ATTACK\n"); } else if (!is_active && g_synth_data.currentStage2 != ENV_IDLE && g_synth_data.currentStage2 != ENV_RELEASE) { g_synth_data.lastEnvValue2 = calculate_current_envelope_wave2(&g_synth_data); g_synth_data.currentStage2 = ENV_RELEASE; g_synth_data.timeInStage2 = 0.0; printf("GUI: Note OFF (Wave 2) -> RELEASE (from %.4f)\n", g_synth_data.lastEnvValue2); } ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "note2 unlock"); }
 }
 
 
 // ==================== PRESET CALLBACKS ====================
 static void on_save_preset_clicked(GtkButton *button, gpointer user_data) {
     handle_save_preset(GTK_WINDOW(user_data));
 }
 
 static void on_preset_combo_changed(GtkComboBox *widget, gpointer user_data) {
     GtkWindow *parent_window = GTK_WINDOW(user_data);
     char *selected_preset_filename = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
     int success = 0;
 
     if (selected_preset_filename != NULL && strcmp(selected_preset_filename, "Select Preset...") != 0) {
         char *full_path = g_build_filename(PRESET_DIR, selected_preset_filename, NULL);
         if (full_path) {
              printf("GUI: Attempting to load preset: %s\n", full_path);
              success = handle_load_preset_from_file(full_path, parent_window);
              if (success) {
                  update_gui_from_data();
                  printf("GUI: Preset loaded and GUI updated.\n");
              } else {
                  printf("GUI: Preset loading failed.\n");
              }
              g_free(full_path);
         } else {
             fprintf(stderr, "Error building full path for preset '%s'.\n", selected_preset_filename);
              GtkWidget *err_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Internal error building path for preset.");
              gtk_dialog_run(GTK_DIALOG(err_dialog));
              gtk_widget_destroy(err_dialog);
         }
     } else {
         printf("GUI: Placeholder or NULL selected in preset combo.\n");
     }
     g_free(selected_preset_filename);
 }
 
 
 // ==================== GUI UPDATE HELPER ====================
 static void update_gui_from_data() {
     int ret_lock, ret_unlock;
     PresetData current_data_for_gui;
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_lock, "update_gui lock");
     if(ret_lock != 0) return;
 
     current_data_for_gui.frequency1 = g_synth_data.frequency; current_data_for_gui.amplitude1 = g_synth_data.amplitude; current_data_for_gui.waveform1 = g_synth_data.waveform; current_data_for_gui.attackTime1 = g_synth_data.attackTime; current_data_for_gui.decayTime1 = g_synth_data.decayTime; current_data_for_gui.sustainLevel1 = g_synth_data.sustainLevel; current_data_for_gui.releaseTime1 = g_synth_data.releaseTime;
     current_data_for_gui.frequency2 = g_synth_data.frequency2; current_data_for_gui.amplitude2 = g_synth_data.amplitude2; current_data_for_gui.waveform2 = g_synth_data.waveform2; current_data_for_gui.attackTime2 = g_synth_data.attackTime2; current_data_for_gui.decayTime2 = g_synth_data.decayTime2; current_data_for_gui.sustainLevel2 = g_synth_data.sustainLevel2; current_data_for_gui.releaseTime2 = g_synth_data.releaseTime2;
 
     ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
     CHECK_PTHREAD_ERR(ret_unlock, "update_gui unlock");
 
     if(g_synth_data.freq_slider1_widget) gtk_range_set_value(g_synth_data.freq_slider1_widget, log_freq_to_linear(current_data_for_gui.frequency1));
     if(g_synth_data.freq_slider2_widget) gtk_range_set_value(g_synth_data.freq_slider2_widget, log_freq_to_linear(current_data_for_gui.frequency2));
 
     gchar *freq_str1 = g_strdup_printf("%.1f Hz", current_data_for_gui.frequency1);
     gchar *freq_str2 = g_strdup_printf("%.1f Hz", current_data_for_gui.frequency2);
     if(freq_value_label1) gtk_label_set_text(GTK_LABEL(freq_value_label1), freq_str1);
     if(freq_value_label2) gtk_label_set_text(GTK_LABEL(freq_value_label2), freq_str2);
     g_free(freq_str1); g_free(freq_str2);
 
     if(g_synth_data.amp_slider1_widget) gtk_range_set_value(g_synth_data.amp_slider1_widget, current_data_for_gui.amplitude1);
     if(g_synth_data.waveform_combo1_widget) gtk_combo_box_set_active(g_synth_data.waveform_combo1_widget, (gint)current_data_for_gui.waveform1);
     if(g_synth_data.attack_slider1_widget) gtk_range_set_value(g_synth_data.attack_slider1_widget, current_data_for_gui.attackTime1);
     if(g_synth_data.decay_slider1_widget) gtk_range_set_value(g_synth_data.decay_slider1_widget, current_data_for_gui.decayTime1);
     if(g_synth_data.sustain_slider1_widget) gtk_range_set_value(g_synth_data.sustain_slider1_widget, current_data_for_gui.sustainLevel1);
     if(g_synth_data.release_slider1_widget) gtk_range_set_value(g_synth_data.release_slider1_widget, current_data_for_gui.releaseTime1);
     if(g_synth_data.amp_slider2_widget) gtk_range_set_value(g_synth_data.amp_slider2_widget, current_data_for_gui.amplitude2);
     if(g_synth_data.waveform_combo2_widget) gtk_combo_box_set_active(g_synth_data.waveform_combo2_widget, (gint)current_data_for_gui.waveform2);
     if(g_synth_data.attack_slider2_widget) gtk_range_set_value(g_synth_data.attack_slider2_widget, current_data_for_gui.attackTime2);
     if(g_synth_data.decay_slider2_widget) gtk_range_set_value(g_synth_data.decay_slider2_widget, current_data_for_gui.decayTime2);
     if(g_synth_data.sustain_slider2_widget) gtk_range_set_value(g_synth_data.sustain_slider2_widget, current_data_for_gui.sustainLevel2);
     if(g_synth_data.release_slider2_widget) gtk_range_set_value(g_synth_data.release_slider2_widget, current_data_for_gui.releaseTime2);
 
     if (g_synth_data.waveform_drawing_area) { gtk_widget_queue_draw(g_synth_data.waveform_drawing_area); }
 }
 
 
 // ==================== DRAWING CALLBACK ====================
 static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
     guint width, height;
     double line_width = 1.5;
     const double sample_x_step = 1.0;
     int num_samples_to_draw = 0;
     double local_freq1, local_amp1, local_freq2, local_amp2, local_sampleRate;
     WaveformType local_wave1, local_wave2;
     double phase1 = 0.0, phase2 = 0.0;
     double phase_increment1 = 0.0, phase_increment2 = 0.0;
     int ret_lock, ret_unlock;
     gboolean wave1_valid = FALSE, wave2_valid = FALSE;
 
     width = gtk_widget_get_allocated_width(widget);
     height = gtk_widget_get_allocated_height(widget);
     if (width <= 0 || height <= 0) return FALSE;
 
     num_samples_to_draw = width;
 
     cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); cairo_paint(cr);
 
     ret_lock = pthread_mutex_lock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_lock, "draw lock");
     if (ret_lock != 0) return FALSE;
     local_freq1 = g_synth_data.frequency; local_amp1 = g_synth_data.amplitude; local_wave1 = g_synth_data.waveform;
     local_freq2 = g_synth_data.frequency2; local_amp2 = g_synth_data.amplitude2; local_wave2 = g_synth_data.waveform2;
     local_sampleRate = g_synth_data.sampleRate;
     ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex); CHECK_PTHREAD_ERR(ret_unlock, "draw unlock");
 
     if (local_sampleRate <= 0) return FALSE;
 
     if (local_freq1 > DBL_EPSILON) {
         phase_increment1 = 2.0 * M_PI * local_freq1 / local_sampleRate;
         if (!isnan(phase_increment1) && !isinf(phase_increment1) && phase_increment1 > DBL_EPSILON) {
            wave1_valid = TRUE;
         }
     }
     if (local_freq2 > DBL_EPSILON) {
         phase_increment2 = 2.0 * M_PI * local_freq2 / local_sampleRate;
          if (!isnan(phase_increment2) && !isinf(phase_increment2) && phase_increment2 > DBL_EPSILON) {
            wave2_valid = TRUE;
         }
     }
 
     cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5); cairo_set_line_width(cr, 1.0); cairo_move_to(cr, 0, height / 2.0); cairo_line_to(cr, width, height / 2.0); cairo_stroke(cr);
 
     if (wave1_valid && local_amp1 > DBL_EPSILON) {
         cairo_new_path(cr);
         cairo_set_source_rgb(cr, 1.0, 0.2, 0.8); // Magenta
         cairo_set_line_width(cr, line_width);
         cairo_move_to(cr, 0, height / 2.0);
         for (int i = 0; i < num_samples_to_draw; ++i) {
             float sample = 0.0f; double x_pos = i * sample_x_step; double y_pos;
             switch(local_wave1) { case WAVE_SINE: sample = (float)(sin(phase1)); break; case WAVE_SQUARE: sample = (float)((sin(phase1) >= 0.0 ? 1.0 : -1.0)); break; case WAVE_SAWTOOTH: sample = (float)(2.0 * (fmod(phase1 / (2.0 * M_PI), 1.0)) - 1.0); break; case WAVE_TRIANGLE: sample = (float)((2.0 / M_PI) * asin(sin(phase1))); break; }
             sample *= local_amp1; phase1 += phase_increment1;
             y_pos = (height / 2.0) - (sample * (height / 2.0) * 0.9);
             y_pos = fmax(line_width / 2.0, fmin(height - line_width / 2.0, y_pos));
             cairo_line_to(cr, x_pos, y_pos);
         }
         cairo_stroke(cr);
     }
 
     if (wave2_valid && local_amp2 > DBL_EPSILON) {
         cairo_new_path(cr);
         phase2 = 0.0;
         cairo_set_source_rgb(cr, 0.2, 0.8, 1.0); // Cyan-Blue
         cairo_set_line_width(cr, line_width);
         cairo_move_to(cr, 0, height / 2.0);
         for (int i = 0; i < num_samples_to_draw; ++i) {
             float sample = 0.0f; double x_pos = i * sample_x_step; double y_pos;
             switch(local_wave2) { case WAVE_SINE: sample = (float)(sin(phase2)); break; case WAVE_SQUARE: sample = (float)((sin(phase2) >= 0.0 ? 1.0 : -1.0)); break; case WAVE_SAWTOOTH: sample = (float)(2.0 * (fmod(phase2 / (2.0 * M_PI), 1.0)) - 1.0); break; case WAVE_TRIANGLE: sample = (float)((2.0 / M_PI) * asin(sin(phase2))); break; }
             sample *= local_amp2; phase2 += phase_increment2;
             y_pos = (height / 2.0) - (sample * (height / 2.0) * 0.9);
             y_pos = fmax(line_width / 2.0, fmin(height - line_width / 2.0, y_pos));
             cairo_line_to(cr, x_pos, y_pos);
         }
         cairo_stroke(cr);
     }
 
     return FALSE;
 }
 
 
 // ==================== CLEANUP CALLBACK ====================
 static void cleanup_on_destroy() {
     printf("GUI: Window destroyed signal received.\n");
 }