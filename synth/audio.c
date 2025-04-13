/**
 * @file audio.c
 * @brief Handles audio initialization, processing, and termination using PortAudio.
 *
 * This file contains the core audio callback function responsible for generating
 * synthesizer waveforms based on shared parameters, including ADSR envelope
 * calculation. It also manages the PortAudio stream lifecycle.
 */

 #include <portaudio.h>
 // #include <pa_linux_alsa.h> // Linux-specific, not needed on macOS
 #include <pthread.h>
 #include <math.h>
 #include <stdio.h>
 #include <string.h> // For strerror
 #include <errno.h>  // For errno
 #include <sched.h> // For scheduling constants/functions
 
 #include "audio.h"      // Include the header for this module
 #include "synth_data.h" // Include the shared data structure definition
 
 // --- External Global Shared Data Instance ---
 /**
  * @var g_synth_data
  * @brief External reference to the globally shared synthesizer data structure.
  * @note Defined in main.c, declared here as extern. Protected by a mutex within the struct.
  */
 extern SharedSynthData g_synth_data;
 
 // --- Module-Specific Global Variable ---
 /**
  * @var g_paStream
  * @brief Pointer to the active PortAudio stream. NULL if no stream is active.
  * @note Static to this file, managed by start_audio(), stop_audio().
  */
 static PaStream *g_paStream = NULL;
 
 // --- Error Handling Macros ---
 
 /**
  * @def CHECK_PTHREAD_ERR(ret, func_name)
  * @brief Checks the return value of a pthread function and prints an error if non-zero.
  * @param ret The return value from the pthread function.
  * @param func_name A string literal naming the function call being checked.
  */
 #define CHECK_PTHREAD_ERR(ret, func_name) \
     if (ret != 0) { \
         fprintf(stderr, "PThread Error in %s (Audio): %s\n", func_name, strerror(ret)); \
     }
 
 /**
  * @def CHECK_PA_ERR_RETURN(err, func_name)
  * @brief Checks the return value of a PortAudio function and returns the error code on failure.
  * Also attempts to clean up the PortAudio stream if an error occurs during
  * initialization or operation (but not during stop/close itself).
  * @param err The PaError code returned by the PortAudio function.
  * @param func_name A string literal naming the function call being checked.
  */
 #define CHECK_PA_ERR_RETURN(err, func_name) \
     if (err != paNoError) { \
         fprintf(stderr, "PortAudio Error in %s: %s\n", func_name, Pa_GetErrorText(err)); \
         if (g_paStream && (strcmp(func_name,"Pa_CloseStream")!=0) && (strcmp(func_name,"Pa_StopStream")!=0) ) { \
              /* Attempt cleanup only if stream exists and error wasn't during stop/close */ \
              Pa_CloseStream(g_paStream); g_paStream = NULL; \
         } \
         /* Pa_Terminate(); Don't terminate here if error is recoverable */ \
         return err; \
     }
 
 // --- PortAudio Callback Function ---
 
 /**
  * @brief PortAudio callback function for generating audio samples.
  *
  * This function is called by the PortAudio library in a high-priority thread
  * whenever the audio device needs more samples. It reads shared synthesizer
  * parameters, calculates the ADSR envelope, generates the appropriate waveform,
  * applies the envelope, and writes the samples to the output buffer.
  * It minimizes mutex lock time by copying parameters locally.
  *
  * @param inputBuffer Unused (input audio buffer).
  * @param outputBuffer Buffer where generated audio samples (float) should be written.
  * @param framesPerBuffer The number of sample frames to generate for the buffer.
  * @param timeInfo Timing information from PortAudio (unused).
  * @param statusFlags Flags indicating buffer under/overflow or other conditions.
  * @param userData A pointer to the SharedSynthData structure containing synth parameters and state.
  *
  * @return `paContinue` (0) if processing should continue, or `paAbort` (<0) on critical errors (like mutex failure).
  *
  * @note This function is conditionally non-static (`#ifdef TESTING`) to allow unit testing.
  * @warning Must be real-time safe. Avoid blocking operations, excessive computation, or holding mutexes for too long.
  */
 #ifdef TESTING
 int paCallback( const void *inputBuffer, void *outputBuffer,
 #else
 static int paCallback( const void *inputBuffer, void *outputBuffer,
 #endif // TESTING
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData )
 {
     SharedSynthData *shared_data = (SharedSynthData*)userData;
     float *out = (float*)outputBuffer;
     unsigned long i;
     int ret_lock, ret_unlock;
 
     // Local copies for thread safety and reduced lock contention
     double local_freq, local_amp, local_sustain_level, local_attack_time, local_decay_time, local_release_time;
     WaveformType local_wave;
     EnvelopeStage local_stage;
     int local_note_active;
     double local_phase;
     double local_timeInStage;
     double local_lastEnvValue; // Value captured by GUI thread at note-off
     double local_sampleRate;
 
     // ADSR state variables local to the callback run
     double env_multiplier = 0.0;
     double time_increment;
 
     // Check for PortAudio buffer issues
     if (statusFlags & (paOutputUnderflow | paOutputOverflow)) {
         fprintf(stderr, "PortAudio Warning: Buffer under/overflow detected (flags: %lu)\n", statusFlags);
     }
 
     // --- Short Critical Section: Read Shared Parameters and State ---
     ret_lock = pthread_mutex_lock(&shared_data->mutex);
     if (ret_lock != 0) {
         fprintf(stderr, "CRITICAL: Error in paCallback lock (read): %s. Outputting silence.\n", strerror(ret_lock));
         // Output silence to prevent garbage audio
         for( i = 0; i < framesPerBuffer; i++ ) { *out++ = 0.0f; }
         return paAbort; // Abort stream on critical lock failure
     }
 
     // Copy necessary data from shared structure to local variables
     local_freq = shared_data->frequency;
     local_amp = shared_data->amplitude;
     local_wave = shared_data->waveform;
     local_note_active = shared_data->note_active;
     local_stage = shared_data->currentStage;
     local_attack_time = shared_data->attackTime;
     local_decay_time = shared_data->decayTime;
     local_sustain_level = shared_data->sustainLevel;
     local_release_time = shared_data->releaseTime;
     local_phase = shared_data->phase;
     local_timeInStage = shared_data->timeInStage;
     local_lastEnvValue = shared_data->lastEnvValue; // Get value captured by GUI
     local_sampleRate = shared_data->sampleRate;
 
     // Unlock mutex as quickly as possible
     ret_unlock = pthread_mutex_unlock(&shared_data->mutex);
      if (ret_unlock != 0) {
          fprintf(stderr, "CRITICAL Error in paCallback unlock (read): %s\n", strerror(ret_unlock));
          // Data might be inconsistent, but try to generate silence before aborting
          for( i = 0; i < framesPerBuffer; i++ ) { *out++ = 0.0f; }
          return paAbort;
     }
     // --- End Read Critical Section ---
 
     // Calculate time increment per sample
     time_increment = 1.0 / local_sampleRate;
 
     // --- Audio Generation Loop (Mutex is NOT HELD) ---
     for( i = 0; i < framesPerBuffer; i++ )
     {
         float sample = 0.0f;
 
         // --- 1. Update Envelope Stage & Calculate Multiplier (using local copies) ---
         local_timeInStage += time_increment;
 
         // State machine for ADSR envelope
         switch(local_stage)
         {
             case ENV_IDLE:
                 env_multiplier = 0.0;
                 break;
             case ENV_ATTACK:
                 // Calculate attack ramp
                 if (local_attack_time <= 0.0) { // Instant attack
                     env_multiplier = local_amp;
                     local_stage = ENV_DECAY; local_timeInStage = 0.0;
                 } else {
                     env_multiplier = local_amp * fmin(1.0, (local_timeInStage / local_attack_time));
                 }
                 // Check for transition to Decay
                 if (local_timeInStage >= local_attack_time) {
                     env_multiplier = local_amp; // Ensure peak
                     local_stage = ENV_DECAY; local_timeInStage = 0.0;
                 }
                 break;
             case ENV_DECAY:
                 // Calculate decay ramp towards sustain level
                  if (local_decay_time <= 0.0 || local_sustain_level >= 1.0) { // Instant decay or max sustain
                     env_multiplier = local_amp * local_sustain_level;
                     local_stage = ENV_SUSTAIN; local_timeInStage = 0.0;
                  } else {
                     double decay_factor = fmin(1.0, local_timeInStage / local_decay_time);
                     env_multiplier = local_amp * (1.0 - (1.0 - local_sustain_level) * decay_factor);
                  }
                 // Check for transition to Sustain
                 if (local_timeInStage >= local_decay_time) {
                     env_multiplier = local_amp * local_sustain_level; // Ensure sustain level reached
                     local_stage = ENV_SUSTAIN; local_timeInStage = 0.0;
                 }
                 // Clamp lower bound during decay
                 if (env_multiplier < local_amp * local_sustain_level) {
                      env_multiplier = local_amp * local_sustain_level;
                 }
                 break;
             case ENV_SUSTAIN:
                 // Hold at sustain level
                 env_multiplier = local_amp * local_sustain_level;
                 // Stage remains SUSTAIN until GUI triggers RELEASE
                 break;
             case ENV_RELEASE:
                  // Calculate release ramp down from captured level
                  if (local_release_time <= 0.0 || local_lastEnvValue <= 1e-9) { // Instant release or starting near zero
                     env_multiplier = 0.0;
                  } else {
                     env_multiplier = local_lastEnvValue * fmax(0.0, (1.0 - (local_timeInStage / local_release_time)));
                  }
                  // Check for transition to Idle
                 if (local_timeInStage >= local_release_time || env_multiplier <= 1e-9) {
                     env_multiplier = 0.0;
                     local_stage = ENV_IDLE;
                     local_note_active = 0; // Mark note fully off now
                 }
                 break;
              default: // Should not happen
                 fprintf(stderr, "Warning: Unknown envelope stage %d\n", local_stage);
                 env_multiplier = 0.0;
                 local_stage = ENV_IDLE;
                 local_note_active = 0;
                 break;
         }
         // Clamp envelope multiplier (safety check)
         env_multiplier = fmax(0.0, fmin(local_amp, env_multiplier));
 
         // --- 2. Generate Waveform Sample ---
         // Only generate if envelope is audible
         if (env_multiplier > 1e-9) {
             // Select waveform based on local copy
             switch(local_wave) {
                  case WAVE_SINE:     sample = (float)(sin(local_phase)); break;
                  case WAVE_SQUARE:   sample = (float)((sin(local_phase) >= 0.0 ? 1.0 : -1.0)); break;
                  case WAVE_SAWTOOTH: sample = (float)((fmod(local_phase, 2.0 * M_PI) / M_PI) - 1.0); break; // Ramps -1 to +1
                  case WAVE_TRIANGLE: sample = (float)((2.0 / M_PI) * asin(sin(local_phase))); break; // Approximation -1 to +1
                  default:            sample = 0.0f; break;
             }
             // Apply envelope
             sample *= env_multiplier;
 
             // --- 3. Update Phase ---
             // Increment phase based on frequency and sample rate
             local_phase += 2.0 * M_PI * local_freq / local_sampleRate;
             // Wrap phase to [0, 2*PI)
             local_phase = fmod(local_phase, 2.0 * M_PI);
             if (local_phase < 0.0) local_phase += 2.0 * M_PI;
         } else {
             // Output silence if envelope is effectively zero
             sample = 0.0f;
              if (local_stage == ENV_IDLE && local_phase != 0.0) {
                  // local_phase = 0.0; // Optional: Reset phase when idle?
              }
         }
         // Write the final sample to the output buffer
         *out++ = sample;
     }
     // --- End Audio Generation Loop ---
 
     // --- Short Critical Section: Write Back Updated State ---
     // Lock mutex to safely update shared state variables
     ret_lock = pthread_mutex_lock(&shared_data->mutex);
      if (ret_lock != 0) {
         fprintf(stderr, "CRITICAL: Error in paCallback lock (write): %s. State lost.\n", strerror(ret_lock));
         // Cannot safely update state. Abort stream to prevent inconsistent state.
         return paAbort;
     }
 
     // Write back state variables that were modified locally
     shared_data->phase = local_phase;
     shared_data->timeInStage = local_timeInStage;
     shared_data->currentStage = local_stage;
     shared_data->note_active = local_note_active;
     // lastEnvValue is only written by GUI thread now
 
     // Unlock mutex
     ret_unlock = pthread_mutex_unlock(&shared_data->mutex);
      if (ret_unlock != 0) {
          fprintf(stderr, "CRITICAL Error in paCallback unlock (write): %s\n", strerror(ret_unlock));
          // Mutex state is potentially undefined. Abort stream.
          return paAbort;
     }
     // --- End Write Critical Section ---
 
     // Signal PortAudio to continue processing
     return paContinue; // paContinue = 0
 }
 
 
 /**
  * @brief Initializes the PortAudio library and synth state.
  *
  * Must be called once at application startup before any other audio functions.
  * Initializes the PortAudio library itself and sets the initial ADSR envelope
  * state in the shared data structure.
  *
  * @param[in,out] data Pointer to the shared synthesizer data structure.
  * The envelope state fields (currentStage, timeInStage, lastEnvValue)
  * will be initialized. Mutex must be initialized beforehand.
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  */
 PaError initialize_audio(SharedSynthData *data) {
     PaError err = Pa_Initialize();
      if (err != paNoError) {
         fprintf(stderr, "PortAudio Error in Pa_Initialize: %s\n", Pa_GetErrorText(err));
         return err;
     }
     printf("PortAudio initialized. Version: %s\n", Pa_GetVersionInfo()->versionText);
 
     // Initialize ADSR state safely within the shared data
     int ret_lock = pthread_mutex_lock(&data->mutex);
     CHECK_PTHREAD_ERR(ret_lock, "initialize_audio lock");
     if (ret_lock == 0) {
         data->currentStage = ENV_IDLE;
         data->timeInStage = 0.0;
         data->lastEnvValue = 0.0; // Initialize to 0
         int ret_unlock = pthread_mutex_unlock(&data->mutex);
         CHECK_PTHREAD_ERR(ret_unlock, "initialize_audio unlock");
     } else {
         // Handle lock failure during initialization - critical error.
         Pa_Terminate(); // Clean up PortAudio if lock fails
         return paInternalError; // Indicate initialization failure
     }
     return paNoError;
 }
 
 
 /**
  * @brief Opens and starts the default PortAudio output stream.
  *
  * Configures and opens the default audio output device using parameters
  * (like sample rate) from the shared data structure. Starts the stream,
  * which begins calling the `paCallback` function.
  *
  * @param[in] data Pointer to the shared synthesizer data structure (used for sample rate and passed to callback).
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  * @note Sets the global `g_paStream` pointer on success.
  */
 PaError start_audio(SharedSynthData *data) {
     PaError err;
     PaStreamParameters outputParameters;
     // Let PortAudio choose the buffer size for potentially lower latency
     unsigned long framesPerBuffer = paFramesPerBufferUnspecified;
 
     // Check if stream is already running
     if (g_paStream != NULL) {
         printf("Audio stream already started.\n");
         return paNoError;
     }
 
     // Get the default output device
     outputParameters.device = Pa_GetDefaultOutputDevice();
     if (outputParameters.device == paNoDevice) {
         fprintf(stderr,"Error: No default output device found.\n");
         return paDeviceUnavailable;
     }
 
     // Get device information (for name and suggested latency)
     const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(outputParameters.device);
     if (deviceInfo == NULL) {
         fprintf(stderr,"Error: Could not get info for default output device %d.\n", outputParameters.device);
         return paInternalError;
     }
     printf("Using default output device: %s\n", deviceInfo->name);
 
     // Configure output stream parameters
     outputParameters.channelCount = 1; // Mono output
     outputParameters.sampleFormat = paFloat32; // Use 32-bit float samples
     // Use default low latency setting - adjust if needed
     outputParameters.suggestedLatency = deviceInfo->defaultLowOutputLatency;
     outputParameters.hostApiSpecificStreamInfo = NULL; // No specific info needed
 
     // Read sample rate safely from shared data
     double currentSampleRate;
     int ret_lock = pthread_mutex_lock(&data->mutex);
     CHECK_PTHREAD_ERR(ret_lock, "start_audio lock");
     if (ret_lock != 0) return paInternalError; // Cannot proceed without sample rate
     currentSampleRate = data->sampleRate;
     int ret_unlock = pthread_mutex_unlock(&data->mutex);
     CHECK_PTHREAD_ERR(ret_unlock, "start_audio unlock");
      // Check unlock failure - if lock succeeded, unlock should ideally not fail here often
      if (ret_lock != 0 && ret_unlock != 0) return paInternalError;
 
 
     printf("Opening stream: SR=%.1f, Frames/Buf=%lu, Suggested Latency=%.4f\n",
            currentSampleRate, framesPerBuffer, outputParameters.suggestedLatency);
 
     // Open the default stream
     err = Pa_OpenDefaultStream(&g_paStream, // Pointer to the stream pointer variable
                                0, // no input channels
                                outputParameters.channelCount,
                                outputParameters.sampleFormat,
                                currentSampleRate,
                                framesPerBuffer,
                                paCallback, // The audio processing callback
                                data );     // User data pointer passed to callback
     // Use macro that checks error and returns on failure
     CHECK_PA_ERR_RETURN(err, "Pa_OpenDefaultStream");
 
     // Start the stream (begins callback execution)
     err = Pa_StartStream(g_paStream);
     CHECK_PA_ERR_RETURN(err, "Pa_StartStream");
 
     printf("Audio stream started successfully.\n");
     return paNoError;
 }
 
 
 /**
  * @brief Stops and closes the active PortAudio stream.
  *
  * If a stream is active (`g_paStream` is not NULL), this function stops it
  * (preventing further calls to `paCallback`) and then closes it, releasing
  * associated resources. Safe to call even if the stream is already stopped.
  *
  * @return `paNoError` (0) on success, or a negative PaError code if closing the stream fails.
  * Errors during stopping are logged but don't prevent closing attempt.
  * @note Resets the global `g_paStream` pointer to NULL on success or after a close failure.
  */
 PaError stop_audio() {
     PaError err = paNoError;
     // Check if stream exists
     if (g_paStream == NULL) { return paNoError; }
 
     printf("Stopping audio stream...\n");
 
     // Stop the stream
     err = Pa_StopStream(g_paStream);
     // Log error if stop fails, unless it was already stopped
     if (err != paNoError && err != paStreamIsStopped) {
        fprintf(stderr, "PortAudio Error in Pa_StopStream: %s\n", Pa_GetErrorText(err));
        // Continue to close even if stop failed
     }
 
     // Close the stream
     err = Pa_CloseStream(g_paStream);
     g_paStream = NULL; // Mark as closed *after* attempting close
      if (err != paNoError) {
         // Log error if close fails
         fprintf(stderr, "PortAudio Error in Pa_CloseStream: %s\n", Pa_GetErrorText(err));
         return err; // Return the error from CloseStream
     }
 
     printf("Audio stream stopped and closed.\n");
     return paNoError; // Return success only if CloseStream succeeded
 }
 
 
 /**
  * @brief Terminates the PortAudio library.
  *
  * Should be called once at application exit after the audio stream has been
  * stopped and closed. Attempts to stop/close the stream first if it appears active.
  *
  * @return `paNoError` (0) on success, or a negative PaError code on failure.
  */
 PaError terminate_audio() {
     PaError err = paNoError;
 
     // Ensure stream is stopped before terminating PortAudio
     if (g_paStream != NULL) {
         fprintf(stderr, "Warning: Terminating PortAudio while stream seems open. Attempting stop first.\n");
         stop_audio(); // Attempt graceful stop/close
         // Check if stop_audio failed to clear the stream pointer
         if (g_paStream != NULL) {
              fprintf(stderr, "Error: Failed to stop/close stream before termination.\n");
              g_paStream = NULL; // Force NULL if close failed to avoid issues
         }
     }
 
     printf("Terminating PortAudio...\n");
     // Terminate the PortAudio library
     err = Pa_Terminate();
      if (err != paNoError) {
         // Log termination errors prominently
         fprintf(stderr, "FATAL PortAudio Error in Pa_Terminate: %s\n", Pa_GetErrorText(err));
         return err;
     }
 
     printf("PortAudio terminated successfully.\n");
     return paNoError;
 } 