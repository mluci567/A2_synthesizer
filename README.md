# C Dual Wave Synthesizer

A simple dual-oscillator synthesizer application built in C, featuring a GTK+ 3 graphical user interface and real-time audio output using the PortAudio library. It generates sound by mixing two independently controllable synthesizer waves.

## Features

* **Oscillator:** Generates basic waveforms:
    * Sine
    * Square
    * Sawtooth
    * Triangle
**Per-Wave Controls:** Each wave has its own dedicated controls:
    * **Adjustable frequency (20Hz - 2000Hz)**: Frequency directly corresponds to the perceived pitch of the sound. A higher frequency value results in a higher-pitched note, while a lower frequency results in a lower-pitched note. In the audio callback (`paCallback` in `audio.c`), the local_freq variable determines how much the oscillator's `local_phase` is incremented per sample (`local_phase += 2.0 * M_PI * local_freq / local_sampleRate;`). A faster phase change means the waveform completes its cycle more times per second, producing a higher pitch.
    * **Adjustable master amplitude (0.0 - 1.0)**: Amplitude represents the intensity or "height" of the sound wave. A higher amplitude value results in a louder sound. In the audio callback, the `local_amp` value scales the generated waveform sample (`sample *= env_multiplier;`) and is also used as the peak target level during the ADSR envelope calculations (e.g., the attack phase ramps up to `local_amp`).
    * **Full ADSR (Attack, Decay, Sustain, Release) envelope control via sliders**: The ADSR (Attack-Decay-Sustain-Release) envelope controls how the sound's amplitude changes over time.
        * Attack Time Slider: Sets the time (in seconds) for the sound to ramp up from silence to its maximum amplitude after "Note On". Controls `g_synth_data.attackTime`. Defines the duration of the `ENV_ATTACK` stage. Short attack = abrupt start; long attack = slow fade-in.
        * Decay Time Slider: Sets the time (in seconds) for the sound to decrease from the peak attack amplitude to the sustain level. Controls `g_synth_data.decayTime`. Defines the duration of the `ENV_DECAY` stage. Short decay = brief peak; long decay = gradual drop to sustain level.
        * Sustain Level Slider: Sets the amplitude level (0.0-1.0 relative to max amplitude) the sound holds after attack/decay, while "Note On" is active. Controls `g_synth_data.sustainLevel`. Determines the amplitude during the `ENV_SUSTAIN` stage. High sustain = note stays loud; low sustain = note gets quieter; 0 sustain = sound decays away even while held.
        * Release Time Slider: Sets the time (in seconds) for the sound to fade from the current level to silence after "Note On" is released. Controls `g_synth_data.releaseTime`. Defines the duration of the `ENV_RELEASE` stage. Short release = sound stops abruptly; long release = sound fades out gradually.
    * **Waveform selection via dropdown menu**: The waveform shape determines the timbre or tonal quality of the sound. Different shapes have different harmonic content, making them sound distinct even at the same pitch and loudness.
        * Sine: The purest tone, containing only the fundamental frequency. Sounds smooth. Generated using `sin(local_phase)`.
        * Square: Contains only odd harmonics. Sounds bright and hollow. Generated using `(sin(local_phase) >= 0.0 ? 1.0 : -1.0)`.
        * Sawtooth: Contains both even and odd harmonics. Sounds bright and buzzy. Generated using `(fmod(local_phase, 2.0 * M_PI) / M_PI) - 1.0`.
        * Triangle: Contains only odd harmonics, rolling off faster than a square wave. Brighter than sine, mellower than square/sawtooth. Generated using `(2.0 / M_PI) * asin(sin(local_phase))`. The `paCallback` function uses a switch statement based on the selected `local_wave` to apply the corresponding mathematical formula.
    * **"Note On/Off" toggle button to trigger and release sounds**
        * Pressing (Note On**)**: The callback (`on_note_on_button_toggled`) sets the ADSR state (`g_synth_data.currentStage`) to `ENV_ATTACK`, resets `timeInStage` and `phase`, and sets the `note_active` flag. The audio callback begins processing the Attack -> Decay -> Sustain stages.
        * Releasing (Note Off): The callback calculates the current envelope amplitude (`calculate_current_envelope`) and stores it in `lastEnvValue`. It then sets currentStage to `ENV_RELEASE` and resets `timeInStage`. The audio callback uses `lastEnvValue` as the starting point for the release ramp down to silence.
* **Audio Engine:**
    * Uses PortAudio for cross-platform audio I/O
    * Real-time audio callback generates samples based on current parameters
    * ADSR envelope applied during audio generation
* **User Interface:**
    * Built with GTK+ 3
    * Visualizes the selected oscillator waveform (without envelope) in a drawing area
* **Concurrency:** Uses pthreads and a mutex to safely share synthesizer parameters between the GUI thread and the real-time audio thread

## Dependencies

To build and run this project, you will need the following:

* **Build Tools:**
    * `gcc` (or compatible C compiler)
    * `make`
    * `pkg-config` (for detecting library flags)
* **Libraries (Development Headers):**
    * `GTK+ 3` (e.g., `libgtk-3-dev` on Debian/Ubuntu)
    * `PortAudio v19` (e.g., `libportaudio-dev` on Debian/Ubuntu)
    * `pthreads` (usually part of the standard C library/toolchain)
    * `CUnit` (for testing, e.g., `libcunit1-dev` on Debian/Ubuntu)
    * `CMocka` (for testing, e.g., `libcmocka-dev` on Debian/Ubuntu)

## Building

1.  Ensure all dependencies are installed.
2.  Navigate to the `A2_synthesizer` directory in your terminal.
3.  Run the make command:
    ```bash
    make
    ```
    or
    ```bash
    make all
    ```
    This will compile the source files and create an executable named `synthesizer` in the current directory.

## Running

After successfully building the project, run the executable:

```bash
./synthesizer
```
This will launch the GTK+ interface.

## Usage
* Use the sliders to adjust Frequency, Amplitude, and ADSR envelope parameters (Attack, Decay, Sustain level, Release time).
* Select the desired Waveform from the dropdown menu.
* Click the "Note On/Off" button to start playing a sound. Click it again to trigger the release phase of the envelope.
* The drawing area at the bottom displays a visual representation of the selected waveform shape and amplitude.

## Project Structure
```
A2_synthesizer/
├── makefile              # Build script for the application and tests
├── synth/                # Core synthesizer source code
│   ├── main.c            # Main application entry point, initialization
│   ├── gui.c             # GTK+ GUI implementation and callbacks
│   ├── gui.h             # Header for GUI functions
│   ├── audio.c           # PortAudio implementation, audio callback, ADSR logic
│   ├── audio.h           # Header for audio functions
│   └── synth_data.h      # Shared data structures and enums (Waveform, ADSR Stage)
└── tests/                # Unit tests
    ├── test_audio_lifecycle.c # CMocka tests for audio init/start/stop/terminate
    ├── test_gui_helpers.c  # CUnit tests for GUI helper functions (e.g., envelope calculation)
    └── test_audio.c        # CUnit tests for the audio processing callback (paCallback) 
```
## Testing
The project includes unit tests using the `CUnit` and `CMocka` frameworks.

Ensure `CUnit` and `CMocka` development libraries are installed.
Navigate to the `A2_synthesizer` directory.
Run the test target using make:
```Bash

make test
```
This will compile the necessary test files and run the test suites, printing the results to the console.
* CMocka tests (test_runner_audio_lifecycle) are failing significantly. The errors like `%s() has remaining non-returned values` and `%s function was expected to be called but was not` mean that the mock functions I defined in`tests`/`test_audio_lifecycle.c` (like `__wrap_Pa_Initialize`) are not actually being called when the tests run the real functions from `audio.c` (like `initialize_audio`) buttt they do work - I ran out of time to fix them after switching to 2 waves - oops!
