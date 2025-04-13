/**
 * @file synth_data.h
 * @brief Defines shared data structures and enumerations for the synthesizer.
 *
 * This header file contains the definitions for the main `SharedSynthData`
 * structure, which holds all parameters and state shared between the GUI
 * and audio threads. It also defines enumerations for waveform types
 * and ADSR envelope stages.
 */

 #ifndef SYNTH_DATA_H
 #define SYNTH_DATA_H
 
 #include <gtk/gtk.h>      // For GtkWidget (used for drawing area link)
 #include <pthread.h>    // For pthread_mutex_t
 
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
 
 
 // --- Shared Parameter Structure ---
 
 /**
  * @struct SharedSynthData
  * @brief Structure holding all shared parameters and state for the synthesizer.
  *
  * This structure is the central point for communication between the GUI thread
  * and the real-time audio thread. Access must be protected by the included mutex.
  */
 typedef struct {
     // --- Synthesis Parameters (Controlled by GUI, Read by Audio) ---
     double frequency;       ///< Oscillator frequency in Hz.
     double amplitude;       ///< Master amplitude/volume (0.0 to 1.0).
     WaveformType waveform;  ///< Selected oscillator waveform type (see @ref WaveformType).
     double attackTime;      ///< ADSR Attack time in seconds.
     double decayTime;       ///< ADSR Decay time in seconds.
     double sustainLevel;    ///< ADSR Sustain level (0.0 to 1.0, relative to max amplitude).
     double releaseTime;     ///< ADSR Release time in seconds.
 
     // --- State Variables (Managed primarily by Audio Thread) ---
     double phase;           ///< Current phase of the oscillator (0 to 2*PI), updated by audio thread.
     int note_active;        ///< Flag indicating if a note is conceptually "on" (1) or "off" (0). Set by GUI on note-on, cleared by audio thread when release finishes.
 
     // --- ADSR Envelope State (Managed primarily by Audio Thread) ---
     EnvelopeStage currentStage; ///< Current stage of the ADSR envelope (see @ref EnvelopeStage). Updated by GUI (note on/off) and audio thread (transitions).
     double timeInStage;     ///< Time elapsed (in seconds) within the current envelope stage. Updated by audio thread.
     double lastEnvValue;    ///< Envelope value captured at the moment ENV_RELEASE stage begins. Set by GUI thread, read by audio thread.
 
     // --- Runtime Configuration ---
     double sampleRate;      ///< Audio sample rate in samples per second (e.g., 44100.0). Set at initialization.
 
     // --- Synchronization Primitive ---
     pthread_mutex_t mutex;  ///< Mutex to protect concurrent access to this structure from GUI and audio threads.
 
     // --- GUI Link (Optional but useful) ---
     GtkWidget *waveform_drawing_area; ///< Pointer to the GTK drawing area widget used for waveform visualization. Set by GUI thread.
 
 } SharedSynthData;
 
 #endif // SYNTH_DATA_H