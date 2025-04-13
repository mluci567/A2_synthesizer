/**
 * @file gui.h
 * @brief Public interface for the GTK+ GUI module.
 *
 * Declares the function responsible for creating and setting up the main
 * graphical user interface for the synthesizer application.
 */

 #ifndef GUI_H
 #define GUI_H
 
 #include <gtk/gtk.h>      // For GtkApplication and other GTK types
 #include "synth_data.h" // For SharedSynthData type
 
 // --- Public GUI Function ---
 
 /**
  * @brief Creates and displays the main GUI window and its widgets.
  *
  * Initializes all GTK widgets for the synthesizer interface (sliders,
  * buttons, labels, drawing area, etc.), connects their signals to
  * appropriate callback functions defined in gui.c, and shows the window.
  *
  * @param[in] app The GtkApplication instance that the main window belongs to.
  * @note Assumes g_synth_data is accessible globally via extern declaration in gui.c.
  * @see create_gui() implementation in gui.c
  */
 void create_gui(GtkApplication *app);
 
 
 // --- Declaration for Testing ---
 #ifdef TESTING
 /**
  * @brief Declaration of the envelope calculation helper function for testing.
  *
  * This declaration makes the internal calculate_current_envelope function
  * (defined in gui.c) visible to the test harness when the TESTING macro
  * is defined during compilation.
  *
  * @param[in] data Pointer to the shared synthesizer data structure.
  * @return The calculated envelope multiplier based on the state in `data`.
  * @see calculate_current_envelope() implementation in gui.c
  */
 double calculate_current_envelope(const SharedSynthData *data);
 #endif // TESTING
 
 
 #endif // GUI_H