/**
 * @file test_audio_lifecycle.c
 * @brief Unit tests for the audio lifecycle functions (init, start, stop, terminate)
 * using the CMocka mocking framework.
 *
 * This file tests the functions in audio.c that interact with the PortAudio library
 * by providing mock implementations of the PortAudio API calls. It verifies that
 * the lifecycle functions call the appropriate PortAudio functions and handle
 * expected success and failure scenarios.
 *
 * @note This test file includes audio.c directly to allow testing of its logic
 * and interaction with its static global variable `g_paStream` via the mocks.
 */

 #include <stdarg.h>
 #include <stddef.h>
 #include <setjmp.h>
 #include <stdint.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h> // For strcmp used in audio.c's error macro
 
 // CMocka header for unit testing framework
 #include <cmocka.h>
 
 // --- Mock PortAudio Definitions ---
 // Define necessary PortAudio types and error codes used by audio.c.
 // These replace the actual definitions from portaudio.h for this test environment.
 
 /** @brief Mocked PaError type (usually int). */
 typedef int PaError;
 /** @brief Mocked paNoError code. */
 #define paNoError 0
 /** @brief Mocked paInternalError code. */
 #define paInternalError -10000
 /** @brief Mocked paDeviceUnavailable code. */
 #define paDeviceUnavailable -9985
 /** @brief Mocked paStreamIsStopped code (status, not error). */
 #define paStreamIsStopped 1
 
 /** @brief Mocked opaque PaStream pointer type. */
 typedef void PaStream;
 /** @brief Mocked PaDeviceIndex type (usually int). */
 typedef int PaDeviceIndex;
 /** @brief Mocked paNoDevice index. */
 #define paNoDevice ((PaDeviceIndex)-1)
 
 /** @brief Mocked PaStreamParameters struct (simplified). */
 typedef struct PaStreamParameters {
     PaDeviceIndex device; ///< Mocked device index field.
 } PaStreamParameters;
 
 /** @brief Mocked PaDeviceInfo struct (simplified). */
 typedef struct PaDeviceInfo {
     const char *name;                   ///< Mocked device name.
     double defaultLowOutputLatency;     ///< Mocked low latency value.
     double defaultHighOutputLatency;    ///< Mocked high latency value.
 } PaDeviceInfo;
 
 /** @brief Mocked PaVersionInfo struct (simplified). */
 typedef struct PaVersionInfo {
     const char *versionText; ///< Mocked version string.
 } PaVersionInfo;
 
 /** @brief Mocked PaStreamCallback function signature type. */
 typedef int PaStreamCallback(
     const void *input, void *output, unsigned long frameCount,
     const void* timeInfo, // Simplified type for mock
     unsigned long statusFlags, // Simplified type for mock
     void *userData);
 
 // --- Mocked PortAudio Functions ---
 // Redefine PortAudio functions using CMocka's mocking capabilities.
 
 /** @brief Mock implementation of Pa_Initialize. Uses CMocka expectations. */
 PaError Pa_Initialize(void) {
     check_expected(mock()); // Expect call, no args checked here
     return (PaError)mock(); // Return value set by test case
 }
 
 /** @brief Mock implementation of Pa_Terminate. Uses CMocka expectations. */
 PaError Pa_Terminate(void) {
     check_expected(mock());
     return (PaError)mock();
 }
 
 /** @brief Mock implementation of Pa_GetDefaultOutputDevice. Uses CMocka expectations. */
 PaDeviceIndex Pa_GetDefaultOutputDevice(void) {
     check_expected(mock());
     return (PaDeviceIndex)mock();
 }
 
 /** @brief Static mock device info instance returned by mock Pa_GetDeviceInfo. */
 static const PaDeviceInfo mock_device_info = {"Mock Device", 0.01, 0.1};
 /** @brief Mock implementation of Pa_GetDeviceInfo. Uses CMocka expectations. */
 const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex device) {
     check_expected(device); // Expect the device index argument
     return (const PaDeviceInfo*)mock(); // Return value set by test case (e.g., &mock_device_info or NULL)
 }
 
 /** @brief Static mock version info instance returned by mock Pa_GetVersionInfo. */
 static const PaVersionInfo mock_version_info = {"Mock PortAudio V19"};
 /** @brief Mock implementation of Pa_GetVersionInfo. */
 const PaVersionInfo* Pa_GetVersionInfo(void) {
     // No expectations needed typically, just return valid pointer for printf
     return &mock_version_info;
 }
 
 /** @brief Mock PaStream pointer value used to simulate an active stream. */
 #define MOCK_PA_STREAM ((PaStream*)0xDEADBEEF)
 
 /** @brief Mock implementation of Pa_OpenDefaultStream. Uses CMocka expectations. */
 PaError Pa_OpenDefaultStream(PaStream** stream,
                              int numInputChannels, int numOutputChannels,
                              int sampleFormat, // Simplified type
                              double sampleRate,
                              unsigned long framesPerBuffer,
                              PaStreamCallback *streamCallback,
                              void *userData) {
     // Check expected arguments passed from start_audio
     check_expected(stream);
     check_expected(numInputChannels);
     check_expected(numOutputChannels);
     check_expected(sampleRate);
     check_expected(userData);
 
     // Simulate setting the stream pointer based on mock return value
     PaError result = (PaError)mock(); // Get return value from test case
     if (result == paNoError) {
         *stream = MOCK_PA_STREAM; // Simulate success by setting stream pointer
     } else {
         *stream = NULL;
     }
     return result;
 }
 
 /** @brief Mock implementation of Pa_StartStream. Uses CMocka expectations. */
 PaError Pa_StartStream(PaStream *stream) {
     check_expected(stream); // Expect the stream pointer argument
     return (PaError)mock();
 }
 
 /** @brief Mock implementation of Pa_StopStream. Uses CMocka expectations. */
 PaError Pa_StopStream(PaStream *stream) {
     check_expected(stream);
     return (PaError)mock();
 }
 
 /** @brief Mock implementation of Pa_CloseStream. Uses CMocka expectations. */
 PaError Pa_CloseStream(PaStream *stream) {
     check_expected(stream);
     return (PaError)mock();
 }
 
 /** @brief Mock implementation of Pa_GetErrorText. */
 const char* Pa_GetErrorText(PaError err) {
     // Simple mock, returns basic strings for common errors
     if (err == paNoError) return "No Error";
     if (err == paInternalError) return "Internal Error";
     if (err == paDeviceUnavailable) return "Device Unavailable";
     return "Unknown Mock Error";
 }
 
 
 // --- Include the C file to be tested AFTER mocks are defined ---
 /**
  * @note Including audio.c directly here allows the test harness to intercept
  * calls to PortAudio functions made within audio.c using the mocks
  * defined above. It also gives the test access to the static g_paStream.
  */
 #include "../synthesizer/synth_data.h"
 #include "../synthesizer/audio.h" // Include header for function prototypes under test
 #include "../synthesizer/audio.c" // Include the implementation!
 
 // --- Test Globals ---
 /**
  * @var g_test_synth_data
  * @brief Global instance of shared data used by the functions under test.
  * @note Initialized in the setup() function.
  */
 SharedSynthData g_test_synth_data;
 
 // --- Test Setup/Teardown Functions ---
 
 /**
  * @brief Setup function called before each test case in this group.
  * Initializes the global `g_test_synth_data` and its mutex.
  * Resets the mocked global `g_paStream` to NULL.
  * @param state Unused CMocka state pointer.
  * @return 0 on success, -1 on failure (e.g., mutex init fails).
  */
 static int setup(void **state) {
     // Reset global synth data before each test
     g_test_synth_data = (SharedSynthData){
         .sampleRate = 44100.0,
         // Other fields can be default as lifecycle functions don't use them much
     };
     // Initialize mutex (required by initialize_audio, start_audio)
     if (pthread_mutex_init(&g_test_synth_data.mutex, NULL) != 0) {
         fprintf(stderr, "Failed to init mutex in test setup\n");
         return -1; // Indicate setup failure
     }
     // Reset the mocked global stream pointer before each test
     g_paStream = NULL;
     return 0; // Success
 }
 
 /**
  * @brief Teardown function called after each test case in this group.
  * Destroys the mutex in `g_test_synth_data`.
  * Resets the mocked global `g_paStream`.
  * @param state Unused CMocka state pointer.
  * @return 0 on success.
  */
 static int teardown(void **state) {
     // Ensure stream is reset if test failed mid-way (optional but safe)
     g_paStream = NULL;
     pthread_mutex_destroy(&g_test_synth_data.mutex);
     return 0; // Success
 }
 
 // --- Test Cases ---
 
 /**
  * @brief Tests successful initialization of PortAudio via initialize_audio().
  * @param state Unused CMocka state pointer.
  */
 static void test_initialize_audio_success(void **state) {
     // Set expectations: Pa_Initialize should be called and return paNoError
     expect_function_call(Pa_Initialize);
     will_return(Pa_Initialize, paNoError);
 
     // Call the function under test
     PaError result = initialize_audio(&g_test_synth_data);
 
     // Assertions: Check return value
     assert_int_equal(result, paNoError);
     // CMocka automatically verifies Pa_Initialize was called as expected
 }
 
 /**
  * @brief Tests handling of PortAudio initialization failure in initialize_audio().
  * @param state Unused CMocka state pointer.
  */
 static void test_initialize_audio_fail(void **state) {
     // Set expectations: Pa_Initialize should be called and return an error
     expect_function_call(Pa_Initialize);
     will_return(Pa_Initialize, paInternalError);
 
     // Call the function under test
     PaError result = initialize_audio(&g_test_synth_data);
 
     // Assertions: Check return value matches the mocked error
     assert_int_equal(result, paInternalError);
 }
 
 /**
  * @brief Tests the successful path for starting the audio stream via start_audio().
  * @param state Unused CMocka state pointer.
  */
 static void test_start_audio_success(void **state) {
     PaDeviceIndex defaultDevice = 0; // Simulate a valid device index
 
     // Set expectations for the sequence of calls in start_audio
     expect_function_call(Pa_GetDefaultOutputDevice);
     will_return(Pa_GetDefaultOutputDevice, defaultDevice);
 
     expect_value(Pa_GetDeviceInfo, device, defaultDevice);
     will_return(Pa_GetDeviceInfo, &mock_device_info); // Return valid mock device info
 
     expect_any(Pa_OpenDefaultStream, stream); // Expect call, don't care about address of ptr
     expect_value(Pa_OpenDefaultStream, numInputChannels, 0);
     expect_value(Pa_OpenDefaultStream, numOutputChannels, 1); // Mono
     expect_value(Pa_OpenDefaultStream, sampleRate, g_test_synth_data.sampleRate);
     expect_value(Pa_OpenDefaultStream, userData, &g_test_synth_data); // Expect pointer to our test data
     will_return(Pa_OpenDefaultStream, paNoError); // Simulate successful open
 
     expect_value(Pa_StartStream, stream, MOCK_PA_STREAM); // Expect call with the mock stream ptr
     will_return(Pa_StartStream, paNoError); // Simulate successful start
 
     // Call the function under test
     PaError result = start_audio(&g_test_synth_data);
 
     // Assertions
     assert_int_equal(result, paNoError);
     assert_ptr_equal(g_paStream, MOCK_PA_STREAM); // Verify global stream pointer was set
 }
 
 /**
  * @brief Tests start_audio() when no default output device is found.
  * @param state Unused CMocka state pointer.
  */
 static void test_start_audio_no_device(void **state) {
     // Set expectations: Pa_GetDefaultOutputDevice returns paNoDevice
     expect_function_call(Pa_GetDefaultOutputDevice);
     will_return(Pa_GetDefaultOutputDevice, paNoDevice);
 
     // Call the function under test
     PaError result = start_audio(&g_test_synth_data);
 
     // Assertions
     assert_int_equal(result, paDeviceUnavailable); // Expect specific error code
     assert_null(g_paStream); // Stream should not have been opened/set
 }
 
 /**
  * @brief Tests start_audio() when opening the stream fails.
  * @param state Unused CMocka state pointer.
  */
 static void test_start_audio_open_fail(void **state) {
     PaDeviceIndex defaultDevice = 0;
     // Set expectations for successful device checks
     expect_function_call(Pa_GetDefaultOutputDevice);
     will_return(Pa_GetDefaultOutputDevice, defaultDevice);
     expect_value(Pa_GetDeviceInfo, device, defaultDevice);
     will_return(Pa_GetDeviceInfo, &mock_device_info);
 
     // Set expectations for Pa_OpenDefaultStream to fail
     expect_any(Pa_OpenDefaultStream, stream);
     expect_any(Pa_OpenDefaultStream, numInputChannels);
     expect_any(Pa_OpenDefaultStream, numOutputChannels);
     expect_any(Pa_OpenDefaultStream, sampleRate);
     expect_any(Pa_OpenDefaultStream, userData);
     will_return(Pa_OpenDefaultStream, paInternalError); // Simulate open failure
 
     // Call the function under test
     PaError result = start_audio(&g_test_synth_data);
 
     // Assertions
     assert_int_equal(result, paInternalError); // Expect the error from Pa_OpenDefaultStream
     assert_null(g_paStream); // Stream should not have been set
 }
 
 
 /**
  * @brief Tests successful stopping of an active audio stream via stop_audio().
  * @param state Unused CMocka state pointer.
  */
 static void test_stop_audio_success(void **state) {
     // Simulate stream being active before the call
     g_paStream = MOCK_PA_STREAM;
 
     // Set expectations for Pa_StopStream and Pa_CloseStream
     expect_value(Pa_StopStream, stream, MOCK_PA_STREAM);
     will_return(Pa_StopStream, paNoError);
     expect_value(Pa_CloseStream, stream, MOCK_PA_STREAM);
     will_return(Pa_CloseStream, paNoError);
 
     // Call the function under test
     PaError result = stop_audio();
 
     // Assertions
     assert_int_equal(result, paNoError);
     assert_null(g_paStream); // Verify global stream pointer was reset
 }
 
 /**
  * @brief Tests stop_audio() when the stream is already stopped (g_paStream is NULL).
  * @param state Unused CMocka state pointer.
  */
 static void test_stop_audio_already_stopped(void **state) {
     // Ensure stream is inactive
     g_paStream = NULL;
 
     // No PortAudio functions should be called in this case.
     // CMocka will automatically fail the test if any unexpected calls occur.
 
     // Call the function under test
     PaError result = stop_audio();
 
     // Assertions
     assert_int_equal(result, paNoError);
     assert_null(g_paStream); // Should remain NULL
 }
 
 /**
  * @brief Tests stop_audio() when Pa_StopStream fails but Pa_CloseStream succeeds.
  * @param state Unused CMocka state pointer.
  */
 static void test_stop_audio_stop_fails(void **state) {
     // Simulate active stream
     g_paStream = MOCK_PA_STREAM;
 
     // Set expectations: Stop fails, Close succeeds
     expect_value(Pa_StopStream, stream, MOCK_PA_STREAM);
     will_return(Pa_StopStream, paInternalError); // Simulate Stop failure
     expect_value(Pa_CloseStream, stream, MOCK_PA_STREAM);
     will_return(Pa_CloseStream, paNoError); // Simulate Close success
 
     // Call the function under test
     PaError result = stop_audio();
 
     // Assertions: stop_audio logs the stop error but returns the result of close
     assert_int_equal(result, paNoError);
     assert_null(g_paStream); // Should be NULL because close was attempted and succeeded
 }
 
 /**
  * @brief Tests stop_audio() when Pa_CloseStream fails.
  * @param state Unused CMocka state pointer.
  */
 static void test_stop_audio_close_fails(void **state) {
     // Simulate active stream
     g_paStream = MOCK_PA_STREAM;
 
     // Set expectations: Stop succeeds, Close fails
     expect_value(Pa_StopStream, stream, MOCK_PA_STREAM);
     will_return(Pa_StopStream, paNoError);
     expect_value(Pa_CloseStream, stream, MOCK_PA_STREAM);
     will_return(Pa_CloseStream, paInternalError); // Simulate Close failure
 
     // Call the function under test
     PaError result = stop_audio();
 
     // Assertions: Should return the error from Pa_CloseStream
     assert_int_equal(result, paInternalError);
     assert_null(g_paStream); // Should be set to NULL even if close fails
 }
 
 
 /**
  * @brief Tests successful termination via terminate_audio() when stream is already NULL.
  * @param state Unused CMocka state pointer.
  */
 static void test_terminate_audio_success_stream_null(void **state) {
     // Ensure stream is inactive
     g_paStream = NULL;
 
     // Set expectations: Only Pa_Terminate should be called
     expect_function_call(Pa_Terminate);
     will_return(Pa_Terminate, paNoError);
 
     // Call the function under test
     PaError result = terminate_audio();
 
     // Assertions
     assert_int_equal(result, paNoError);
 }
 
 /**
  * @brief Tests successful termination via terminate_audio() when stream is active.
  * Verifies that stop_audio() logic (mocked calls) is invoked first.
  * @param state Unused CMocka state pointer.
  */
 static void test_terminate_audio_success_stream_active(void **state) {
     // Simulate stream being active
     g_paStream = MOCK_PA_STREAM;
 
     // Set expectations for the implicit stop_audio() call within terminate_audio()
     expect_value(Pa_StopStream, stream, MOCK_PA_STREAM);
     will_return(Pa_StopStream, paNoError);
     expect_value(Pa_CloseStream, stream, MOCK_PA_STREAM);
     will_return(Pa_CloseStream, paNoError);
 
     // Set expectation for the final Pa_Terminate call
     expect_function_call(Pa_Terminate);
     will_return(Pa_Terminate, paNoError);
 
     // Call the function under test
     PaError result = terminate_audio();
 
     // Assertions
     assert_int_equal(result, paNoError);
     assert_null(g_paStream); // Verify stream pointer is NULL after stop_audio runs
 }
 
 /**
  * @brief Tests handling of PortAudio termination failure in terminate_audio().
  * @param state Unused CMocka state pointer.
  */
 static void test_terminate_audio_fail(void **state) {
     // Ensure stream is inactive
     g_paStream = NULL;
 
     // Set expectations: Pa_Terminate fails
     expect_function_call(Pa_Terminate);
     will_return(Pa_Terminate, paInternalError);
 
     // Call the function under test
     PaError result = terminate_audio();
 
     // Assertions
     assert_int_equal(result, paInternalError);
 }
 
 
 // --- Main Test Runner ---
 
 /**
  * @brief Main entry point for the audio lifecycle test suite.
  * Sets up the CMocka test cases and runs them.
  * @return Result of the test run (0 on success, non-zero on failure).
  */
 int main(void) {
     // Array defining the tests to be run
     const struct CMUnitTest tests[] = {
         // initialize_audio tests
         cmocka_unit_test_setup_teardown(test_initialize_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_initialize_audio_fail, setup, teardown),
         // start_audio tests
         cmocka_unit_test_setup_teardown(test_start_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_start_audio_no_device, setup, teardown),
         cmocka_unit_test_setup_teardown(test_start_audio_open_fail, setup, teardown),
         // stop_audio tests
         cmocka_unit_test_setup_teardown(test_stop_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_stop_audio_already_stopped, setup, teardown),
         cmocka_unit_test_setup_teardown(test_stop_audio_stop_fails, setup, teardown),
         cmocka_unit_test_setup_teardown(test_stop_audio_close_fails, setup, teardown),
         // terminate_audio tests
         cmocka_unit_test_setup_teardown(test_terminate_audio_success_stream_null, setup, teardown),
         cmocka_unit_test_setup_teardown(test_terminate_audio_success_stream_active, setup, teardown),
         cmocka_unit_test_setup_teardown(test_terminate_audio_fail, setup, teardown),
     };
 
     // Run the test group
     return cmocka_run_group_tests(tests, NULL, NULL);
 } 