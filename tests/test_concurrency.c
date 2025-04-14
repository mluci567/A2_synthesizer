/**
 * @file test_concurrency.c
 * @brief Basic concurrency tests for shared synth data access using CUnit and pthreads.
 *
 * This file tests basic concurrent read/write access to the global g_synth_data
 * structure protected by its mutex.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <unistd.h> 
 #include <CUnit/Basic.h>
 
 #include "../synth/synth_data.h"
 
 // --- Test Globals & Setup ---
 
 // Make the global accessible for the test (extern if needed, or define locally for test scope)
 // If running this test requires the actual global instance from main.c, linking might be complex.
 // For simplicity, this example defines a local instance for the test scope.
 SharedSynthData g_test_synth_data_concurrency;
 
 // Number of iterations for threads
 #define NUM_ITERATIONS 10000
 // Simple shared counter within the test data for basic race detection
 volatile int g_shared_counter = 0;
 
 /**
  * @brief Test suite initialization. Initializes synth data and mutex.
  * @return 0 on success, -1 on failure.
  */
 int init_concurrency_suite(void) {
     g_test_synth_data_concurrency = (SharedSynthData){
         .frequency = 100.0, .amplitude = 0.5, .frequency2 = 200.0, .amplitude2 = 0.5,
         .sampleRate = 44100.0
     };
     g_shared_counter = 0;
 
     // Initialize the mutex
     int ret = pthread_mutex_init(&g_test_synth_data_concurrency.mutex, NULL);
     if (ret != 0) {
         fprintf(stderr, "Failed to initialize mutex for concurrency test: %s\n", strerror(ret));
         return -1; // CUnit setup failure
     }
     return 0; // CUnit setup success
 }
 
 /**
  * @brief Test suite cleanup. Destroys the mutex.
  * @return 0 on success.
  */
 int clean_concurrency_suite(void) {
     pthread_mutex_destroy(&g_test_synth_data_concurrency.mutex);
     return 0; // CUnit teardown success
 }
 
 // --- Thread Functions ---
 
 /**
  * @brief Thread function simulating GUI updates (writing data).
  */
 void *gui_thread_simulator(void *arg) {
     for (int i = 0; i < NUM_ITERATIONS; ++i) {
         int ret_lock = pthread_mutex_lock(&g_test_synth_data_concurrency.mutex);
         CU_ASSERT_EQUAL_FATAL(ret_lock, 0); // Fail test if lock fails
 
         // Simulate writing data
         g_test_synth_data_concurrency.frequency += 0.1;
         if (g_test_synth_data_concurrency.frequency > 1000.0) {
             g_test_synth_data_concurrency.frequency = 100.0;
         }
         g_shared_counter++; // Increment shared counter under lock
 
         int ret_unlock = pthread_mutex_unlock(&g_test_synth_data_concurrency.mutex);
         CU_ASSERT_EQUAL_FATAL(ret_unlock, 0); // Fail test if unlock fails
 
         // usleep(rand() % 50); // Requires <unistd.h> and seeding rand()
     }
     return NULL;
 }
 
 /**
  * @brief Thread function simulating audio callback access (reading/writing data).
  */
 void *audio_thread_simulator(void *arg) {
     double local_freq_sum = 0; // Accumulate a value read under lock
 
     for (int i = 0; i < NUM_ITERATIONS; ++i) {
         int ret_lock = pthread_mutex_lock(&g_test_synth_data_concurrency.mutex);
         CU_ASSERT_EQUAL_FATAL(ret_lock, 0);
 
         // Simulate reading data
         local_freq_sum += g_test_synth_data_concurrency.frequency;
         // Simulate writing state (like phase - simple increment here)
         g_test_synth_data_concurrency.phase += 0.01;
         g_shared_counter++; // Increment shared counter under lock
 
         int ret_unlock = pthread_mutex_unlock(&g_test_synth_data_concurrency.mutex);
         CU_ASSERT_EQUAL_FATAL(ret_unlock, 0);
 
         // usleep(rand() % 50);
     }
     // Pass back the accumulated value (just for demo purposes)
     double *result = malloc(sizeof(double));
     if (result) *result = local_freq_sum;
     return result; // Needs to be freed by the joining thread
 }
 
 
 // --- Test Case ---
 
 /**
  * @brief Creates two threads that concurrently access g_test_synth_data_concurrency.
  * Checks if the test completes without deadlock and if a shared counter was updated correctly.
  */
 void test_concurrent_access(void) {
     pthread_t gui_tid, audio_tid;
     void *audio_result_ptr = NULL;
     int ret_gui, ret_audio;
 
     // Create the threads
     ret_gui = pthread_create(&gui_tid, NULL, gui_thread_simulator, NULL);
     CU_ASSERT_EQUAL(ret_gui, 0);
     ret_audio = pthread_create(&audio_tid, NULL, audio_thread_simulator, NULL);
     CU_ASSERT_EQUAL(ret_audio, 0);
 
     // Wait for threads to complete
     if (ret_gui == 0) {
         pthread_join(gui_tid, NULL);
     }
     if (ret_audio == 0) {
         pthread_join(audio_tid, &audio_result_ptr);
     }
 
     // --- Assertions ---
     // 1. Basic Check: Did the test run to completion (implies no deadlock)?
     //    (Implicitly checked by reaching this point if threads were created)
     CU_PASS("Threads completed execution (no deadlock detected).");
 
     // 2. Check shared counter: Simple check for potential lost updates.
     //    Each thread increments NUM_ITERATIONS times.
     CU_ASSERT_EQUAL(g_shared_counter, NUM_ITERATIONS * 2);
 
     if (audio_result_ptr) {
         // double final_sum = *(double*)audio_result_ptr;
         // printf("Debug: Audio thread accumulated frequency sum: %f\n", final_sum);
         free(audio_result_ptr); // Free the allocated memory
     }
 }
 
 // --- Main Test Runner Function (Example - Adapt to your makefile structure) ---
 
 /*
  * This main function is only for standalone testing example.
  * In your project, you'll add the suite and test to your existing CUnit
  * registry setup (likely managed by your makefile and a central test runner).
  */
 int main() {
    CU_pSuite pSuite = NULL;
 
    // Initialize CUnit registry
    if (CUE_SUCCESS != CU_initialize_registry())
       return CU_get_error();
 
    // Add suite
    pSuite = CU_add_suite("Concurrency_Tests", init_concurrency_suite, clean_concurrency_suite);
    if (NULL == pSuite) {
       CU_cleanup_registry();
       return CU_get_error();
    }
 
    // Add test case
    if ((NULL == CU_add_test(pSuite, "test_concurrent_access", test_concurrent_access)))
    {
       CU_cleanup_registry();
       return CU_get_error();
    }
 
    // Run tests using the Basic interface
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    printf("\n");
    CU_basic_show_failures(CU_get_failure_list());
    printf("\n\n");
 
    // Get number of failures
    unsigned int failures = CU_get_number_of_failures();
 
    // Cleanup registry
    CU_cleanup_registry();
 
    // Return failure indicator
    return (failures > 0) ? 1 : 0;
 }