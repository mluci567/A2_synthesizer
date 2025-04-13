/**
 * @file audio.h
 * @brief Public interface for the audio processing module.
 *
 * Declares functions for initializing, starting, stopping, and terminating
 * the audio stream using PortAudio. Also includes definitions shared
 * between the audio module and other parts of the application.
 */

 #ifndef AUDIO_H
 #define AUDIO_H
 
 #include <portaudio.h>  // For PaError and other PortAudio types
 #include "synth_data.h" // For SharedSynthData type
 
 // --- Public Audio Control Functions ---
 
 /**
  * @brief Initializes the PortAudio library and synth state.
  * @param[in,out] data Pointer to the shared synthesizer data structure.
  * The envelope state fields will be initialized.
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  * @see initialize_audio() implementation in audio.c
  */
 PaError initialize_audio(SharedSynthData *data);
 
 /**
  * @brief Opens and starts the default PortAudio output stream.
  * @param[in] data Pointer to the shared synthesizer data structure (used for
  * sample rate and passed to the audio callback).
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  * @see start_audio() implementation in audio.c
  */
 PaError start_audio(SharedSynthData *data);
 
 /**
  * @brief Stops and closes the active PortAudio stream.
  * @return `paNoError` (0) on success, or a negative PaError code if closing fails.
  * @note Safe to call even if the stream is already stopped.
  * @see stop_audio() implementation in audio.c
  */
 PaError stop_audio();
 
 /**
  * @brief Terminates the PortAudio library.
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  * @note Should be called after stop_audio() at application exit.
  * @see terminate_audio() implementation in audio.c
  */
 PaError terminate_audio();
 
 
 // --- Declaration for Testing ---
 #ifdef TESTING
 /**
  * @brief Declaration of the audio callback function for testing purposes.
  *
  * This declaration makes the internal paCallback function visible to the
  * test harness when the TESTING macro is defined during compilation.
  * The actual implementation is in audio.c.
  *
  * @param inputBuffer Unused input buffer.
  * @param outputBuffer Buffer for generated audio samples.
  * @param framesPerBuffer Number of sample frames to generate.
  * @param timeInfo PortAudio timing information. Use the official type.
  * @param statusFlags PortAudio status flags. Use the official type.
  * @param userData Pointer to the SharedSynthData structure.
  * @return `paContinue` (0) or `paAbort` (<0).
  * @see paCallback() implementation in audio.c
  */
 int paCallback( const void *inputBuffer, void *outputBuffer,
                 unsigned long framesPerBuffer,
                 const PaStreamCallbackTimeInfo* timeInfo, // <-- Use official type
                 PaStreamCallbackFlags statusFlags,       // <-- Use official type
                 void *userData );
 #endif
 
 
 #endif // AUDIO_H