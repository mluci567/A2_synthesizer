/**
 * @file test_audio_lifecycle.c
 * @brief Unit tests for the audio lifecycle functions (init, start, stop, terminate)
 * using the CMocka mocking framework and C preprocessor redirection.
 *
 * This file tests the functions in audio.c by linking against audio.o_test
 * and using C preprocessor defines to redirect calls from audio.c to the
 * mock implementations below. This method is used for platforms where linker
 * wrapping (--wrap) is not supported (e.g., macOS).
 */

 #include <stdarg.h>
 #include <stddef.h>
 #include <setjmp.h>
 #include <stdint.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 
 #include <cmocka.h>
 #include <portaudio.h> // Include the REAL PortAudio header
 
 // --- Preprocessor Defines for Mocking ---
 // Redirect calls to the real PortAudio functions to our mock wrappers below.
 // These MUST come BEFORE including the header file that uses the real functions (audio.h).
 #define Pa_Initialize __wrap_Pa_Initialize
 #define Pa_Terminate __wrap_Pa_Terminate
 #define Pa_GetDefaultOutputDevice __wrap_Pa_GetDefaultOutputDevice
 #define Pa_GetDeviceInfo __wrap_Pa_GetDeviceInfo
 #define Pa_GetVersionInfo __wrap_Pa_GetVersionInfo
 #define Pa_OpenDefaultStream __wrap_Pa_OpenDefaultStream
 #define Pa_StartStream __wrap_Pa_StartStream
 #define Pa_StopStream __wrap_Pa_StopStream
 #define Pa_CloseStream __wrap_Pa_CloseStream
 #define Pa_GetErrorText __wrap_Pa_GetErrorText
 
 // --- Now include project headers ---
 #include "../synth/synth_data.h"
 #include "../synth/audio.h" // Include header for function prototypes under test
 
 // NO direct include of audio.c
 
 // --- Mock Wrapper Implementations ---
 // Implement the mock logic within functions named __wrap_FunctionName
 
 /** @brief Mock implementation FOR Pa_Initialize. Uses CMocka expectations. */
 PaError __wrap_Pa_Initialize(void) {
     check_expected(mock());
     return (PaError)mock();
 }
 
 /** @brief Mock implementation FOR Pa_Terminate. Uses CMocka expectations. */
 PaError __wrap_Pa_Terminate(void) {
     check_expected(mock());
     return (PaError)mock();
 }
 
 /** @brief Mock implementation FOR Pa_GetDefaultOutputDevice. Uses CMocka expectations. */
 PaDeviceIndex __wrap_Pa_GetDefaultOutputDevice(void) {
     check_expected(mock());
     return (PaDeviceIndex)mock();
 }
 
 /** @brief Static mock device info instance returned by mock Pa_GetDeviceInfo. */
 static const PaDeviceInfo mock_device_info = {
     .structVersion = 1, .name = "Mock Device", .hostApi = 0, .maxInputChannels = 0,
     .maxOutputChannels = 2, .defaultLowInputLatency = 0.0, .defaultLowOutputLatency = 0.01,
     .defaultHighInputLatency = 0.0, .defaultHighOutputLatency = 0.1, .defaultSampleRate = 44100.0
 };
 /** @brief Mock implementation FOR Pa_GetDeviceInfo. Uses CMocka expectations. */
 const PaDeviceInfo* __wrap_Pa_GetDeviceInfo(PaDeviceIndex device) {
     check_expected(device);
     return (const PaDeviceInfo*)mock();
 }
 
 /** @brief Static mock version info instance returned by mock Pa_GetVersionInfo. */
 static const PaVersionInfo mock_version_info = {
     .versionMajor = 19, .versionMinor = 7, .versionSubMinor = 0,
     .versionControlRevision = "mock_control_rev", .versionText = "Mock PortAudio V19"
 };
 /** @brief Mock implementation FOR Pa_GetVersionInfo. */
 const PaVersionInfo* __wrap_Pa_GetVersionInfo(void) {
     return &mock_version_info;
 }
 
 /** @brief Mock PaStream pointer value used to simulate an active stream. */
 #define MOCK_PA_STREAM ((PaStream*)0xDEADBEEF)
 
 /** @brief Mock implementation FOR Pa_OpenDefaultStream. Uses CMocka expectations. */
 PaError __wrap_Pa_OpenDefaultStream( PaStream** stream,
                                      int numInputChannels, int numOutputChannels,
                                      PaSampleFormat sampleFormat, double sampleRate,
                                      unsigned long framesPerBuffer,
                                      PaStreamCallback *streamCallback, void *userData ) {
     check_expected(stream);
     check_expected(numInputChannels);
     check_expected(numOutputChannels);
     check_expected(sampleRate);
     check_expected(userData);
     PaError result = (PaError)mock();
     if (result == paNoError) *stream = MOCK_PA_STREAM;
     else *stream = NULL;
     return result;
 }
 
 /** @brief Mock implementation FOR Pa_StartStream. Uses CMocka expectations. */
 PaError __wrap_Pa_StartStream(PaStream *stream) {
     check_expected(stream);
     return (PaError)mock();
 }
 
 /** @brief Mock implementation FOR Pa_StopStream. Uses CMocka expectations. */
 PaError __wrap_Pa_StopStream(PaStream *stream) {
     check_expected(stream);
     return (PaError)mock();
 }
 
 /** @brief Mock implementation FOR Pa_CloseStream. Uses CMocka expectations. */
 PaError __wrap_Pa_CloseStream(PaStream *stream) {
     check_expected(stream);
     return (PaError)mock();
 }
 
 /** @brief Mock implementation FOR Pa_GetErrorText. */
 const char* __wrap_Pa_GetErrorText(PaError err) {
     if (err == paNoError) return "No Error";
     if (err == paInternalError) return "Internal Error";
     if (err == paDeviceUnavailable) return "Device Unavailable";
     return "Unknown Mock Error";
 }
 
 
 // --- Test Globals ---
 SharedSynthData g_test_synth_data;
 // We still need to simulate the internal static g_paStream for some tests.
 // Making it non-static here allows tests to influence setup if needed,
 // although direct testing of its state is less feasible now.
 PaStream *g_paStream = NULL;
 
 
 // --- Test Setup/Teardown Functions ---
 static int setup(void **state) {
     g_test_synth_data = (SharedSynthData){
         .sampleRate = 44100.0, .currentStage = ENV_ATTACK, .timeInStage = 1.0,
         .lastEnvValue = 0.5, .currentStage2 = ENV_DECAY, .timeInStage2 = 1.5,
         .lastEnvValue2 = 0.25,
     };
     if (pthread_mutex_init(&g_test_synth_data.mutex, NULL) != 0) {
         fprintf(stderr, "Failed to init mutex in test setup\n");
         return -1;
     }
     g_paStream = NULL;
     return 0;
 }
 
 static int teardown(void **state) {
     g_paStream = NULL;
     pthread_mutex_destroy(&g_test_synth_data.mutex);
     return 0;
 }
 
 // --- Test Cases ---
 // These test cases remain largely the same. They call the real functions
 // from audio.o_test, but the preprocessor redirects the PortAudio calls
 // within them to the __wrap_ implementations above.
 
 static void test_initialize_audio_success(void **state) {
     expect_function_call(__wrap_Pa_Initialize); // Expect call to wrapper
     will_return(__wrap_Pa_Initialize, paNoError);
 
     PaError result = initialize_audio(&g_test_synth_data); // Calls real function
 
     assert_int_equal(result, paNoError);
     assert_int_equal(g_test_synth_data.currentStage, ENV_IDLE);
     assert_float_equal(g_test_synth_data.timeInStage, 0.0, 1e-9);
     assert_float_equal(g_test_synth_data.lastEnvValue, 0.0, 1e-9);
     assert_int_equal(g_test_synth_data.currentStage2, ENV_IDLE);
     assert_float_equal(g_test_synth_data.timeInStage2, 0.0, 1e-9);
     assert_float_equal(g_test_synth_data.lastEnvValue2, 0.0, 1e-9);
 }
 
 static void test_initialize_audio_fail(void **state) {
     expect_function_call(__wrap_Pa_Initialize);
     will_return(__wrap_Pa_Initialize, paInternalError);
     PaError result = initialize_audio(&g_test_synth_data);
     assert_int_equal(result, paInternalError);
 }
 
 static void test_start_audio_success(void **state) {
     PaDeviceIndex defaultDevice = 0;
     expect_function_call(__wrap_Pa_GetDefaultOutputDevice);
     will_return(__wrap_Pa_GetDefaultOutputDevice, defaultDevice);
     expect_value(__wrap_Pa_GetDeviceInfo, device, defaultDevice);
     will_return(__wrap_Pa_GetDeviceInfo, &mock_device_info);
     expect_any(__wrap_Pa_OpenDefaultStream, stream);
     expect_value(__wrap_Pa_OpenDefaultStream, numInputChannels, 0);
     expect_value(__wrap_Pa_OpenDefaultStream, numOutputChannels, 1);
     expect_value(__wrap_Pa_OpenDefaultStream, sampleRate, g_test_synth_data.sampleRate);
     expect_value(__wrap_Pa_OpenDefaultStream, userData, &g_test_synth_data);
     will_return(__wrap_Pa_OpenDefaultStream, paNoError);
     expect_value(__wrap_Pa_StartStream, stream, MOCK_PA_STREAM);
     will_return(__wrap_Pa_StartStream, paNoError);
 
     PaError result = start_audio(&g_test_synth_data); // Calls real function
 
     assert_int_equal(result, paNoError);
 }
 
 static void test_start_audio_no_device(void **state) {
     expect_function_call(__wrap_Pa_GetDefaultOutputDevice);
     will_return(__wrap_Pa_GetDefaultOutputDevice, paNoDevice);
     PaError result = start_audio(&g_test_synth_data);
     assert_int_equal(result, paDeviceUnavailable);
 }
 
 static void test_start_audio_open_fail(void **state) {
     PaDeviceIndex defaultDevice = 0;
     expect_function_call(__wrap_Pa_GetDefaultOutputDevice);
     will_return(__wrap_Pa_GetDefaultOutputDevice, defaultDevice);
     expect_value(__wrap_Pa_GetDeviceInfo, device, defaultDevice);
     will_return(__wrap_Pa_GetDeviceInfo, &mock_device_info);
     expect_any(__wrap_Pa_OpenDefaultStream, stream);
     expect_any(__wrap_Pa_OpenDefaultStream, numInputChannels);
     expect_any(__wrap_Pa_OpenDefaultStream, numOutputChannels);
     expect_any(__wrap_Pa_OpenDefaultStream, sampleRate);
     expect_any(__wrap_Pa_OpenDefaultStream, userData);
     will_return(__wrap_Pa_OpenDefaultStream, paInternalError);
 
     PaError result = start_audio(&g_test_synth_data);
 
     assert_int_equal(result, paInternalError);
 }
 
 // Similar adjustments for stop/terminate tests - expect calls to __wrap_ functions
 static void test_stop_audio_success(void **state) {
     // Simulate internal state by calling start_audio first
     PaDeviceIndex defaultDevice = 0;
     expect_function_call(__wrap_Pa_GetDefaultOutputDevice); will_return(__wrap_Pa_GetDefaultOutputDevice, defaultDevice);
     expect_value(__wrap_Pa_GetDeviceInfo, device, defaultDevice); will_return(__wrap_Pa_GetDeviceInfo, &mock_device_info);
     expect_any(__wrap_Pa_OpenDefaultStream, stream); expect_any(__wrap_Pa_OpenDefaultStream, numInputChannels); expect_any(__wrap_Pa_OpenDefaultStream, numOutputChannels); expect_any(__wrap_Pa_OpenDefaultStream, sampleRate); expect_any(__wrap_Pa_OpenDefaultStream, userData);
     will_return(__wrap_Pa_OpenDefaultStream, paNoError);
     expect_value(__wrap_Pa_StartStream, stream, MOCK_PA_STREAM); will_return(__wrap_Pa_StartStream, paNoError);
     start_audio(&g_test_synth_data); // Call real start
 
     // Set expectations for stop
     expect_value(__wrap_Pa_StopStream, stream, MOCK_PA_STREAM); // Expect internal value
     will_return(__wrap_Pa_StopStream, paNoError);
     expect_value(__wrap_Pa_CloseStream, stream, MOCK_PA_STREAM); // Expect internal value
     will_return(__wrap_Pa_CloseStream, paNoError);
 
     PaError result = stop_audio(); // Call real stop
 
     assert_int_equal(result, paNoError);
 }
 
 static void test_stop_audio_already_stopped(void **state) {
     // No PortAudio functions should be called via defines if internal stream is NULL
     PaError result = stop_audio();
     assert_int_equal(result, paNoError);
 }
 
 static void test_terminate_audio_success_stream_null(void **state) {
     expect_function_call(__wrap_Pa_Terminate);
     will_return(__wrap_Pa_Terminate, paNoError);
     PaError result = terminate_audio();
     assert_int_equal(result, paNoError);
 }
 
 static void test_terminate_audio_success_stream_active(void **state) {
     // Simulate internal state by calling start_audio first
     PaDeviceIndex defaultDevice = 0;
     expect_function_call(__wrap_Pa_GetDefaultOutputDevice); will_return(__wrap_Pa_GetDefaultOutputDevice, defaultDevice);
     expect_value(__wrap_Pa_GetDeviceInfo, device, defaultDevice); will_return(__wrap_Pa_GetDeviceInfo, &mock_device_info);
     expect_any(__wrap_Pa_OpenDefaultStream, stream); expect_any(__wrap_Pa_OpenDefaultStream, numInputChannels); expect_any(__wrap_Pa_OpenDefaultStream, numOutputChannels); expect_any(__wrap_Pa_OpenDefaultStream, sampleRate); expect_any(__wrap_Pa_OpenDefaultStream, userData);
     will_return(__wrap_Pa_OpenDefaultStream, paNoError);
     expect_value(__wrap_Pa_StartStream, stream, MOCK_PA_STREAM); will_return(__wrap_Pa_StartStream, paNoError);
     start_audio(&g_test_synth_data); // Call real start
 
     expect_value(__wrap_Pa_StopStream, stream, MOCK_PA_STREAM);
     will_return(__wrap_Pa_StopStream, paNoError);
     expect_value(__wrap_Pa_CloseStream, stream, MOCK_PA_STREAM);
     will_return(__wrap_Pa_CloseStream, paNoError);
     expect_function_call(__wrap_Pa_Terminate);
     will_return(__wrap_Pa_Terminate, paNoError);
 
     PaError result = terminate_audio();
 
     assert_int_equal(result, paNoError);
 }
 
 static void test_terminate_audio_fail(void **state) {
     expect_function_call(__wrap_Pa_Terminate);
     will_return(__wrap_Pa_Terminate, paInternalError);
     PaError result = terminate_audio();
     assert_int_equal(result, paInternalError);
 }
 
 // --- Main Test Runner ---
 int main(void) {
     const struct CMUnitTest tests[] = {
         cmocka_unit_test_setup_teardown(test_initialize_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_initialize_audio_fail, setup, teardown),
         cmocka_unit_test_setup_teardown(test_start_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_start_audio_no_device, setup, teardown),
         cmocka_unit_test_setup_teardown(test_start_audio_open_fail, setup, teardown),
         cmocka_unit_test_setup_teardown(test_stop_audio_success, setup, teardown),
         cmocka_unit_test_setup_teardown(test_stop_audio_already_stopped, setup, teardown),
         // Add tests for stop_audio failures here if needed
         cmocka_unit_test_setup_teardown(test_terminate_audio_success_stream_null, setup, teardown),
         cmocka_unit_test_setup_teardown(test_terminate_audio_success_stream_active, setup, teardown),
         cmocka_unit_test_setup_teardown(test_terminate_audio_fail, setup, teardown),
     };
     return cmocka_run_group_tests(tests, NULL, NULL);
 }