/**
 * @file synth_data.h
 * @brief Defines shared data structures and enumerations for the synthesizer.
 *
 * This header file contains the definitions for the main `SharedSynthData`
 * structure, which holds all parameters and state shared between the GUI
 * and audio threads. It also defines enumerations for waveform types
 * and ADSR envelope stages, and includes the structure for preset data.
 */

 #ifndef SYNTH_DATA_H
 #define SYNTH_DATA_H
 
 #include <gtk/gtk.h>
 #include <pthread.h>
 
 // --- Enums ---
 
 /**
  * @enum WaveformType
  * @brief Enumeration of available oscillator waveform types.
  */
 typedef enum {
     WAVE_SINE,      ///< Sine wave.
     WAVE_SQUARE,    ///< Square wave.
     WAVE_SAWTOOTH,  ///< Sawtooth wave.
     WAVE_TRIANGLE   ///< Triangle wave.
 } WaveformType;
 
 /**
  * @enum EnvelopeStage
  * @brief Enumeration of the stages in the ADSR (Attack-Decay-Sustain-Release) envelope.
  */
 typedef enum {
     ENV_IDLE,       ///< Envelope is inactive (note off, finished release).
     ENV_ATTACK,     ///< Initial ramp-up phase when note starts.
     ENV_DECAY,      ///< Ramp-down phase after attack, towards sustain level.
     ENV_SUSTAIN,    ///< Constant level phase while note is held after decay.
     ENV_RELEASE     ///< Ramp-down phase after note is released.
 } EnvelopeStage;
 
 
 // --- Preset Data Structure ---
 /**
  * @struct PresetData
  * @brief Structure holding parameters for a single synthesizer preset.
  * Contains values for both waves, suitable for saving/loading.
  */
 typedef struct {
     // Wave 1
     double frequency1;
     double amplitude1;
     WaveformType waveform1; // Store as int/enum value
     double attackTime1;
     double decayTime1;
     double sustainLevel1;
     double releaseTime1;
     // Wave 2
     double frequency2;
     double amplitude2;
     WaveformType waveform2; // Store as int/enum value
     double attackTime2;
     double decayTime2;
     double sustainLevel2;
     double releaseTime2;
 } PresetData;
 
 
 // --- Shared Parameter Structure ---
 
 /**
  * @struct SharedSynthData
  * @brief Structure holding all shared parameters and state for the synthesizer.
  *
  * This structure is the central point for communication between the GUI thread
  * and the real-time audio thread. Access must be protected by the included mutex.
  * Now includes parameters for two independent waves/oscillators and pointers
  * to GUI widgets required for preset loading updates.
  */
 typedef struct {
     // --- Wave 1 Synthesis Parameters (Controlled by GUI, Read by Audio) ---
     double frequency;       ///< Wave 1: Oscillator frequency in Hz.
     double amplitude;       ///< Wave 1: Master amplitude/volume (0.0 to 1.0).
     WaveformType waveform;  ///< Wave 1: Selected oscillator waveform type (see @ref WaveformType).
     double attackTime;      ///< Wave 1: ADSR Attack time in seconds.
     double decayTime;       ///< Wave 1: ADSR Decay time in seconds.
     double sustainLevel;    ///< Wave 1: ADSR Sustain level (0.0 to 1.0, relative to max amplitude).
     double releaseTime;     ///< Wave 1: ADSR Release time in seconds.
 
     // --- Wave 1 State Variables (Managed primarily by Audio Thread) ---
     double phase;           ///< Wave 1: Current phase of the oscillator (0 to 2*PI).
     int note_active;        ///< Wave 1: Flag indicating if note is conceptually "on" (1) or "off" (0).
     EnvelopeStage currentStage; ///< Wave 1: Current stage of the ADSR envelope (see @ref EnvelopeStage).
     double timeInStage;     ///< Wave 1: Time elapsed (in seconds) within the current envelope stage.
     double lastEnvValue;    ///< Wave 1: Envelope value captured at the moment ENV_RELEASE stage begins.
 
     // --- Wave 2 Synthesis Parameters (Controlled by GUI, Read by Audio) ---
     double frequency2;      ///< Wave 2: Oscillator frequency in Hz.
     double amplitude2;      ///< Wave 2: Master amplitude/volume (0.0 to 1.0).
     WaveformType waveform2; ///< Wave 2: Selected oscillator waveform type (see @ref WaveformType).
     double attackTime2;     ///< Wave 2: ADSR Attack time in seconds.
     double decayTime2;      ///< Wave 2: ADSR Decay time in seconds.
     double sustainLevel2;   ///< Wave 2: ADSR Sustain level (0.0 to 1.0, relative to max amplitude).
     double releaseTime2;    ///< Wave 2: ADSR Release time in seconds.
 
     // --- Wave 2 State Variables (Managed primarily by Audio Thread) ---
     double phase2;          ///< Wave 2: Current phase of the oscillator (0 to 2*PI).
     int note_active2;       ///< Wave 2: Flag indicating if note is conceptually "on" (1) or "off" (0).
     EnvelopeStage currentStage2; ///< Wave 2: Current stage of the ADSR envelope (see @ref EnvelopeStage).
     double timeInStage2;    ///< Wave 2: Time elapsed (in seconds) within the current envelope stage.
     double lastEnvValue2;   ///< Wave 2: Envelope value captured at the moment ENV_RELEASE stage begins.
 
     // --- Runtime Configuration ---
     double sampleRate;      ///< Audio sample rate in samples per second (e.g., 44100.0). Set at initialization.
 
     // --- Synchronization Primitive ---
     pthread_mutex_t mutex;  ///< Mutex to protect concurrent access to this structure from GUI and audio threads.
 
     // --- GUI Link (Set by GUI thread) ---
     GtkWidget *waveform_drawing_area; ///< Pointer to the GTK drawing area widget used for waveform visualization.
 
     // --- GUI Widget Pointers (Needed for updating GUI on preset load) ---
     // These should be assigned in create_gui() after widgets are created.
     // Wave 1 Widgets
     GtkRange *freq_slider1_widget;
     GtkRange *amp_slider1_widget;
     GtkComboBox *waveform_combo1_widget;
     GtkRange *attack_slider1_widget;
     GtkRange *decay_slider1_widget;
     GtkRange *sustain_slider1_widget;
     GtkRange *release_slider1_widget;
     // Wave 2 Widgets
     GtkRange *freq_slider2_widget;
     GtkRange *amp_slider2_widget;
     GtkComboBox *waveform_combo2_widget;
     GtkRange *attack_slider2_widget;
     GtkRange *decay_slider2_widget;
     GtkRange *sustain_slider2_widget;
     GtkRange *release_slider2_widget;
 
 } SharedSynthData;
 
 #endif // SYNTH_DATA_H