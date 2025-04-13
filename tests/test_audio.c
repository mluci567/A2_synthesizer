/**
 * @file test_audio.c
 * @brief Unit tests for the audio processing callback (paCallback) using CUnit.
 *
 * This file tests the core audio generation logic within paCallback from audio.c,
 * focusing on the ADSR envelope behavior and waveform generation. It assumes
 * paCallback is made accessible for testing (e.g., via conditional compilation).
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include <pthread.h> // For mutex type in SharedSynthData
 #include <float.h>  // For FLT_MAX
 
 // Include CUnit headers
 #include <CUnit/Basic.h>
 
 // Include project headers (adjust path as needed)
 #include "../synthesizer/synth_data.h" 
 #include "../synthesizer/audio.h"     
 
 // --- Test Globals ---
 /** @brief Mock sample rate used for calculations in tests. */
 #define TEST_SAMPLE_RATE 44100.0
 /** @brief Size of the output buffer used for testing paCallback. */
 #define TEST_BUFFER_SIZE 256
 /** @brief Global output buffer filled by paCallback during tests. */
 float g_test_output_buffer[TEST_BUFFER_SIZE];
 /** @brief Global instance of shared data used as input/output for paCallback tests. */
 SharedSynthData g_test_synth_data;
 /** @brief Tolerance used for comparing floating-point values in assertions. */
 const double TOLERANCE = 0.05; // Tolerance for float comparisons
 
 // --- Test Suite Setup/Teardown ---
 
 /**
  * @brief Initialization function for the Audio Callback test suite.
  * Currently performs no actions.
  * @return 0 on success.
  */
 int init_audio_suite(void) {
     // Mutex initialization not strictly needed here as we won't have
     // concurrent access in these single-threaded unit tests.
     return 0; // Success
 }
 
 /**
  * @brief Cleanup function for the Audio Callback test suite.
  * Currently performs no actions.
  * @return 0 on success.
  */
 int clean_audio_suite(void) {
     return 0; // Success
 }
 
 // --- Helper Functions ---
 
 /**
  * @brief Resets the global `g_test_synth_data` structure and clears the
  * global output buffer before each test function runs.
  */
 void setup_default_synth_data(void) {
     memset(g_test_output_buffer, 0, sizeof(g_test_output_buffer));
     g_test_synth_data = (SharedSynthData){
         .frequency = 440.0, .amplitude = 0.8, .waveform = WAVE_SINE,
         .attackTime = 0.1, .decayTime = 0.2, .sustainLevel = 0.5, .releaseTime = 0.3,
         .phase = 0.0, .note_active = 0, .sampleRate = TEST_SAMPLE_RATE,
         .currentStage = ENV_IDLE, .timeInStage = 0.0, .lastEnvValue = 0.0,
         .waveform_drawing_area = NULL
     };
 }
 
 /**
  * @brief Finds the maximum absolute sample value in the global test output buffer.
  * @return The maximum absolute value found in `g_test_output_buffer`.
  */
 float get_max_abs_output(void) {
     float max_val = 0.0f;
     for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
         if (fabsf(g_test_output_buffer[i]) > max_val) {
             max_val = fabsf(g_test_output_buffer[i]);
         }
     }
     return max_val;
 }
 
 /**
  * @brief Checks if the absolute values in the buffer are generally increasing.
  * Used to verify attack ramps. Allows for minor dips due to waveform shape.
  * @return 1 if the end value is significantly greater than the start value, 0 otherwise.
  * @note This is a heuristic check, not a strict mathematical monotonicity test.
  */
 int is_increasing(void) {
     // Basic check: Is the end value significantly larger than the start value?
     return fabsf(g_test_output_buffer[TEST_BUFFER_SIZE - 1]) > fabsf(g_test_output_buffer[0]);
     // A more complex check could verify the overall trend more robustly.
 }
 
 /**
  * @brief Checks if the absolute values in the buffer are generally decreasing.
  * Used to verify decay/release ramps. Allows for minor peaks due to waveform shape.
  * @return 1 if the end value is significantly smaller than the start value, 0 otherwise.
  * @note This is a heuristic check, not a strict mathematical monotonicity test.
  */
 int is_decreasing(void) {
     // Basic check: Is the end value significantly smaller than the start value?
     return fabsf(g_test_output_buffer[TEST_BUFFER_SIZE - 1]) < fabsf(g_test_output_buffer[0]);
     // A more complex check could verify the overall trend more robustly.
 }
 
 
 // --- Test Functions ---
 
 /**
  * @brief Test paCallback when the envelope stage is ENV_IDLE.
  * Verifies the callback returns paContinue, the stage remains IDLE,
  * and the output buffer contains only silence (zeros).
  */
 void test_adsr_idle(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_IDLE;
     g_test_synth_data.note_active = 0;
 
     // Call the function under test
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     // Assertions
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
     // Check output buffer is silent
     for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
         CU_ASSERT_DOUBLE_EQUAL(g_test_output_buffer[i], 0.0, 1e-9);
     }
 }
 
 /**
  * @brief Test paCallback during the ATTACK phase when attack time is longer than buffer duration.
  * Verifies the stage remains ATTACK, timeInStage increases, the output buffer shows
  * an increasing amplitude trend, and the peak amplitude is less than the maximum.
  */
 void test_adsr_attack_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.attackTime = 0.1; // Longer than buffer duration
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.timeInStage = 0.0; // Start of attack
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_ATTACK); // Should still be attacking
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0); // Time should advance
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.attackTime); // Should not finish attack
     CU_ASSERT(is_increasing()); // Amplitude should generally increase
     CU_ASSERT(get_max_abs_output() < g_test_synth_data.amplitude * 0.9); // Peak shouldn't reach max yet
 }
 
 /**
  * @brief Test paCallback during the ATTACK phase when attack time is very short.
  * Verifies the stage transitions to ENV_DECAY within the callback execution.
  */
 void test_adsr_attack_to_decay_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.attackTime = 0.001; // Very short attack
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.timeInStage = 0.0;
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_DECAY); // Should transition to Decay
     CU_ASSERT(g_test_synth_data.timeInStage < (TEST_BUFFER_SIZE / TEST_SAMPLE_RATE)); // timeInStage reset/low
 }
 
 /**
  * @brief Test paCallback during the DECAY phase when decay time is longer than buffer duration.
  * Verifies the stage remains DECAY, timeInStage increases, the output buffer shows
  * a decreasing amplitude trend towards the sustain level, and the peak amplitude
  * is near the starting (max) amplitude.
  */
 void test_adsr_decay_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.decayTime = 0.1; // Longer than buffer duration
     g_test_synth_data.sustainLevel = 0.25; // Target level 0.2
     g_test_synth_data.timeInStage = 0.0; // Start of decay
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_DECAY); // Should still be decaying
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0); // Time should advance
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.decayTime); // Should not finish decay
     CU_ASSERT(is_decreasing()); // Amplitude should generally decrease
     CU_ASSERT_DOUBLE_EQUAL(get_max_abs_output(), g_test_synth_data.amplitude, TOLERANCE*2); // Starts near peak
     CU_ASSERT(fabsf(g_test_output_buffer[TEST_BUFFER_SIZE - 1]) < fabsf(g_test_output_buffer[0])); // End value lower than start
     CU_ASSERT(get_max_abs_output() > (g_test_synth_data.amplitude * g_test_synth_data.sustainLevel)); // Still above sustain level
 }
 
 /**
  * @brief Test paCallback during the DECAY phase when decay time is very short.
  * Verifies the stage transitions to ENV_SUSTAIN within the callback execution.
  */
 void test_adsr_decay_to_sustain_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.decayTime = 0.001; // Very short decay
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     g_test_synth_data.timeInStage = 0.0;
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN); // Should transition to Sustain
     CU_ASSERT(g_test_synth_data.timeInStage < (TEST_BUFFER_SIZE / TEST_SAMPLE_RATE)); // timeInStage likely 0
 }
 
 /**
  * @brief Test paCallback during the SUSTAIN phase.
  * Verifies the stage remains SUSTAIN and the peak output amplitude matches
  * the expected sustain level (amplitude * sustainLevel).
  */
 void test_adsr_sustain_level(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SINE; // Use sine for predictable peaks
     g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5; // Expect peak amplitude around 0.4
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN); // Stage shouldn't change
     // Check peak value is approximately amplitude * sustainLevel
     CU_ASSERT_DOUBLE_EQUAL(get_max_abs_output(), g_test_synth_data.amplitude * g_test_synth_data.sustainLevel, TOLERANCE);
 }
 
 /**
  * @brief Test paCallback during the RELEASE phase when release time is longer than buffer duration.
  * Verifies the stage remains RELEASE, note_active remains 1, timeInStage increases,
  * the output buffer shows a decreasing amplitude trend from the lastEnvValue,
  * and the peak amplitude is no more than lastEnvValue.
  */
 void test_adsr_release_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_RELEASE;
     g_test_synth_data.note_active = 1; // Note still active
     g_test_synth_data.releaseTime = 0.1; // Longer than buffer duration
     g_test_synth_data.lastEnvValue = 0.4; // Simulate value captured at note off
     g_test_synth_data.timeInStage = 0.0; // Start of release
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_RELEASE); // Should still be releasing
     CU_ASSERT_EQUAL(g_test_synth_data.note_active, 1); // Still active
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0); // Time should advance
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.releaseTime); // Should not finish release
     CU_ASSERT(is_decreasing()); // Amplitude should generally decrease
     CU_ASSERT(get_max_abs_output() <= g_test_synth_data.lastEnvValue * (1.0 + TOLERANCE) ); // Peak should be <= start value
 }
 
 /**
  * @brief Test paCallback during the RELEASE phase when release time is very short.
  * Verifies the stage transitions to ENV_IDLE and note_active becomes 0
  * within the callback execution. Checks output is silent at the end.
  */
 void test_adsr_release_to_idle_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_RELEASE;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.releaseTime = 0.001; // Very short release
     g_test_synth_data.lastEnvValue = 0.4;
     g_test_synth_data.timeInStage = 0.0;
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE); // Should transition to Idle
     CU_ASSERT_EQUAL(g_test_synth_data.note_active, 0); // Note should become inactive
     // Check output buffer is silent by the end
      CU_ASSERT_DOUBLE_EQUAL(g_test_output_buffer[TEST_BUFFER_SIZE - 1], 0.0, 1e-6);
 }
 
 // --- Test Waveforms (during sustain for stable amplitude) ---
 
 /**
  * @brief Test SINE wave generation during SUSTAIN phase.
  * Reuses sustain level test as basic validation.
  */
 void test_waveform_sine(void) {
     // Sine wave is tested as part of the sustain level test
     test_adsr_sustain_level();
     // More specific frequency analysis could be added but is complex for unit tests.
 }
 
 /**
  * @brief Test SQUARE wave generation during SUSTAIN phase.
  * Verifies the output samples are approximately equal to +/- the expected amplitude.
  */
 void test_waveform_square(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SQUARE;
     g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0; // Use full amplitude for simplicity
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
 
     // Check that values are only +amp or -amp (approx)
     int transitions = 0;
     float expected_val = g_test_synth_data.amplitude;
     for (int k = 0; k < TEST_BUFFER_SIZE; ++k) {
         CU_ASSERT_DOUBLE_EQUAL(fabsf(g_test_output_buffer[k]), expected_val, TOLERANCE);
         if (k > 0 && (g_test_output_buffer[k] * g_test_output_buffer[k-1] < 0)) {
            transitions++; // Count sign changes
         }
     }
     CU_ASSERT(transitions > 0); // Ensure it's oscillating, not stuck
 }
 
 /**
  * @brief Test SAWTOOTH wave generation during SUSTAIN phase.
  * Verifies the output is non-silent, stays within amplitude bounds,
  * and exhibits the characteristic sharp drop.
  */
 void test_waveform_sawtooth(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SAWTOOTH;
     g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0; // Use full amplitude
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
 
     // Check characteristics: ramp up (-amp to +amp) and sharp drop
     int drops = 0;
     for (int k = 1; k < TEST_BUFFER_SIZE; ++k) {
         // Check for the sharp drop using a heuristic threshold
         if (g_test_output_buffer[k] < g_test_output_buffer[k-1] - 0.1) {
            drops++;
         }
         // Values should generally be within +/- amplitude
         CU_ASSERT(fabsf(g_test_output_buffer[k]) <= g_test_synth_data.amplitude * (1.0 + TOLERANCE));
     }
     CU_ASSERT(drops > 0); // Ensure the characteristic drop occurs at least once
     CU_ASSERT(get_max_abs_output() > 0.0); // Ensure output is not silent
 }
 
 /**
  * @brief Test TRIANGLE wave generation during SUSTAIN phase.
  * Verifies the output is non-silent, stays within amplitude bounds,
  * and exhibits turning points (peaks and valleys).
  */
 void test_waveform_triangle(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_TRIANGLE;
     g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0; // Use full amplitude
 
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
 
     CU_ASSERT_EQUAL(result, 0); // paContinue
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
 
     // Check characteristics: ramps up and down
     int peaks = 0;
     int valleys = 0;
      for (int k = 1; k < TEST_BUFFER_SIZE - 1; ++k) {
         // Check for local peaks
         if (g_test_output_buffer[k] > g_test_output_buffer[k-1] && g_test_output_buffer[k] > g_test_output_buffer[k+1]) {
            peaks++;
            CU_ASSERT(fabsf(g_test_output_buffer[k]) <= g_test_synth_data.amplitude * (1.0 + TOLERANCE));
         }
         // Check for local valleys
          if (g_test_output_buffer[k] < g_test_output_buffer[k-1] && g_test_output_buffer[k] < g_test_output_buffer[k+1]) {
            valleys++;
             CU_ASSERT(fabsf(g_test_output_buffer[k]) <= g_test_synth_data.amplitude * (1.0 + TOLERANCE));
         }
     }
     CU_ASSERT(peaks > 0 || valleys > 0); // Ensure there are turning points
     CU_ASSERT(get_max_abs_output() > 0.0); // Ensure output is not silent
 }
 
 // --- Main Test Runner Function ---
 
 /**
  * @brief Main entry point for the audio callback test suite.
  * Initializes the CUnit registry, adds the test suite and associated test cases,
  * runs the tests using the basic console interface, and reports results.
  * @return 0 if all tests pass, non-zero otherwise.
  */
 int main() {
     CU_pSuite pSuite = NULL;
 
     // Initialize the CUnit test registry
     if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
 
     // Add a suite to the registry
     pSuite = CU_add_suite("Audio_Callback_Tests", init_audio_suite, clean_audio_suite);
     if (NULL == pSuite) {
         CU_cleanup_registry(); return CU_get_error();
     }
 
     // Add the tests to the suite
     if ((NULL == CU_add_test(pSuite, "test_adsr_idle", test_adsr_idle)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_attack_ramp", test_adsr_attack_ramp)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_attack_to_decay_transition", test_adsr_attack_to_decay_transition)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_decay_ramp", test_adsr_decay_ramp)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_decay_to_sustain_transition", test_adsr_decay_to_sustain_transition)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_sustain_level", test_adsr_sustain_level)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_release_ramp", test_adsr_release_ramp)) ||
         (NULL == CU_add_test(pSuite, "test_adsr_release_to_idle_transition", test_adsr_release_to_idle_transition)) ||
         (NULL == CU_add_test(pSuite, "test_waveform_sine", test_waveform_sine)) ||
         (NULL == CU_add_test(pSuite, "test_waveform_square", test_waveform_square)) ||
         (NULL == CU_add_test(pSuite, "test_waveform_sawtooth", test_waveform_sawtooth)) ||
         (NULL == CU_add_test(pSuite, "test_waveform_triangle", test_waveform_triangle))
        )
     {
         CU_cleanup_registry(); return CU_get_error();
     }
 
     // Run all tests using the CUnit Basic interface
     CU_basic_set_mode(CU_BRM_VERBOSE); // Set output mode
     CU_basic_run_tests();             // Run tests
     printf("\n");
     CU_basic_show_failures(CU_get_failure_list()); // Print summary of failures
     printf("\n\n");
 
     // Get number of failures to return appropriate exit code
     unsigned int failures = CU_get_number_of_failures();
 
     // Cleanup CUnit registry
     CU_cleanup_registry();
 
     // Return 1 if any tests failed, 0 otherwise
     return (failures > 0) ? 1 : 0;
 }
 