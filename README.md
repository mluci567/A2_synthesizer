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
* **Preset System:**
    * Save the current settings of *both* waves to a preset file (`.synthpreset`).
    * Load settings for both waves from previously saved preset files via a dropdown menu.
    * Presets are stored in the `presets` directory.
* **Audio Engine:**
    * Uses PortAudio for cross-platform audio I/O
    * Real-time audio callback generates samples based on current parameters
    * ADSR envelope applied during audio generation
* **User Interface:**
    * Built with GTK+ 3
    * Visualizes the selected oscillator waveform (without envelope) in a drawing area


## Dependencies

To build and run this project, you will need the following:

* **Build Tools:**
    * `gcc` (or compatible C compiler)
    * `make`
    * `pkg-config` (for detecting library flags)
* **Libraries (Development Headers):**
    * `GTK+ 3` (e.g., `libgtk-3-dev` on Debian/Ubuntu)
    * `PortAudio v19` (e.g., `libportaudio-dev` on Debian/Ubuntu)
    * `GLib 2.0` (e.g., `libglib2.0-dev` on Debian/Ubuntu - often a dependency of GTK+)
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
* The interface is split into sections for Wave 1 and Wave 2 controls.
* For each wave, use the sliders to adjust Frequency, Amplitude, and ADSR envelope parameters (Attack, Decay, Sustain level, Release time).
* For each wave, select the desired Waveform from its dropdown menu.
* Click the "Note On/Off" button for a specific wave to start playing its sound. Click it again to trigger the release phase of that wave's envelope. Both waves can be triggered independently.
* The drawing area at the bottom displays a visual representation of the selected waveform shapes and amplitudes for both waves simultaneously (Wave 1: Magenta, Wave 2: Cyan-Blue).
* Presets:
    * To save the current settings of both waves, click "Save Preset As..." and choose a filename (ending in .synthpreset) in the presets directory.
    * To load previously saved settings, select the desired preset file from the "Load Preset:" dropdown menu. The controls for both waves will update automatically.

## Project Structure
```
A2_synthesizer/
├── makefile              # Build script for the application and tests
├── synth/                # Core synthesizer source code
│   ├── main.c            # Main application entry point, initialization for dual waves
│   ├── gui.c             # GTK+ GUI implementation (dual controls, presets)
│   ├── gui.h             # Header for GUI functions
│   ├── audio.c           # PortAudio implementation (dual wave callback, mixing)
│   ├── audio.h           # Header for audio functions
│   ├── presets.c         # Preset saving and loading logic
│   ├── presets.h         # Header for preset functions
│   └── synth_data.h      # Shared data structures (dual wave params/state, PresetData)
├── presets 
│   └── "_".synthpreset   # Included preset files may vary 
└── tests/                # Unit tests
    ├── test_audio_lifecycle.c # CMocka tests for audio init/start/stop/terminate
    ├── test_gui_helpers.c  # CUnit tests for GUI helper functions (dual wave envelope calcs)
    ├── test_audio.c        # CUnit tests for the audio processing callback (dual wave ADSR, mixing)
    └── test_concurrency.c  # CUnit tests for basic concurrent data access
```
## Preset File Format (`.synthpreset`)

The synthesizer allows saving and loading the complete state of both waves using preset files.

* **Location:** Preset files are expected to be stored in the `presets/` directory relative to the executable.
* **Extension:** Preset files should use the `.synthpreset` extension.
* **Format:**
    * Plain text file.
    * Each line represents one parameter in a `key: value` format.
    * Whitespace around the key, colon, and value is generally ignored during loading.
    * The order of lines does not matter, but all required parameters must be present.
* **Required Parameters:** A valid preset file must contain lines for all 14 parameters (7 for each wave):
    * **Wave 1:**
        * `frequency1`: (float) Frequency in Hz.
        * `amplitude1`: (float) Amplitude (0.0 to 1.0).
        * `waveform1`: (int) Waveform type (0=Sine, 1=Square, 2=Sawtooth, 3=Triangle).
        * `attackTime1`: (float) Attack time in seconds.
        * `decayTime1`: (float) Decay time in seconds.
        * `sustainLevel1`: (float) Sustain level (0.0 to 1.0).
        * `releaseTime1`: (float) Release time in seconds.
    * **Wave 2:**
        * `frequency2`: (float) Frequency in Hz.
        * `amplitude2`: (float) Amplitude (0.0 to 1.0).
        * `waveform2`: (int) Waveform type (0=Sine, 1=Square, 2=Sawtooth, 3=Triangle).
        * `attackTime2`: (float) Attack time in seconds.
        * `decayTime2`: (float) Decay time in seconds.
        * `sustainLevel2`: (float) Sustain level (0.0 to 1.0).
        * `releaseTime2`: (float) Release time in seconds.

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
