/**
 * @file test_gui_helpers.c
 * @brief Unit tests for helper functions defined in gui.c using the CUnit framework.
 *
 * Currently focuses on testing the calculate_current_envelope() function, which
 * determines the envelope amplitude based on the current ADSR state.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include <pthread.h> // For mutex type in SharedSynthData
 
 #include <CUnit/Basic.h>
 
 #include "../synthesizer/synth_data.h" // Need the data structure definition 
 #include "../synthesizer/gui.h"     // Need the calculate_current_envelope declaration 
 
 // --- Test Globals ---
 /** @brief Mock sample rate (not used by calculate_current_envelope but part of struct). */
 #define TEST_SAMPLE_RATE 44100.0
 /** @brief Global instance of shared data used as input for the function under test. */
 SharedSynthData g_test_synth_data;
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
  * @brief Resets the global `g_test_synth_data` structure to default values
  * before each test function runs.
  */
 void setup_test_data(void) {
     // Set typical values for testing
     g_test_synth_data = (SharedSynthData){
         .frequency = 440.0, .amplitude = 0.8, .waveform = WAVE_SINE,
         .attackTime = 0.1, .decayTime = 0.2, .sustainLevel = 0.5, .releaseTime = 0.3,
         .phase = 0.0, .note_active = 0, .sampleRate = TEST_SAMPLE_RATE,
         .currentStage = ENV_IDLE, .timeInStage = 0.0, .lastEnvValue = 0.0,
         .waveform_drawing_area = NULL
     };
 }
 
 // --- Test Functions for calculate_current_envelope ---
 
 /**
  * @brief Test calculate_current_envelope when the stage is ENV_IDLE.
  * Expected result is 0.0.
  */
 void test_calc_env_idle(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_IDLE;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /**
  * @brief Test calculate_current_envelope at the very start of the ATTACK phase (time = 0).
  * Expected result is 0.0.
  */
 void test_calc_env_attack_start(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.timeInStage = 0.0;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
 }
 
 /**
  * @brief Test calculate_current_envelope halfway through the ATTACK phase.
  * Expected result follows the linear ramp: amplitude * (timeInStage / attackTime).
  */
 void test_calc_env_attack_mid(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.attackTime = 0.1;
     g_test_synth_data.timeInStage = 0.05; // Halfway
     g_test_synth_data.amplitude = 0.8;
     double expected = g_test_synth_data.amplitude * (g_test_synth_data.timeInStage / g_test_synth_data.attackTime);
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /**
  * @brief Test calculate_current_envelope exactly at the end of the ATTACK phase.
  * Expected result is the full amplitude.
  */
 void test_calc_env_attack_end(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.attackTime = 0.1;
     g_test_synth_data.timeInStage = 0.1; // End
     g_test_synth_data.amplitude = 0.8;
     double expected = g_test_synth_data.amplitude;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /**
  * @brief Test calculate_current_envelope after the ATTACK phase should have ended.
  * Expected result is clamped at the full amplitude.
  */
 void test_calc_env_attack_past_end(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.attackTime = 0.1;
     g_test_synth_data.timeInStage = 0.15; // Past end
     g_test_synth_data.amplitude = 0.8;
     double expected = g_test_synth_data.amplitude; // Clamped
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /**
  * @brief Test calculate_current_envelope with zero attack time (instant attack).
  * Expected result is the full amplitude immediately.
  */
 void test_calc_env_attack_zero_time(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_ATTACK;
     g_test_synth_data.attackTime = 0.0;
     g_test_synth_data.timeInStage = 0.0;
     g_test_synth_data.amplitude = 0.8;
     double expected = g_test_synth_data.amplitude;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /**
  * @brief Test calculate_current_envelope at the start of the DECAY phase (time = 0).
  * Expected result is the full amplitude (as it just finished attack).
  */
 void test_calc_env_decay_start(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.timeInStage = 0.0;
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double expected = g_test_synth_data.amplitude;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.8
 }
 
 /**
  * @brief Test calculate_current_envelope halfway through the DECAY phase.
  * Expected result follows the linear ramp towards the sustain level.
  */
 void test_calc_env_decay_mid(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.decayTime = 0.2;
     g_test_synth_data.timeInStage = 0.1; // Halfway
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double decay_factor = g_test_synth_data.timeInStage / g_test_synth_data.decayTime;
     double expected = g_test_synth_data.amplitude * (1.0 - (1.0 - g_test_synth_data.sustainLevel) * decay_factor);
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.6
 }
 
 /**
  * @brief Test calculate_current_envelope exactly at the end of the DECAY phase.
  * Expected result is the sustain level multiplied by amplitude.
  */
 void test_calc_env_decay_end(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.decayTime = 0.2;
     g_test_synth_data.timeInStage = 0.2; // End
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double expected = g_test_synth_data.amplitude * g_test_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /**
  * @brief Test calculate_current_envelope after the DECAY phase should have ended.
  * Expected result is clamped at the sustain level.
  */
 void test_calc_env_decay_past_end(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.decayTime = 0.2;
     g_test_synth_data.timeInStage = 0.3; // Past end
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double expected = g_test_synth_data.amplitude * g_test_synth_data.sustainLevel; // Clamped
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /**
  * @brief Test calculate_current_envelope with zero decay time (instant decay).
  * Expected result is the sustain level immediately.
  */
 void test_calc_env_decay_zero_time(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_DECAY;
     g_test_synth_data.decayTime = 0.0;
     g_test_synth_data.timeInStage = 0.0;
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double expected = g_test_synth_data.amplitude * g_test_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /**
  * @brief Test calculate_current_envelope during the SUSTAIN phase.
  * Expected result is the sustain level multiplied by amplitude, regardless of timeInStage.
  */
 void test_calc_env_sustain(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_SUSTAIN;
     g_test_synth_data.timeInStage = 1.0; // Time doesn't matter
     g_test_synth_data.amplitude = 0.8;
     g_test_synth_data.sustainLevel = 0.5;
     double expected = g_test_synth_data.amplitude * g_test_synth_data.sustainLevel;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, expected, TOLERANCE); // Expect 0.4
 }
 
 /**
  * @brief Test calculate_current_envelope when the stage is ENV_RELEASE.
  * Expected result is 0.0, as this function calculates the value *before* release starts.
  */
 void test_calc_env_release(void) {
     setup_test_data();
     g_test_synth_data.currentStage = ENV_RELEASE;
     double result = calculate_current_envelope(&g_test_synth_data);
     CU_ASSERT_DOUBLE_EQUAL(result, 0.0, TOLERANCE);
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
     pSuite = CU_add_suite("GUI_Helper_Tests", init_gui_helper_suite, clean_gui_helper_suite);
     if (NULL == pSuite) {
         CU_cleanup_registry();
         return CU_get_error();
     }
 
     // Add the tests to the suite
     if ((NULL == CU_add_test(pSuite, "test_calc_env_idle", test_calc_env_idle)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_attack_start", test_calc_env_attack_start)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_attack_mid", test_calc_env_attack_mid)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_attack_end", test_calc_env_attack_end)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_attack_past_end", test_calc_env_attack_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_attack_zero_time", test_calc_env_attack_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_decay_start", test_calc_env_decay_start)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_decay_mid", test_calc_env_decay_mid)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_decay_end", test_calc_env_decay_end)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_decay_past_end", test_calc_env_decay_past_end)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_decay_zero_time", test_calc_env_decay_zero_time)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_sustain", test_calc_env_sustain)) ||
         (NULL == CU_add_test(pSuite, "test_calc_env_release", test_calc_env_release))
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
 