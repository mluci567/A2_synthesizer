/**
 * @file test_gui_helpers.c
 * @brief Unit tests for helper functions defined in gui.c using the CUnit framework.
 *
 * Focuses on testing the calculate_current_envelope() and calculate_current_envelope_wave2()
 * functions, which determine the envelope amplitude based on the current ADSR state for
 * each respective wave.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include <pthread.h> 
 #include <CUnit/Basic.h>
 
 #include "../synth/synth_data.h" 
 #include "../synth/gui.h"   
 
 #ifdef TESTING
 /**
  * @brief Declaration of the Wave 2 envelope calculation helper function for testing.
  * @param[in] data Pointer to the shared synthesizer data structure.
  * @return The calculated envelope multiplier for Wave 2 based on the state in `data`.
  */
 double calculate_current_envelope_wave2(const SharedSynthData *data);
 #endif
 
 // --- Test Globals ---
 /** @brief Mock sample rate (not used by calculate_current_envelope but part of struct). */
 #define TEST_SAMPLE_RATE 44100.0
 
 /**
  * @brief Definition of g_synth_data for the test scope.
  * This satisfies the linker's need for the symbol referenced by gui.c functions,
  * even though the functions under test here don't use the global directly.
  */
 SharedSynthData g_synth_data; // *** FIXED: Renamed from g_test_synth_data ***
 
 /** @brief Tolerance used for comparing double-precision floating-point values in assertions. */
 const double TOLERANCE = 1e-6;
 
 // --- Test Suite Setup/Teardown ---
 
 /**
  * @brief Initialization function for the GUI Helper test suite.
  * Currently does nothing.
  * @return 0 on success.
  */
 int init_gui_helper_suite(void) {
     // No specific setup needed for this suite
     return 0; // Success
 }
 
 /**
  * @brief Cleanup function for the GUI Helper test suite.
  * Currently does nothing.
  * @return 0 on success.
  */
 int clean_gui_helper_suite(void) {
     // No specific cleanup needed
     return 0; // Success
 }
 
 // --- Helper Function ---
 
 /**
  * @brief Resets the global `g_synth_data` structure to default values
  * for both waves before each test function runs.
  */
 void setup_test_data(void) {
     // Set typical values for testing, including Wave 2 defaults
     g_synth_data = (SharedSynthData){ 
         // Wave 1
         .frequency = 440.0, .amplitude = 0.8, .waveform = WAVE_SINE,
         .attackTime = 0.1, .decayTime = 0.2, .sustainLevel = 0.5, .releaseTime = 0.3,
         .phase = 0.0, .note_active = 0,
         .currentStage = ENV_IDLE, .timeInStage = 0.0, .lastEnvValue = 0.0,
         // Wave 2
         .frequency2 = 660.0, .amplitude2 = 0.6, .waveform2 = WAVE_SQUARE,
         .attackTime2 = 0.05, .decayTime2 = 0.15, .sustainLevel2 = 0.7, .releaseTime2 = 0.4,
         .phase2 = 0.0, .note_active2 = 0,
         .currentStage2 = ENV_IDLE, .timeInStage2 = 0.0, .lastEnvValue2 = 0.0,
         // Common
         .sampleRate = TEST_SAMPLE_RATE,
         .waveform_drawing_area = NULL // This will be NULL for tests, which is fine
     };
     // No need to initialize mutex here as these functions don't use it
 }
 
 // ============================================================
 // --- Test Functions for calculate_current_envelope (Wave 1) ---
 // ============================================================
 
 /** @brief Test Wave 1 calc env: ENV_IDLE. */
 void test_w1_calc_env_idle(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_IDLE;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /** @brief Test Wave 1 calc env: ENV_ATTACK start. */
 void test_w1_calc_env_attack_start(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_ATTACK;
     g_synth_data.timeInStage = 0.0;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /** @brief Test Wave 1 calc env: ENV_ATTACK mid-point. */
 void test_w1_calc_env_attack_mid(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_ATTACK;
     g_synth_data.attackTime = 0.1;
     g_synth_data.timeInStage = 0.05; // Halfway
     g_synth_data.amplitude = 0.8;
     double expected = g_synth_data.amplitude * (g_synth_data.timeInStage / g_synth_data.attackTime);
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /** @brief Test Wave 1 calc env: ENV_ATTACK end. */
 void test_w1_calc_env_attack_end(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_ATTACK;
     g_synth_data.attackTime = 0.1;
     g_synth_data.timeInStage = 0.1; // End
     g_synth_data.amplitude = 0.8;
     double expected = g_synth_data.amplitude;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /** @brief Test Wave 1 calc env: ENV_ATTACK past end (clamped). */
 void test_w1_calc_env_attack_past_end(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_ATTACK;
     g_synth_data.attackTime = 0.1;
     g_synth_data.timeInStage = 0.15; // Past end
     g_synth_data.amplitude = 0.8;
     double expected = g_synth_data.amplitude; // Clamped
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /** @brief Test Wave 1 calc env: ENV_ATTACK zero time. */
 void test_w1_calc_env_attack_zero_time(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_ATTACK;
     g_synth_data.attackTime = 0.0;
     g_synth_data.timeInStage = 0.0;
     g_synth_data.amplitude = 0.8;
     double expected = g_synth_data.amplitude;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /** @brief Test Wave 1 calc env: ENV_DECAY start. */
 void test_w1_calc_env_decay_start(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_DECAY;
     g_synth_data.timeInStage = 0.0;
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double expected = g_synth_data.amplitude;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /** @brief Test Wave 1 calc env: ENV_DECAY mid-point. */
 void test_w1_calc_env_decay_mid(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_DECAY;
     g_synth_data.decayTime = 0.2;
     g_synth_data.timeInStage = 0.1; // Halfway
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double decay_factor = g_synth_data.timeInStage / g_synth_data.decayTime;
     double expected = g_synth_data.amplitude * (1.0 - (1.0 - g_synth_data.sustainLevel) * decay_factor);
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /** @brief Test Wave 1 calc env: ENV_DECAY end. */
 void test_w1_calc_env_decay_end(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_DECAY;
     g_synth_data.decayTime = 0.2;
     g_synth_data.timeInStage = 0.2; // End
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double expected = g_synth_data.amplitude * g_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /** @brief Test Wave 1 calc env: ENV_DECAY past end (clamped). */
 void test_w1_calc_env_decay_past_end(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_DECAY;
     g_synth_data.decayTime = 0.2;
     g_synth_data.timeInStage = 0.3; // Past end
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double expected = g_synth_data.amplitude * g_synth_data.sustainLevel; // Clamped
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /** @brief Test Wave 1 calc env: ENV_DECAY zero time. */
 void test_w1_calc_env_decay_zero_time(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_DECAY;
     g_synth_data.decayTime = 0.0;
     g_synth_data.timeInStage = 0.0;
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double expected = g_synth_data.amplitude * g_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /** @brief Test Wave 1 calc env: ENV_SUSTAIN. */
 void test_w1_calc_env_sustain(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_SUSTAIN;
     g_synth_data.timeInStage = 1.0; // Time doesn't matter
     g_synth_data.amplitude = 0.8;
     g_synth_data.sustainLevel = 0.5;
     double expected = g_synth_data.amplitude * g_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /** @brief Test Wave 1 calc env: ENV_RELEASE. */
 void test_w1_calc_env_release(void) {
     setup_test_data();
     g_synth_data.currentStage = ENV_RELEASE;
     double result = calculate_current_envelope(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE); // Expect 0.0 as function calculates pre-release value
 }
 
 
 // =============================================================
 // --- Test Functions for calculate_current_envelope_wave2 ---
 // =============================================================
 
 /** @brief Test Wave 2 calc env: ENV_IDLE. */
 void test_w2_calc_env_idle(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_IDLE;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /** @brief Test Wave 2 calc env: ENV_ATTACK start. */
 void test_w2_calc_env_attack_start(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_ATTACK;
     g_synth_data.timeInStage2 = 0.0;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /** @brief Test Wave 2 calc env: ENV_ATTACK mid-point. */
 void test_w2_calc_env_attack_mid(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_ATTACK;
     g_synth_data.attackTime2 = 0.1; // Use W2 param
     g_synth_data.timeInStage2 = 0.05; // Halfway, use W2 state
     g_synth_data.amplitude2 = 0.6; // Use W2 param
     double expected = g_synth_data.amplitude2 * (g_synth_data.timeInStage2 / g_synth_data.attackTime2);
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.3
 }
 
 /** @brief Test Wave 2 calc env: ENV_ATTACK end. */
 void test_w2_calc_env_attack_end(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_ATTACK;
     g_synth_data.attackTime2 = 0.1;
     g_synth_data.timeInStage2 = 0.1; // End
     g_synth_data.amplitude2 = 0.6;
     double expected = g_synth_data.amplitude2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /** @brief Test Wave 2 calc env: ENV_ATTACK past end (clamped). */
 void test_w2_calc_env_attack_past_end(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_ATTACK;
     g_synth_data.attackTime2 = 0.1;
     g_synth_data.timeInStage2 = 0.15; // Past end
     g_synth_data.amplitude2 = 0.6;
     double expected = g_synth_data.amplitude2; // Clamped
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /** @brief Test Wave 2 calc env: ENV_ATTACK zero time. */
 void test_w2_calc_env_attack_zero_time(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_ATTACK;
     g_synth_data.attackTime2 = 0.0;
     g_synth_data.timeInStage2 = 0.0;
     g_synth_data.amplitude2 = 0.6;
     double expected = g_synth_data.amplitude2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /** @brief Test Wave 2 calc env: ENV_DECAY start. */
 void test_w2_calc_env_decay_start(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_DECAY;
     g_synth_data.timeInStage2 = 0.0;
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double expected = g_synth_data.amplitude2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /** @brief Test Wave 2 calc env: ENV_DECAY mid-point. */
 void test_w2_calc_env_decay_mid(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_DECAY;
     g_synth_data.decayTime2 = 0.2;
     g_synth_data.timeInStage2 = 0.1; // Halfway
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double decay_factor = g_synth_data.timeInStage2 / g_synth_data.decayTime2;
     double expected = g_synth_data.amplitude2 * (1.0 - (1.0 - g_synth_data.sustainLevel2) * decay_factor);
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6 * (1 - (1-0.7)*0.5) = 0.6 * (1-0.15) = 0.6 * 0.85 = 0.51
 }
 
 /** @brief Test Wave 2 calc env: ENV_DECAY end. */
 void test_w2_calc_env_decay_end(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_DECAY;
     g_synth_data.decayTime2 = 0.2;
     g_synth_data.timeInStage2 = 0.2; // End
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double expected = g_synth_data.amplitude2 * g_synth_data.sustainLevel2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6 * 0.7 = 0.42
 }
 
 /** @brief Test Wave 2 calc env: ENV_DECAY past end (clamped). */
 void test_w2_calc_env_decay_past_end(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_DECAY;
     g_synth_data.decayTime2 = 0.2;
     g_synth_data.timeInStage2 = 0.3; // Past end
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double expected = g_synth_data.amplitude2 * g_synth_data.sustainLevel2; // Clamped
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.42
 }
 
 /** @brief Test Wave 2 calc env: ENV_DECAY zero time. */
 void test_w2_calc_env_decay_zero_time(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_DECAY;
     g_synth_data.decayTime2 = 0.0;
     g_synth_data.timeInStage2 = 0.0;
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double expected = g_synth_data.amplitude2 * g_synth_data.sustainLevel2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.42
 }
 
 /** @brief Test Wave 2 calc env: ENV_SUSTAIN. */
 void test_w2_calc_env_sustain(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_SUSTAIN;
     g_synth_data.timeInStage2 = 1.0; // Time doesn't matter
     g_synth_data.amplitude2 = 0.6;
     g_synth_data.sustainLevel2 = 0.7;
     double expected = g_synth_data.amplitude2 * g_synth_data.sustainLevel2;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.42
 }
 
 /** @brief Test Wave 2 calc env: ENV_RELEASE. */
 void test_w2_calc_env_release(void) {
     setup_test_data();
     g_synth_data.currentStage2 = ENV_RELEASE;
     double result = calculate_current_envelope_wave2(&g_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE); // Expect 0.0 as function calculates pre-release value
 }
 
 
 // --- Main Test Runner Function ---
 
 /**
  * @brief Main entry point for the GUI helper test suite.
  * Initializes the CUnit registry, adds the test suite and associated test cases,
  * runs the tests using the basic console interface, and reports results.
  * @return 0 if all tests pass, non-zero otherwise.
  */
 int main() {
     CU_pSuite pSuite = NULL;
 
     // Initialize the CUnit test registry
     if (CUE_SUCCESS != CU_initialize_registry())
         return CU_get_error();
 
     // Add a suite to the registry
     pSuite = CU_add_suite("GUI_Helper_Tests_DualWave", init_gui_helper_suite, clean_gui_helper_suite);
     if (NULL == pSuite) {
         CU_cleanup_registry();
         return CU_get_error();
     }
 
     // Add the Wave 1 tests to the suite
     if ((NULL == CU_add_test(pSuite, "test_w1_calc_env_idle", test_w1_calc_env_idle)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_attack_start", test_w1_calc_env_attack_start)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_attack_mid", test_w1_calc_env_attack_mid)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_attack_end", test_w1_calc_env_attack_end)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_attack_past_end", test_w1_calc_env_attack_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_attack_zero_time", test_w1_calc_env_attack_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_decay_start", test_w1_calc_env_decay_start)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_decay_mid", test_w1_calc_env_decay_mid)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_decay_end", test_w1_calc_env_decay_end)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_decay_past_end", test_w1_calc_env_decay_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_decay_zero_time", test_w1_calc_env_decay_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_sustain", test_w1_calc_env_sustain)) ||
         (NULL == CU_add_test(pSuite, "test_w1_calc_env_release", test_w1_calc_env_release)) ||
     // Add the Wave 2 tests to the suite
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_idle", test_w2_calc_env_idle)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_attack_start", test_w2_calc_env_attack_start)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_attack_mid", test_w2_calc_env_attack_mid)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_attack_end", test_w2_calc_env_attack_end)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_attack_past_end", test_w2_calc_env_attack_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_attack_zero_time", test_w2_calc_env_attack_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_decay_start", test_w2_calc_env_decay_start)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_decay_mid", test_w2_calc_env_decay_mid)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_decay_end", test_w2_calc_env_decay_end)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_decay_past_end", test_w2_calc_env_decay_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_decay_zero_time", test_w2_calc_env_decay_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_sustain", test_w2_calc_env_sustain)) ||
         (NULL == CU_add_test(pSuite, "test_w2_calc_env_release", test_w2_calc_env_release))
        )
     {
         CU_cleanup_registry();
         return CU_get_error();
     }
 
     // Run all tests using the CUnit Basic interface
     CU_basic_set_mode(CU_BRM_VERBOSE); // Set output mode (VERBOSE shows passes)
     CU_basic_run_tests();             // Run the tests
     printf("\n");
     CU_basic_show_failures(CU_get_failure_list()); // Print summary of failures
     printf("\n\n");
 
     // Get number of failures to return appropriate exit code
     unsigned int failures = CU_get_number_of_failures();
 
     // Cleanup CUnit registry
     CU_cleanup_registry();
 
     // Return 1 if any tests failed, 0 otherwise (useful for makefiles/CI)
     return (failures > 0) ? 1 : 0;
 }