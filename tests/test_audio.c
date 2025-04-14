/**
 * @file test_audio.c
 * @brief Unit tests for the audio processing callback (paCallback) using CUnit.
 *
 * This file tests the core audio generation logic within paCallback from audio.c,
 * focusing on the ADSR envelope behavior, waveform generation, and mixing for
 * two independent waves. It assumes paCallback is made accessible for testing
 * (e.g., via conditional compilation).
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include <pthread.h> 
 #include <float.h>  
 #include <errno.h>  
 #include <CUnit/Basic.h>

 #include "../synth/synth_data.h" 
 #include "../synth/audio.h"     

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

 int init_audio_suite(void) {
     return 0;
 }

 int clean_audio_suite(void) {
     int ret = pthread_mutex_destroy(&g_test_synth_data.mutex);
      if (ret != 0) {
         fprintf(stderr, "Warning: Failed to destroy mutex in test cleanup: %s\n", strerror(ret));
     }
     return 0;
 }

 // --- Helper Functions ---

 void setup_default_synth_data(void) {
     memset(g_test_output_buffer, 0, sizeof(g_test_output_buffer));
     g_test_synth_data = (SharedSynthData){
         // Wave 1 Defaults
         .frequency = 440.0, .amplitude = 0.8, .waveform = WAVE_SINE,
         .attackTime = 0.1, .decayTime = 0.2, .sustainLevel = 0.5, .releaseTime = 0.3,
         .phase = 0.0, .note_active = 0,
         .currentStage = ENV_IDLE, .timeInStage = 0.0, .lastEnvValue = 0.0,
         // Wave 2 Defaults (Silent)
         .frequency2 = 660.0, .amplitude2 = 0.0, .waveform2 = WAVE_SQUARE,
         .attackTime2 = 0.1, .decayTime2 = 0.2, .sustainLevel2 = 0.5, .releaseTime2 = 0.3,
         .phase2 = 0.0, .note_active2 = 0,
         .currentStage2 = ENV_IDLE, .timeInStage2 = 0.0, .lastEnvValue2 = 0.0,
         // Common
         .sampleRate = TEST_SAMPLE_RATE, .waveform_drawing_area = NULL
     };
     int ret = pthread_mutex_init(&g_test_synth_data.mutex, NULL);
     if (ret != 0) {
         fprintf(stderr, "FATAL: Mutex initialization failed in test setup: %s\n", strerror(ret));
     }
 }

 float get_max_abs_output(void) {
     float max_val = 0.0f;
     for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
         if (fabsf(g_test_output_buffer[i]) > max_val) {
             max_val = fabsf(g_test_output_buffer[i]);
         }
     }
     return max_val;
 }

 int is_increasing(void) {
     // Find max abs value in first and second halves
     int half_point = TEST_BUFFER_SIZE / 2;
     float max_abs_first_half = 0.0f;
     float max_abs_second_half = 0.0f;
     for (int i = 0; i < half_point; ++i) {
         if (fabsf(g_test_output_buffer[i]) > max_abs_first_half) {
             max_abs_first_half = fabsf(g_test_output_buffer[i]);
         }
     }
      for (int i = half_point; i < TEST_BUFFER_SIZE; ++i) {
         if (fabsf(g_test_output_buffer[i]) > max_abs_second_half) {
             max_abs_second_half = fabsf(g_test_output_buffer[i]);
         }
     }
     // Increasing if peak in second half is greater (allow for small tolerance)
     return max_abs_second_half > max_abs_first_half + 1e-6;
 }


 /**
  * @brief Checks if the amplitude envelope in the buffer is generally decreasing.
  * Compares the peak absolute amplitude in the first half of the buffer
  * to the peak absolute amplitude in the second half.
  * @return 1 if the peak amplitude decreased, 0 otherwise.
  * @note This heuristic is more robust to waveform oscillations than comparing start/end samples.
  */
 int is_decreasing(void) {
     int half_point = TEST_BUFFER_SIZE / 2;
     if (half_point <= 0) return 1; // Cannot compare if buffer too small

     float max_abs_first_half = 0.0f;
     float max_abs_second_half = 0.0f;

     // Find max absolute value in the first half
     for (int i = 0; i < half_point; ++i) {
         if (fabsf(g_test_output_buffer[i]) > max_abs_first_half) {
             max_abs_first_half = fabsf(g_test_output_buffer[i]);
         }
     }

     // Find max absolute value in the second half
     for (int i = half_point; i < TEST_BUFFER_SIZE; ++i) {
         if (fabsf(g_test_output_buffer[i]) > max_abs_second_half) {
             max_abs_second_half = fabsf(g_test_output_buffer[i]);
         }
     }

     // Considered decreasing if the peak in the second half is strictly less
     // than the peak in the first half (allowing for floating point noise).
     // Or if the first half was already near zero.
     return (max_abs_second_half < max_abs_first_half - 1e-6) || (max_abs_first_half < 1e-6);
 }


 // --- Test Functions ---

 // ============================================
 // ==          Wave 1 Tests                  ==
 // ============================================
 void test_adsr_idle_both_waves(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_IDLE; g_test_synth_data.note_active = 0;
     g_test_synth_data.currentStage2 = ENV_IDLE; g_test_synth_data.note_active2 = 0;
     g_test_synth_data.amplitude2 = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
     for (int i = 0; i < TEST_BUFFER_SIZE; i++) { CU_ASSERT_DOUBLE_EQUAL(g_test_output_buffer[i], 0.0, 1e-9); }
 }

 void test_w1_adsr_attack_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_ATTACK; g_test_synth_data.note_active = 1;
     g_test_synth_data.attackTime = 0.1; g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_ATTACK);
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0);
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.attackTime);
     CU_ASSERT(is_increasing()); // Use new helper
     CU_ASSERT(get_max_abs_output() < g_test_synth_data.amplitude * 0.9);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_attack_to_decay_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_ATTACK; g_test_synth_data.note_active = 1;
     g_test_synth_data.attackTime = 0.001; g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_DECAY);
     CU_ASSERT(g_test_synth_data.timeInStage < (TEST_BUFFER_SIZE / TEST_SAMPLE_RATE));
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_decay_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_DECAY; g_test_synth_data.note_active = 1;
     g_test_synth_data.amplitude = 0.8; g_test_synth_data.decayTime = 0.1;
     g_test_synth_data.sustainLevel = 0.25; g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_DECAY);
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0);
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.decayTime);
     CU_ASSERT(is_decreasing()); 
     CU_ASSERT(get_max_abs_output() > (g_test_synth_data.amplitude * g_test_synth_data.sustainLevel));
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_decay_to_sustain_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_DECAY; g_test_synth_data.note_active = 1;
     g_test_synth_data.decayTime = 0.001; g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5; g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     CU_ASSERT(g_test_synth_data.timeInStage < (TEST_BUFFER_SIZE / TEST_SAMPLE_RATE));
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_sustain_level(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SINE; g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1; g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     CU_ASSERT_DOUBLE_EQUAL(get_max_abs_output(), g_test_synth_data.amplitude * g_test_synth_data.sustainLevel, TOLERANCE);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_release_ramp(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_RELEASE; g_test_synth_data.note_active = 1;
     g_test_synth_data.releaseTime = 0.1; g_test_synth_data.lastEnvValue = 0.4;
     g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_RELEASE);
     CU_ASSERT_EQUAL(g_test_synth_data.note_active, 1);
     CU_ASSERT(g_test_synth_data.timeInStage > 0.0);
     CU_ASSERT(g_test_synth_data.timeInStage < g_test_synth_data.releaseTime);
     CU_ASSERT(is_decreasing()); 
     CU_ASSERT(get_max_abs_output() <= g_test_synth_data.lastEnvValue * (1.0 + TOLERANCE) );
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_adsr_release_to_idle_transition(void) {
     setup_default_synth_data();
     g_test_synth_data.currentStage = ENV_RELEASE; g_test_synth_data.note_active = 1;
     g_test_synth_data.releaseTime = 0.001; g_test_synth_data.lastEnvValue = 0.4;
     g_test_synth_data.timeInStage = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
     CU_ASSERT_EQUAL(g_test_synth_data.note_active, 0);
     CU_ASSERT_DOUBLE_EQUAL(g_test_output_buffer[TEST_BUFFER_SIZE - 1], 0.0, 1e-6);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 // --- Test Waveforms (Wave 1 only, Wave 2 silent) ---
 void test_w1_waveform_sine(void) { test_w1_adsr_sustain_level(); }

 void test_w1_waveform_square(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SQUARE; g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1; g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     int transitions = 0; float expected_val = g_test_synth_data.amplitude;
     for (int k = 0; k < TEST_BUFFER_SIZE; ++k) {
         CU_ASSERT_DOUBLE_EQUAL(fabsf(g_test_output_buffer[k]), expected_val, TOLERANCE);
         if (k > 0 && (g_test_output_buffer[k] * g_test_output_buffer[k-1] < 0)) { transitions++; }
     }
     CU_ASSERT(transitions > 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_waveform_sawtooth(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SAWTOOTH; g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1; g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     int drops = 0;
     for (int k = 1; k < TEST_BUFFER_SIZE; ++k) {
         if (g_test_output_buffer[k] < g_test_output_buffer[k-1] - 0.1) { drops++; }
         CU_ASSERT(fabsf(g_test_output_buffer[k]) <= g_test_synth_data.amplitude * (1.0 + TOLERANCE));
     }
     CU_ASSERT(drops > 0);
     CU_ASSERT(get_max_abs_output() > 0.0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 void test_w1_waveform_triangle(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_TRIANGLE; g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1; g_test_synth_data.amplitude = 0.6;
     g_test_synth_data.sustainLevel = 1.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     int peaks = 0, valleys = 0;
     for (int k = 1; k < TEST_BUFFER_SIZE - 1; ++k) {
         if (g_test_output_buffer[k] > g_test_output_buffer[k-1] && g_test_output_buffer[k] > g_test_output_buffer[k+1]) { peaks++; }
         if (g_test_output_buffer[k] < g_test_output_buffer[k-1] && g_test_output_buffer[k] < g_test_output_buffer[k+1]) { valleys++; }
         CU_ASSERT(fabsf(g_test_output_buffer[k]) <= g_test_synth_data.amplitude * (1.0 + TOLERANCE));
     }
     CU_ASSERT(peaks > 0 || valleys > 0);
     CU_ASSERT(get_max_abs_output() > 0.0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_IDLE);
 }

 // ============================================
 // ==          Wave 2 Tests                  ==
 // ============================================
 void setup_wave2_active(void) {
     setup_default_synth_data();
     g_test_synth_data.amplitude = 0.0; g_test_synth_data.note_active = 0; g_test_synth_data.currentStage = ENV_IDLE;
     g_test_synth_data.frequency2 = 330.0; g_test_synth_data.amplitude2 = 0.7;
     g_test_synth_data.waveform2 = WAVE_SAWTOOTH; g_test_synth_data.attackTime2 = 0.05;
     g_test_synth_data.decayTime2 = 0.15; g_test_synth_data.sustainLevel2 = 0.6;
     g_test_synth_data.releaseTime2 = 0.25; g_test_synth_data.note_active2 = 1;
     g_test_synth_data.currentStage2 = ENV_ATTACK; g_test_synth_data.timeInStage2 = 0.0;
 }

 void test_w2_adsr_attack_ramp(void) {
     setup_wave2_active();
     g_test_synth_data.attackTime2 = 0.1;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_ATTACK);
     CU_ASSERT(g_test_synth_data.timeInStage2 > 0.0);
     CU_ASSERT(g_test_synth_data.timeInStage2 < g_test_synth_data.attackTime2);
     CU_ASSERT(is_increasing()); // Use new helper
     CU_ASSERT(get_max_abs_output() < g_test_synth_data.amplitude2 * 0.9);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
 }

 void test_w2_adsr_sustain_level(void) {
     setup_wave2_active();
     g_test_synth_data.waveform2 = WAVE_SINE; g_test_synth_data.currentStage2 = ENV_SUSTAIN;
     g_test_synth_data.amplitude2 = 0.7; g_test_synth_data.sustainLevel2 = 0.6;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_SUSTAIN);
     CU_ASSERT_DOUBLE_EQUAL(get_max_abs_output(), g_test_synth_data.amplitude2 * g_test_synth_data.sustainLevel2, TOLERANCE);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
 }

 void test_w2_adsr_release_ramp(void) {
     setup_wave2_active();
     g_test_synth_data.currentStage2 = ENV_RELEASE; g_test_synth_data.releaseTime2 = 0.1;
     g_test_synth_data.lastEnvValue2 = 0.3; g_test_synth_data.timeInStage2 = 0.0;
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_RELEASE);
     CU_ASSERT_EQUAL(g_test_synth_data.note_active2, 1);
     CU_ASSERT(g_test_synth_data.timeInStage2 > 0.0);
     CU_ASSERT(g_test_synth_data.timeInStage2 < g_test_synth_data.releaseTime2);
     CU_ASSERT(is_decreasing()); // Use new helper
     CU_ASSERT(get_max_abs_output() <= g_test_synth_data.lastEnvValue2 * (1.0 + TOLERANCE));
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_IDLE);
 }

 // ============================================
 // ==          Mixing Tests                  ==
 // ============================================
 void test_mixing_two_sines_sustain(void) {
     setup_default_synth_data();
     g_test_synth_data.waveform = WAVE_SINE; g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.note_active = 1; g_test_synth_data.amplitude = 0.5;
     g_test_synth_data.sustainLevel = 0.8; // W1 sustain = 0.4
     g_test_synth_data.waveform2 = WAVE_SINE; g_test_synth_data.currentStage2 = ENV_SUSTAIN;
     g_test_synth_data.note_active2 = 1; g_test_synth_data.amplitude2 = 0.3;
     g_test_synth_data.sustainLevel2 = 1.0; // W2 sustain = 0.3
     g_test_synth_data.frequency2 = g_test_synth_data.frequency * 1.5;
     double expected_peak_approx = (g_test_synth_data.amplitude * g_test_synth_data.sustainLevel) +
                                   (g_test_synth_data.amplitude2 * g_test_synth_data.sustainLevel2);
     int result = paCallback(NULL, g_test_output_buffer, TEST_BUFFER_SIZE, NULL, 0, &g_test_synth_data);
     CU_ASSERT_EQUAL(result, 0);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage, ENV_SUSTAIN);
     CU_ASSERT_EQUAL(g_test_synth_data.currentStage2, ENV_SUSTAIN);
     CU_ASSERT_DOUBLE_EQUAL(get_max_abs_output(), expected_peak_approx, TOLERANCE * 2.5);
     CU_ASSERT(get_max_abs_output() > g_test_synth_data.amplitude * g_test_synth_data.sustainLevel);
     CU_ASSERT(get_max_abs_output() > g_test_synth_data.amplitude2 * g_test_synth_data.sustainLevel2);
 }

 // --- Main Test Runner Function ---
 int main() {
     CU_pSuite pSuite = NULL;
     if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
     pSuite = CU_add_suite("Audio_Callback_Tests_DualWave", init_audio_suite, clean_audio_suite);
     if (NULL == pSuite) { CU_cleanup_registry(); return CU_get_error(); }

     if ( (NULL == CU_add_test(pSuite, "test_adsr_idle_both_waves", test_adsr_idle_both_waves)) ||
          (NULL == CU_add_test(pSuite, "test_w1_adsr_attack_ramp", test_w1_adsr_attack_ramp)) ||
          (NULL == CU_add_test(pSuite, "test_mixing_two_sines_sustain", test_mixing_two_sines_sustain))
        )
     { CU_cleanup_registry(); return CU_get_error(); }

     CU_basic_set_mode(CU_BRM_VERBOSE);
     CU_basic_run_tests();
     printf("\n");
     CU_basic_show_failures(CU_get_failure_list());
     printf("\n\n");
     unsigned int failures = CU_get_number_of_failures();
     CU_cleanup_registry();
     return (failures > 0) ? 1 : 0;
 }