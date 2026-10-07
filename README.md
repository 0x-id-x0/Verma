# VERMA : Wavetable Synth by 0x.id

Serum-style wavetable synth (VST3 + Standalone), built on JUCE 8.

## Features
- **2 wavetable oscillators** (6 band-limited, mipmapped tables x 32 frames: Basic Shapes, Harmonic Sweep, PWM, Vocal Formant, Hollow Fold, Digital Grit)
- Per-osc: WT position, level, pan, octave, semi, fine, **unison up to 16 voices** with detune + blend + stereo spread
- Sub oscillator (sine / tri / square) + noise
- Filter: LP12, LP24, HP12, BP12 with cutoff, resonance, drive, env amount, key tracking
- 2 ADSR envelopes (amp + filter), 2 LFOs (sine/tri/saw/square/S&H) routable to WT pos, cutoff, pitch, level
- FX rack: Distortion, Chorus, Ping-pong Delay, Reverb
- 16-voice polyphony, pitch bend, sustain pedal
- Animated 3D wavetable view, live filter curve, envelope/LFO displays, output scope, on-screen keyboard
- 10 factory presets (supersaw, reese, wobble, pluck, pads, keys...)

## Get the DLL / VST3 (easiest: no installs)
1. Make a free GitHub repo and upload everything in this folder (including the `.github` folder).
2. Open the **Actions** tab, the "Build Verma" job runs automatically (~5-8 min).
3. Download the **Verma-Windows-x64** artifact. Inside is `Verma.vst3`.

## Or build locally on Windows
Install Visual Studio 2022 Community (workload: *Desktop development with C++*), CMake and Git, then double-click `build_windows.bat`.

## Install in FL Studio
1. Copy `Verma.vst3` to `C:\Program Files\Common Files\VST3\`
2. FL Studio > Options > Manage plugins > **Find installed plugins**
3. Channel rack > + > **Verma**
