# Makefile for Synthesizer with Unit Tests

# Compiler
CC = gcc

# Project Name
TARGET = synthesizer

# --- Source Files & Objects for Main Application ---
SYNTH_DIR = synth
SRCS = $(SYNTH_DIR)/main.c $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/audio.c $(SYNTH_DIR)/presets.c
OBJS = $(SRCS:.c=.o)

# --- Compiler and Linker Flags for Main Application ---
# -pthread is needed for compiling AND linking with pthreads
CFLAGS = -Wall -g -pthread
# Include Paths (using pkg-config)
GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
PORTAUDIO_CFLAGS = $(shell pkg-config --cflags portaudio-2.0)
CFLAGS += $(GLIB_CFLAGS) $(GTK_CFLAGS) $(PORTAUDIO_CFLAGS)

# Linker Flags (using pkg-config)
GLIB_LIBS = $(shell pkg-config --libs glib-2.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
PORTAUDIO_LIBS = $(shell pkg-config --libs portaudio-2.0)
LIBS = $(GLIB_LIBS) $(GTK_LIBS) $(PORTAUDIO_LIBS) -lm -lpthread

# --- Testing Specific Definitions ---
TEST_DIR = tests

TEST_AUDIO_CALLBACK_SRC = $(TEST_DIR)/test_audio.c
TEST_AUDIO_CALLBACK_OBJ = $(TEST_AUDIO_CALLBACK_SRC:.c=.o)
TEST_AUDIO_CALLBACK_RUNNER = test_runner_audio_callback
AUDIO_OBJ_FOR_TEST = $(SYNTH_DIR)/audio.o_test

TEST_GUI_HELPERS_SRC = $(TEST_DIR)/test_gui_helpers.c
TEST_GUI_HELPERS_OBJ = $(TEST_GUI_HELPERS_SRC:.c=.o)
TEST_GUI_HELPERS_RUNNER = test_runner_gui_helpers
GUI_OBJ_FOR_TEST = $(SYNTH_DIR)/gui.o_test
PRESETS_OBJ_FOR_TEST = $(SYNTH_DIR)/presets.o_test

TEST_AUDIO_LIFECYCLE_SRC = $(TEST_DIR)/test_audio_lifecycle.c
TEST_AUDIO_LIFECYCLE_OBJ = $(TEST_AUDIO_LIFECYCLE_SRC:.c=.o)
TEST_AUDIO_LIFECYCLE_RUNNER = test_runner_audio_lifecycle
# Note: TEST_AUDIO_LIFECYCLE also uses AUDIO_OBJ_FOR_TEST

TEST_CONCURRENCY_SRC = $(TEST_DIR)/test_concurrency.c
TEST_CONCURRENCY_OBJ = $(TEST_CONCURRENCY_SRC:.c=.o)
TEST_CONCURRENCY_RUNNER = test_runner_concurrency

# Common flags for compiling test code and project code *for* tests
CUNIT_CFLAGS = $(shell pkg-config --cflags cunit)
CMOCKA_CFLAGS = $(shell pkg-config --cflags cmocka)
TEST_CFLAGS = $(CFLAGS) -DTESTING -I$(SYNTH_DIR) $(CUNIT_CFLAGS) $(CMOCKA_CFLAGS)

# Libraries needed for testing
CUNIT_LIBS = $(shell pkg-config --libs cunit)
CMOCKA_LIBS = $(shell pkg-config --libs cmocka)
TEST_COMMON_LIBS = -lpthread -lm

# --- Build Targets ---

# Default target: build the main application
all: $(TARGET)

# Rule to link the main application executable
# Links all objects listed in OBJS
$(TARGET): $(OBJS)
	@echo "Linking main application: $(TARGET)"
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LIBS)

# --- Rules for Compiling Main Application Object Files ---
$(SYNTH_DIR)/main.o: $(SYNTH_DIR)/main.c $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/audio.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SYNTH_DIR)/gui.o: $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/presets.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SYNTH_DIR)/audio.o: $(SYNTH_DIR)/audio.c $(SYNTH_DIR)/audio.h $(SYNTH_DIR)/synth_data.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SYNTH_DIR)/presets.o: $(SYNTH_DIR)/presets.c $(SYNTH_DIR)/presets.h $(SYNTH_DIR)/synth_data.h
	@echo "Compiling presets module: $<"
	$(CC) $(CFLAGS) -c $< -o $@


# --- Rules for Compiling Project Files *for Testing* ---
$(AUDIO_OBJ_FOR_TEST): $(SYNTH_DIR)/audio.c $(SYNTH_DIR)/audio.h $(SYNTH_DIR)/synth_data.h
	@echo "Compiling audio.c for testing..."
	$(CC) $(TEST_CFLAGS) -c $(SYNTH_DIR)/audio.c -o $@

$(GUI_OBJ_FOR_TEST): $(SYNTH_DIR)/gui.c $(SYNTH_DIR)/gui.h $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/presets.h 
	@echo "Compiling gui.c for testing..."
	$(CC) $(TEST_CFLAGS) -c $(SYNTH_DIR)/gui.c -o $@

$(PRESETS_OBJ_FOR_TEST): $(SYNTH_DIR)/presets.c $(SYNTH_DIR)/presets.h $(SYNTH_DIR)/synth_data.h
	@echo "Compiling presets.c for testing..."
	$(CC) $(TEST_CFLAGS) -c $(SYNTH_DIR)/presets.c -o $@


# --- Rules for Compiling Test Harnesses ---
$(TEST_AUDIO_CALLBACK_OBJ): $(TEST_AUDIO_CALLBACK_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/audio.h
	@echo "Compiling test harness: $(TEST_AUDIO_CALLBACK_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_GUI_HELPERS_OBJ): $(TEST_GUI_HELPERS_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/gui.h
	@echo "Compiling test harness: $(TEST_GUI_HELPERS_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_AUDIO_LIFECYCLE_OBJ): $(TEST_AUDIO_LIFECYCLE_SRC) $(SYNTH_DIR)/synth_data.h $(SYNTH_DIR)/audio.h
	@echo "Compiling test harness: $(TEST_AUDIO_LIFECYCLE_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_CONCURRENCY_OBJ): $(TEST_CONCURRENCY_SRC) $(SYNTH_DIR)/synth_data.h
	@echo "Compiling test harness: $(TEST_CONCURRENCY_SRC)"
	$(CC) $(TEST_CFLAGS) -c $< -o $@


# --- Rules for Linking Test Runners ---
$(TEST_AUDIO_CALLBACK_RUNNER): $(TEST_AUDIO_CALLBACK_OBJ) $(AUDIO_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(PORTAUDIO_LIBS) $(TEST_COMMON_LIBS)

# *** rule for linking GUI helpers test runner ***
$(TEST_GUI_HELPERS_RUNNER): $(TEST_GUI_HELPERS_OBJ) $(GUI_OBJ_FOR_TEST) $(PRESETS_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(GLIB_LIBS) $(GTK_LIBS) $(TEST_COMMON_LIBS)

$(TEST_AUDIO_LIFECYCLE_RUNNER): $(TEST_AUDIO_LIFECYCLE_OBJ) $(AUDIO_OBJ_FOR_TEST)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CMOCKA_LIBS) $(PORTAUDIO_LIBS) $(TEST_COMMON_LIBS)

$(TEST_CONCURRENCY_RUNNER): $(TEST_CONCURRENCY_OBJ)
	@echo "Linking test runner: $@"
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(CUNIT_LIBS) $(TEST_COMMON_LIBS)


# --- Main Test Target ---
test: $(TEST_AUDIO_CALLBACK_RUNNER) $(TEST_GUI_HELPERS_RUNNER) $(TEST_AUDIO_LIFECYCLE_RUNNER) $(TEST_CONCURRENCY_RUNNER)
	@echo "\n--- Running Audio Callback Tests (CUnit) ---"
	./$(TEST_AUDIO_CALLBACK_RUNNER)
	@echo "\n--- Running GUI Helper Tests (CUnit) ---"
	./$(TEST_GUI_HELPERS_RUNNER)
	@echo "\n--- Running Audio Lifecycle Tests (CMocka) ---"
	./$(TEST_AUDIO_LIFECYCLE_RUNNER)
	@echo "\n--- Running Concurrency Tests (CUnit) ---"
	./$(TEST_CONCURRENCY_RUNNER)
	@echo "\n--- All tests finished ---"


# --- Clean Target ---
# Remove all test-related artifacts
clean:
	@echo "Cleaning build and test artifacts..."
	# *** ADDED $(PRESETS_OBJ_FOR_TEST) to clean target ***
	rm -f $(TARGET) $(OBJS) \
	      $(TEST_AUDIO_CALLBACK_RUNNER) $(TEST_AUDIO_CALLBACK_OBJ) $(AUDIO_OBJ_FOR_TEST) \
	      $(TEST_GUI_HELPERS_RUNNER) $(TEST_GUI_HELPERS_OBJ) $(GUI_OBJ_FOR_TEST) $(PRESETS_OBJ_FOR_TEST) \
	      $(TEST_AUDIO_LIFECYCLE_RUNNER) $(TEST_AUDIO_LIFECYCLE_OBJ) \
	      $(TEST_CONCURRENCY_RUNNER) $(TEST_CONCURRENCY_OBJ)
	@echo "Clean complete."


# --- Phony Targets ---
.PHONY: all clean test