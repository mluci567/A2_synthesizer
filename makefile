# Makefile for Synthesizer with Unit Tests

# Compiler
CC = gcc

# Project Name
TARGET = synthesizer

# --- Source Files & Objects for Main Application ---
SYNTH_DIR = synth
SRCS = $(SYNTH_DIR)/main.c $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/audio.c
OBJS = $(SRCS:.c=.o)

# --- Compiler and Linker Flags for Main Application ---
# -pthread is needed for compiling AND linking with pthreads
CFLAGS = -Wall -g -pthread
# Include Paths (using pkg-config)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
PORTAUDIO_CFLAGS = $(shell pkg-config --cflags portaudio-2.0)
CFLAGS += $(GTK_CFLAGS) $(PORTAUDIO_CFLAGS)
# Linker Flags (using pkg-config)
# -pthread is needed here for linking
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
PORTAUDIO_LIBS = $(shell pkg-config --libs portaudio-2.0)
LIBS = $(GTK_LIBS) $(PORTAUDIO_LIBS) -lm -lpthread

# --- Testing Specific Definitions ---
TEST_DIR = tests

# Test Suite 1: Audio Callback Logic (CUnit)
TEST_AUDIO_CALLBACK_SRC = $(TEST_DIR)/test_audio.c
TEST_AUDIO_CALLBACK_OBJ = $(TEST_AUDIO_CALLBACK_SRC:.c=.o)
TEST_AUDIO_CALLBACK_RUNNER = test_runner_audio_callback
AUDIO_OBJ_FOR_TEST = $(SYNTH_DIR)/audio.o_test # Object file compiled with -DTESTING

# Test Suite 2: GUI Helpers (CUnit)
TEST_GUI_HELPERS_SRC = $(TEST_DIR)/test_gui_helpers.c
TEST_GUI_HELPERS_OBJ = $(TEST_GUI_HELPERS_SRC:.c=.o)
TEST_GUI_HELPERS_RUNNER = test_runner_gui_helpers
GUI_OBJ_FOR_TEST = $(SYNTH_DIR)/gui.o_test # Object file compiled with -DTESTING

# Test Suite 3: Audio Lifecycle (CMocka)
TEST_AUDIO_LIFECYCLE_SRC = $(TEST_DIR)/test_audio_lifecycle.c
TEST_AUDIO_LIFECYCLE_OBJ = $(TEST_AUDIO_LIFECYCLE_SRC:.c=.o)
TEST_AUDIO_LIFECYCLE_RUNNER = test_runner_audio_lifecycle

# Test Suite 4: Concurrency (CUnit) 
TEST_CONCURRENCY_SRC = $(TEST_DIR)/test_concurrency.c
TEST_CONCURRENCY_OBJ = $(TEST_CONCURRENCY_SRC:.c=.o)
TEST_CONCURRENCY_RUNNER = test_runner_concurrency

# Common flags for compiling test code and project code *for* tests
CUNIT_CFLAGS = $(shell pkg-config --cflags cunit)
CMOCKA_CFLAGS = $(shell pkg-config --cflags cmocka)
# Include synth directory for synth_data.h etc.
TEST_CFLAGS = $(CFLAGS) -DTESTING -I$(SYNTH_DIR) $(CUNIT_CFLAGS) $(CMOCKA_CFLAGS)

# Libraries needed for testing
CUNIT_LIBS = $(shell pkg-config --libs cunit)
CMOCKA_LIBS = $(shell pkg-config --libs cmocka) # Use pkg-config if available
TEST_COMMON_LIBS = -lpthread -lm # Common libs needed by tested code or mocks

# --- Build Targets ---

# Default target: build the main application
all: $(TARGET)

# Rule to link the main application executable
$(TARGET): $(OBJS)
	@echo "Linking main application: $(TARGET)"
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LIBS)

# --- Rules for Compiling Main Application Object Files ---
$(SYNTH_DIR)/main.o: $(SYNTH_DIR)/main.c $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/audio.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SYNTH_DIR)/gui.o: $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/synth_data.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SYNTH_DIR)/audio.o: $(SYNTH_DIR)/audio.c $(SYNTH_DIR)/audio.h $(SYNTH_DIR)/synth_data.h
	$(CC) $(CFLAGS) -c $< -o $@


# --- Rules for Compiling Project Files *for Testing* ---
# These use TEST_CFLAGS

# Rule to compile audio.c for testing
$(AUDIO_OBJ_FOR_TEST): $(SYNTH_DIR)/audio.c $(SYNTH_DIR)/audio.h $(SYNTH_DIR)/synth_data.h
	@echo "Compiling audio.c for testing..."
	$(CC) $(TEST_CFLAGS) -c $(SYNTH_DIR)/audio.c -o $@

# Rule to compile gui.c for testing
$(GUI_OBJ_FOR_TEST): $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/synth_data.h
	@echo "Compiling gui.c for testing..."
	$(CC) $(TEST_CFLAGS) -c $(SYNTH_DIR)/gui.c -o $@


# --- Rules for Compiling Test Harnesses ---
# These use TEST_CFLAGS

# Rule to compile Test Suite 1 harness (Uses CUnit)
$(TEST_AUDIO_CALLBACK_OBJ): $(TEST_AUDIO_CALLBACK_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/audio.h
	@echo "Compiling test harness: $(TEST_AUDIO_CALLBACK_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Rule to compile Test Suite 2 harness (Uses CUnit)
$(TEST_GUI_HELPERS_OBJ): $(TEST_GUI_HELPERS_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/gui.h
	@echo "Compiling test harness: $(TEST_GUI_HELPERS_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Rule to compile Test Suite 3 harness (Uses CMocka)
$(TEST_AUDIO_LIFECYCLE_OBJ): $(TEST_AUDIO_LIFECYCLE_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/audio.h
	@echo "Compiling test harness: $(TEST_AUDIO_LIFECYCLE_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Rule to compile Test Suite 4 harness (Uses CUnit)
$(TEST_CONCURRENCY_OBJ): $(TEST_CONCURRENCY_SRC) $(SYNTH_DIR)/synth_data.h
	@echo "Compiling test harness: $(TEST_CONCURRENCY_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@


# --- Rules for Linking Test Runners ---

# Rule to link Test Suite 1 runner (Audio Callback - CUnit)
$(TEST_AUDIO_CALLBACK_RUNNER): $(TEST_AUDIO_CALLBACK_OBJ) $(AUDIO_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(PORTAUDIO_LIBS) $(TEST_COMMON_LIBS)

# Rule to link Test Suite 2 runner (GUI Helpers - CUnit)
$(TEST_GUI_HELPERS_RUNNER): $(TEST_GUI_HELPERS_OBJ) $(GUI_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(GTK_LIBS) $(TEST_COMMON_LIBS)

# Rule to link Test Suite 3 runner (Audio Lifecycle - CMocka)
$(TEST_AUDIO_LIFECYCLE_RUNNER): $(TEST_AUDIO_LIFECYCLE_OBJ) $(AUDIO_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CMOCKA_LIBS) $(PORTAUDIO_LIBS) $(TEST_COMMON_LIBS)

# Rule to link Test Suite 4 runner (Concurrency - CUnit) - NEW
# Note: This simple test doesn't need gui.o_test or audio.o_test, just the harness object.
# It requires CUnit and pthreads for linking.
$(TEST_CONCURRENCY_RUNNER): $(TEST_CONCURRENCY_OBJ)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(TEST_COMMON_LIBS)


# --- Main Test Target ---
# Depends on all test runners being built 
test: $(TEST_AUDIO_CALLBACK_RUNNER) $(TEST_GUI_HELPERS_RUNNER) $(TEST_AUDIO_LIFECYCLE_RUNNER) $(TEST_CONCURRENCY_RUNNER)
	@echo "\n--- Running Audio Callback Tests (CUnit) ---"
	./$(TEST_AUDIO_CALLBACK_RUNNER)
	@echo "\n--- Running GUI Helper Tests (CUnit) ---"
	./$(TEST_GUI_HELPERS_RUNNER)
	@echo "\n--- Running Audio Lifecycle Tests (CMocka) ---"
	./$(TEST_AUDIO_LIFECYCLE_RUNNER)
	@echo "\n--- Running Concurrency Tests (CUnit) ---" # Added execution
	./$(TEST_CONCURRENCY_RUNNER)
	@echo "\n--- All tests finished ---"


# --- Clean Target ---
# Remove all test-related artifacts 
clean:
	@echo "Cleaning build and test artifacts..."
	rm -f $(TARGET) $(OBJS) \
	      $(TEST_AUDIO_CALLBACK_RUNNER) $(TEST_AUDIO_CALLBACK_OBJ) $(AUDIO_OBJ_FOR_TEST) \
	      $(TEST_GUI_HELPERS_RUNNER) $(TEST_GUI_HELPERS_OBJ) $(GUI_OBJ_FOR_TEST) \
	      $(TEST_AUDIO_LIFECYCLE_RUNNER) $(TEST_AUDIO_LIFECYCLE_OBJ) \
	      $(TEST_CONCURRENCY_RUNNER) $(TEST_CONCURRENCY_OBJ)
	@echo "Clean complete."


# --- Phony Targets ---
.PHONY: all clean test