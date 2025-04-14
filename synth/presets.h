/**
 * @file presets.h
 * @brief Header file for preset saving and loading functionality.
 *
 * Declares functions for saving and loading synthesizer presets,
 * and populating GUI elements with available presets.
 */

 #ifndef PRESETS_H
 #define PRESETS_H
 
 #include <gtk/gtk.h> 
 #include "synth_data.h"
 
 /**
  * @brief Handles the process of saving a synthesizer preset to a file.
  *
  * This function displays a file chooser dialog to allow the user to select
  * a location and filename for the preset file, then writes the current
  * synthesizer parameters to the file.
  *
  * @param parent_window The parent GtkWindow for the file chooser dialog.
  */
 void handle_save_preset(GtkWindow *parent_window);
 
 /**
  * @brief Handles the process of loading a synthesizer preset from a specific file path.
  *
  * Reads the synthesizer parameters from the given file path
  * and updates the global synthesizer data structure.
  *
  * @param filepath The full path to the preset file to load.
  * @param parent_window_for_errors The parent GtkWindow for displaying potential error dialogs.
  * @return 1 on success, 0 on failure. The caller is responsible for updating the GUI.
  */
 int handle_load_preset_from_file(const char *filepath, GtkWindow *parent_window_for_errors);
 
 /**
  * @brief Scans the presets directory and populates a GtkComboBoxText with found preset files.
  *
  * @param combo The GtkComboBoxText widget to populate.
  */
 void populate_preset_combo(GtkComboBoxText *combo);
 
 
 #endif // PRESETS_H