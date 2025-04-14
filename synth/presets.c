/**
 * @file presets.c
 * @brief Implements preset saving, loading, and discovery functionality.
 *
 * Handles file I/O, parsing using a more robust line-by-line approach,
 * and directory scanning for presets.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <errno.h>
 #include <gtk/gtk.h>
 #include <dirent.h>
 #include <sys/stat.h>
 #include <ctype.h> 
 
 #include "synth_data.h" 
 #include "presets.h"    
 
 // --- External Global Shared Data Instance ---
 extern SharedSynthData g_synth_data;
 
 // --- Preset Directory ---
 #define PRESET_DIR "presets"
 #define PRESET_SUFFIX ".synthpreset"
 
 // --- Error Handling Macros ---
 #define CHECK_PTHREAD_ERR(ret, func_name) \
     if (ret != 0) { \
         fprintf(stderr, "PThread Error in %s (Presets): %s\n", func_name, strerror(ret)); \
     }
 
 
 // --- Helper Function ---
 
 /**
  * @brief Trims leading and trailing whitespace from a string in place.
  * @param str The string to trim. Modifies the string directly.
  */
 static void trim_whitespace(char *str) {
     if (!str) return;
     char *start = str;
     // Trim leading space
     while (isspace((unsigned char)*start)) start++;
 
     // Trim trailing space
     char *end = start + strlen(start) - 1;
     while (end > start && isspace((unsigned char)*end)) end--;
 
     // Null terminate trimmed string
     *(end + 1) = '\0';
 
     // Shift string if leading space was trimmed
     if (start != str) {
         memmove(str, start, strlen(start) + 1);
     }
 }
 
 
 /**
  * @brief Handles the process of saving a synthesizer preset to a file.
  * @param parent_window The parent GtkWindow for the file chooser dialog.
  * @note Saves parameters in "key: value\n" format.
  */
 void handle_save_preset(GtkWindow *parent_window) {
     GtkWidget *dialog;
     GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
     gint res;
     char *filename = NULL;
     FILE *fp = NULL;
     PresetData current_preset;
     int ret_lock, ret_unlock;
     int write_errors = 0;
 
     dialog = gtk_file_chooser_dialog_new("Save Preset", parent_window, action,
                                          "_Cancel", GTK_RESPONSE_CANCEL,
                                          "_Save", GTK_RESPONSE_ACCEPT, NULL);
     gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "my_preset.synthpreset");
     gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), PRESET_DIR); // Start in presets dir
 
     GtkFileFilter *filter = gtk_file_filter_new();
     gtk_file_filter_set_name(filter, "Synth Presets (*.synthpreset)");
     gtk_file_filter_add_pattern(filter, "*.synthpreset");
     gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
 
     res = gtk_dialog_run(GTK_DIALOG(dialog));
 
     if (res == GTK_RESPONSE_ACCEPT) {
         filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
         if (!filename) { gtk_widget_destroy(dialog); return; }
 
         ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_lock, "save preset lock");
         if (ret_lock == 0) {
             // Copy parameters to local struct
             current_preset.frequency1 = g_synth_data.frequency;
             current_preset.amplitude1 = g_synth_data.amplitude;
             current_preset.waveform1 = g_synth_data.waveform;
             current_preset.attackTime1 = g_synth_data.attackTime;
             current_preset.decayTime1 = g_synth_data.decayTime;
             current_preset.sustainLevel1 = g_synth_data.sustainLevel;
             current_preset.releaseTime1 = g_synth_data.releaseTime;
             current_preset.frequency2 = g_synth_data.frequency2;
             current_preset.amplitude2 = g_synth_data.amplitude2;
             current_preset.waveform2 = g_synth_data.waveform2;
             current_preset.attackTime2 = g_synth_data.attackTime2;
             current_preset.decayTime2 = g_synth_data.decayTime2;
             current_preset.sustainLevel2 = g_synth_data.sustainLevel2;
             current_preset.releaseTime2 = g_synth_data.releaseTime2;
             ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
             CHECK_PTHREAD_ERR(ret_unlock, "save preset unlock");
         } else { /* Handle lock error */
             g_free(filename); gtk_widget_destroy(dialog);
             GtkWidget *err_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Error locking mutex for saving.");
             gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog); return;
         }
 
         fp = fopen(filename, "w");
         if (fp == NULL) { /* Handle file open error */
             perror("Error opening file for writing");
             GtkWidget *err_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed to open file for writing:\n%s\n%s", filename, strerror(errno));
             gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog);
         } else {
             // Write parameters, checking fprintf return values
             if (fprintf(fp, "frequency1: %f\n", current_preset.frequency1) < 0) write_errors++;
             if (fprintf(fp, "amplitude1: %f\n", current_preset.amplitude1) < 0) write_errors++;
             if (fprintf(fp, "waveform1: %d\n", (int)current_preset.waveform1) < 0) write_errors++;
             if (fprintf(fp, "attackTime1: %f\n", current_preset.attackTime1) < 0) write_errors++;
             if (fprintf(fp, "decayTime1: %f\n", current_preset.decayTime1) < 0) write_errors++;
             if (fprintf(fp, "sustainLevel1: %f\n", current_preset.sustainLevel1) < 0) write_errors++;
             if (fprintf(fp, "releaseTime1: %f\n", current_preset.releaseTime1) < 0) write_errors++;
             if (fprintf(fp, "frequency2: %f\n", current_preset.frequency2) < 0) write_errors++;
             if (fprintf(fp, "amplitude2: %f\n", current_preset.amplitude2) < 0) write_errors++;
             if (fprintf(fp, "waveform2: %d\n", (int)current_preset.waveform2) < 0) write_errors++;
             if (fprintf(fp, "attackTime2: %f\n", current_preset.attackTime2) < 0) write_errors++;
             if (fprintf(fp, "decayTime2: %f\n", current_preset.decayTime2) < 0) write_errors++;
             if (fprintf(fp, "sustainLevel2: %f\n", current_preset.sustainLevel2) < 0) write_errors++;
             if (fprintf(fp, "releaseTime2: %f\n", current_preset.releaseTime2) < 0) write_errors++;
             fclose(fp);
 
             // Show feedback dialog 
             if (write_errors == 0) { /* Success Dialog */
                  GtkWidget *info_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Preset saved successfully:\n%s", filename);
                  gtk_dialog_run(GTK_DIALOG(info_dialog)); gtk_widget_destroy(info_dialog);
             } else { /* Warning Dialog */
                  GtkWidget *err_dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, "Preset saved, but some write errors occurred:\n%s", filename);
                  gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog);
             }
         }
         g_free(filename);
     }
     gtk_widget_destroy(dialog);
 }
 
 
 /**
  * @brief Handles the process of loading a synthesizer preset from a specific file path.
  *
  * Reads the synthesizer parameters from the given file path using a more robust
  * line-by-line parsing method, updates the global synthesizer data structure,
  * and returns success or failure.
  *
  * @param filepath The full path to the preset file to load.
  * @param parent_window_for_errors The parent GtkWindow for displaying potential error dialogs.
  * @return 1 on success (all parameters found and parsed), 0 on failure.
  * @note The caller is responsible for updating the GUI widgets after a successful load.
  */
 int handle_load_preset_from_file(const char *filepath, GtkWindow *parent_window_for_errors) {
     FILE *fp = NULL;
     PresetData loaded_preset;
     int ret_lock, ret_unlock;
     int parse_success = 1;
     char line_buffer[256];
     int fields_found_mask = 0;
     const int ALL_FIELDS_MASK = (1 << 14) - 1; // Mask for 14 fields
     int line_num = 0;
 
     if (!filepath) {
         fprintf(stderr, "Error: Null filepath passed to handle_load_preset_from_file\n");
         return 0;
     }
 
     fp = fopen(filepath, "r");
     if (fp == NULL) {
         perror("Error opening preset file for reading");
         parse_success = 0;
         GtkWidget *err_dialog = gtk_message_dialog_new(parent_window_for_errors, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed to open preset file for reading:\n%s\n%s", filepath, strerror(errno));
         gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog);
         return 0; // Return failure
     }
 
     // --- Robust Parsing Loop ---
     while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
         line_num++;
         char *line = line_buffer; // Pointer to work with for trimming
 
         // 1. Trim leading/trailing whitespace from the whole line
         trim_whitespace(line);
 
         // 2. Skip empty lines or comments (optional, e.g., lines starting with #)
         if (line[0] == '\0' || line[0] == '#') {
             continue;
         }
 
         // 3. Find the colon separator
         char *colon_ptr = strchr(line, ':');
         if (colon_ptr == NULL) {
             fprintf(stderr, "Warning: Invalid format (no colon) on line %d of %s: \"%s\"\n", line_num, filepath, line_buffer);
             // Decide whether to continue parsing or fail immediately
             // parse_success = 0; break;
             continue; 
         }
 
         // 4. Separate key and value strings
         *colon_ptr = '\0'; // Null-terminate the key string
         char *key_str = line;
         char *value_str = colon_ptr + 1;
 
         // 5. Trim whitespace from key and value individually
         trim_whitespace(key_str);
         trim_whitespace(value_str);
 
         // 6. Check if key or value is now empty
         if (key_str[0] == '\0' || value_str[0] == '\0') {
              fprintf(stderr, "Warning: Empty key or value on line %d of %s: \"%s\"\n", line_num, filepath, line_buffer);
              continue; // Skip line if key or value became empty after trim
         }
 
         // 7. Compare key and parse value
         double d_value;
         int i_value;
         int parsed_ok = 0;
 
         if (strcmp(key_str, "frequency1") == 0)      { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.frequency1 = d_value; fields_found_mask |= (1 << 0); parsed_ok=1;} }
         else if (strcmp(key_str, "amplitude1") == 0) { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.amplitude1 = d_value; fields_found_mask |= (1 << 1); parsed_ok=1;} }
         else if (strcmp(key_str, "waveform1") == 0)  { if(sscanf(value_str, "%d", &i_value)==1)  { loaded_preset.waveform1 = (WaveformType)i_value; fields_found_mask |= (1 << 2); parsed_ok=1;} }
         else if (strcmp(key_str, "attackTime1") == 0) { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.attackTime1 = d_value; fields_found_mask |= (1 << 3); parsed_ok=1;} }
         else if (strcmp(key_str, "decayTime1") == 0)  { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.decayTime1 = d_value; fields_found_mask |= (1 << 4); parsed_ok=1;} }
         else if (strcmp(key_str, "sustainLevel1") == 0){ if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.sustainLevel1 = d_value; fields_found_mask |= (1 << 5); parsed_ok=1;} }
         else if (strcmp(key_str, "releaseTime1") == 0) { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.releaseTime1 = d_value; fields_found_mask |= (1 << 6); parsed_ok=1;} }
         // Wave 2
         else if (strcmp(key_str, "frequency2") == 0)  { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.frequency2 = d_value; fields_found_mask |= (1 << 7); parsed_ok=1;} }
         else if (strcmp(key_str, "amplitude2") == 0)  { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.amplitude2 = d_value; fields_found_mask |= (1 << 8); parsed_ok=1;} }
         else if (strcmp(key_str, "waveform2") == 0)  { if(sscanf(value_str, "%d", &i_value)==1)  { loaded_preset.waveform2 = (WaveformType)i_value; fields_found_mask |= (1 << 9); parsed_ok=1;} }
         else if (strcmp(key_str, "attackTime2") == 0) { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.attackTime2 = d_value; fields_found_mask |= (1 << 10); parsed_ok=1;} }
         else if (strcmp(key_str, "decayTime2") == 0)  { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.decayTime2 = d_value; fields_found_mask |= (1 << 11); parsed_ok=1;} }
         else if (strcmp(key_str, "sustainLevel2") == 0){ if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.sustainLevel2 = d_value; fields_found_mask |= (1 << 12); parsed_ok=1;} }
         else if (strcmp(key_str, "releaseTime2") == 0) { if(sscanf(value_str, "%lf", &d_value)==1) { loaded_preset.releaseTime2 = d_value; fields_found_mask |= (1 << 13); parsed_ok=1;} }
         else {
              fprintf(stderr, "Warning: Unknown key '%s' on line %d of %s\n", key_str, line_num, filepath);
         }
 
         // If sscanf failed for a known key
         if (!parsed_ok && strcmp(key_str, "frequency1") != 0 /* Add checks for all known keys */ ) {
              fprintf(stderr, "Error: Failed to parse value for key '%s' on line %d of %s: value was '%s'\n", key_str, line_num, filepath, value_str);
              parse_success = 0; // Fail parsing
              break; // Stop processing file on parse error
         }
 
     } // end while(fgets...)
 
     fclose(fp);
 
     // Check if all fields were found AFTER reading the whole file (if not already failed)
     if (parse_success && fields_found_mask != ALL_FIELDS_MASK) {
         fprintf(stderr, "Error: Preset file format incomplete. Missing fields in %s (mask=0x%X)\n", filepath, fields_found_mask);
         parse_success = 0;
         GtkWidget *err_dialog = gtk_message_dialog_new(parent_window_for_errors, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Failed to load preset:\nIncomplete or invalid file format in\n%s", filepath);
         gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog);
     }
     // --- End Read & Parse ---
 
 
     // --- Update global state if successful ---
     if (parse_success) {
         ret_lock = pthread_mutex_lock(&g_synth_data.mutex);
         CHECK_PTHREAD_ERR(ret_lock, "load preset lock");
         if (ret_lock == 0) {
             // Update global synth data from loaded preset
             g_synth_data.frequency = loaded_preset.frequency1;
             g_synth_data.amplitude = loaded_preset.amplitude1;
             g_synth_data.waveform = loaded_preset.waveform1;
             g_synth_data.attackTime = loaded_preset.attackTime1;
             g_synth_data.decayTime = loaded_preset.decayTime1;
             g_synth_data.sustainLevel = loaded_preset.sustainLevel1;
             g_synth_data.releaseTime = loaded_preset.releaseTime1;
             g_synth_data.frequency2 = loaded_preset.frequency2;
             g_synth_data.amplitude2 = loaded_preset.amplitude2;
             g_synth_data.waveform2 = loaded_preset.waveform2;
             g_synth_data.attackTime2 = loaded_preset.attackTime2;
             g_synth_data.decayTime2 = loaded_preset.decayTime2;
             g_synth_data.sustainLevel2 = loaded_preset.sustainLevel2;
             g_synth_data.releaseTime2 = loaded_preset.releaseTime2;
 
             ret_unlock = pthread_mutex_unlock(&g_synth_data.mutex);
             CHECK_PTHREAD_ERR(ret_unlock, "load preset unlock");
          } else { /* Handle lock failure */
             GtkWidget *err_dialog = gtk_message_dialog_new(parent_window_for_errors, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Error locking mutex to apply loaded preset.");
             gtk_dialog_run(GTK_DIALOG(err_dialog)); gtk_widget_destroy(err_dialog);
             return 0; // Failure
          }
     }
     // --- End Update State ---
 
     return parse_success; // Return 1 if parsed and updated, 0 otherwise
 }
 
 
 /**
  * @brief Scans the presets directory and populates a GtkComboBoxText.
  * @param combo The GtkComboBoxText widget to populate.
  * @note Assumes PRESET_DIR exists relative to the current working directory.
  */
 void populate_preset_combo(GtkComboBoxText *combo) {
     DIR *dir;
     struct dirent *entry;
     struct stat entry_stat;
     char filepath[1024]; // Buffer for constructing full path for stat
 
     // Clear existing items (important for refresh)
     gtk_combo_box_text_remove_all(combo);
 
     // Add a default placeholder item
     gtk_combo_box_text_append_text(combo, "Select Preset...");
     gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0); // Make placeholder active
 
     dir = opendir(PRESET_DIR);
     if (dir == NULL) {
         perror("Could not open presets directory");
         gtk_combo_box_text_append_text(combo, "Error: Cannot open presets dir");
         return;
     }
 
     while ((entry = readdir(dir)) != NULL) {
         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
             continue;
         }
 
         // Construct full path to check if it's a file
         snprintf(filepath, sizeof(filepath), "%s/%s", PRESET_DIR, entry->d_name);
         if (stat(filepath, &entry_stat) == -1) {
             // Silently ignore files u can't stat? Or log?
             // perror("Stat failed for preset entry");
             continue; // Skip if stat fails
         }
 
         // Check if it's a regular file and ends with the preset suffix
         if (S_ISREG(entry_stat.st_mode)) {
             const char *suffix_ptr = strstr(entry->d_name, PRESET_SUFFIX);
             if (suffix_ptr != NULL && strcmp(suffix_ptr, PRESET_SUFFIX) == 0) {
                  // Add only the filename part to the combo box
                  gtk_combo_box_text_append_text(combo, entry->d_name);
             }
         }
     }
 
     closedir(dir);
 }