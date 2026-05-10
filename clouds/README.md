# Clouds → Daisy Patch: Porting Plan

## Architecture Overview

### Original Clouds (STM32F4)
- **32 kHz** sample rate (codec init in `clouds.cc`)
- **Stereo** in/out (`ShortFrame` = int16 L/R pairs)
- ~118 KB main RAM + 64 KB CCM for audio buffers
- 4 playback modes: Granular, Stretch (WSOLA), Looping Delay, Spectral (phase vocoder)
- `GranularProcessor` is the core DSP class
- `CvScaler` maps hardware knobs/CV → `Parameters` struct
- Internal `SampleRateConverter` for low-fidelity mode (16 kHz)

### Daisy Patch (STM32H7)
- **48 kHz** default sample rate
- **Float** audio (not int16)
- ~512 KB AXI SRAM + 128 KB D2 SRAM + **64 MB SDRAM**
- 4 audio in, 4 audio out, 4 CV/knob combos, 2 gates, 2 CV outs, OLED, encoder, MIDI

---

## Challenge 1: Sample Rate Conversion (32 kHz ↔ 48 kHz)

The core DSP (`GranularProcessor::Process()`) expects `ShortFrame*` at 32 kHz. Three options:

### Option A: Run Daisy at 32 kHz (simplest)
Set `hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ)` if libDaisy supports it. Avoids all conversion but changes the global sample rate, affects filter tuning in the common UI, and may not be available.

### Option B: Decimate 48→32 before Clouds, interpolate 32→48 after (recommended)
- Ratio is 2:3 (non-integer). Approach: upsample ×2 → lowpass → downsample ÷3 (48k×2=96k÷3=32k), reverse on output (32k×3=96k÷2=48k).
- A simpler approach: high-quality linear or cubic interpolation resampler. An input block from Daisy (e.g., 96 samples at 48 kHz) maps to 64 samples at 32 kHz. Feed 64 `ShortFrame`s to `GranularProcessor::Process()`, then resample the 64-sample output back to 96 at 48 kHz.
- **Caution**: `GranularProcessor` internally uses `kMaxBlockSize = 32`. So feed it in chunks ≤32 at the 32 kHz rate.
- Adds ~1 ms of latency, acceptable for a granular effect.

### Option C: Adapt Clouds DSP to run at 48 kHz natively
Modify filter coefficients, grain timing, buffer sizes. Very invasive and error-prone — entire DSP calibrated for 32 kHz. **Not recommended.**

---

## Challenge 2: Audio Format Conversion

`GranularProcessor::Process()` takes `ShortFrame*` (int16 stereo). Daisy provides float buffers.

```cpp
// Float → ShortFrame (input)
frame.l = static_cast<int16_t>(in_l * 32768.0f);
frame.r = static_cast<int16_t>(in_r * 32768.0f);

// ShortFrame → Float (output)
out_l = static_cast<float>(frame.l) / 32768.0f;
out_r = static_cast<float>(frame.r) / 32768.0f;
```

---

## Memory Allocation

Original Clouds uses two statically allocated buffers:
- `block_mem[118784]` (~116 KB, main SRAM)
- `block_ccm[65536 - 128]` (~64 KB, CCM)

On Daisy, place these in:
- **AXI SRAM** (512 KB) — default `.bss`
- **SDRAM** (64 MB) — via `DSY_SDRAM_BSS` attribute

With SDRAM, buffer sizes can be **significantly increased** for longer grain/delay times — a major upgrade. Original Clouds has ~1s of recording buffer in 16-bit stereo; with SDRAM this could be tens of seconds.

---

## What to Reuse vs Replace

### Files to COMPILE (DSP core)
| File | Purpose |
|------|---------|
| `dsp/granular_processor.cc/.h` | Core DSP engine |
| `dsp/parameters.h` | Parameters struct |
| `dsp/frame.h` | ShortFrame/FloatFrame types |
| `dsp/audio_buffer.h` | Circular audio buffer |
| `dsp/correlator.cc/.h` | WSOLA correlation |
| `dsp/grain.h` | Grain structure |
| `dsp/granular_sample_player.h` | Granular playback |
| `dsp/looping_sample_player.h` | Delay/looper playback |
| `dsp/wsola_sample_player.h` | Time-stretch playback |
| `dsp/sample_rate_converter.h` | SRC for low-fi mode |
| `dsp/mu_law.cc/.h` | 8-bit mu-law compression |
| `dsp/window.h` | Window functions |
| `dsp/fx/diffuser.h` | Post-processing diffuser |
| `dsp/fx/reverb.h` | Reverb effect |
| `dsp/fx/pitch_shifter.h` | Pitch shifting |
| `dsp/fx/fx_engine.h` | FX engine infrastructure |
| `dsp/pvoc/phase_vocoder.cc/.h` | Spectral mode |
| `dsp/pvoc/stft.cc/.h` | STFT transform |
| `dsp/pvoc/frame_transformation.cc/.h` | Spectral frame processing |
| `resources.cc/.h` | Lookup tables (`lut_xfade_in`, `lut_xfade_out`, `lut_quantized_pitch`, `lut_sine_window_4096`, `src_filter_1x_2_45`) |

### Files to REPLACE (hardware-specific)
| Original File | Replaced By |
|---------------|-------------|
| `clouds.cc` | `clouds/main.cpp` (our entry point) |
| `cv_scaler.cc/.h` | Common UI `CVMappingProcessor` |
| `ui.cc/.h` | Common UI (OLED menu, encoder) |
| `settings.cc/.h` | Common UI `PresetManager` |
| `drivers/*` | libDaisy |
| `meter.h` | Not needed (or reimplemented for display) |

### Stubs Needed
- **`clouds/drivers/debug_pin.h`** — Contains `TIC`/`TOC` profiling macros referenced by `granular_processor.cc`. Need an empty stub header.

---

## Dependencies

- **stmlib** — `stmlib/dsp/filter.h`, `stmlib/dsp/parameter_interpolator.h`, `stmlib/utils/buffer_allocator.h`, `stmlib/fft/shy_fft.h`, etc. Already in `eurorack/stmlib/` submodule, used by plaits port.
- **`DISALLOW_COPY_AND_ASSIGN` macro** — From stmlib, already works.
- **`Prepare()` loop call** — `processor.Prepare()` runs in main loop (NOT audio callback). Handles mode switching, buffer reallocation, WSOLA correlation. Must replicate in main loop or update callback.

---

## Parameter Mapping

### Original Clouds `Parameters` struct
```cpp
struct Parameters {
  float position;        // Grain position in buffer
  float size;            // Grain size
  float pitch;           // Pitch shift (semitones)
  float density;         // Grain density / overlap
  float texture;         // Grain texture / window shape
  float dry_wet;         // Dry/wet mix
  float stereo_spread;   // Stereo spread amount
  float feedback;        // Feedback amount
  float reverb;          // Reverb amount
  bool freeze;           // Freeze buffer recording
  bool trigger;          // External trigger
  bool gate;             // Gate input state
  struct Granular { ... };   // Granular-mode sub-params
  struct Spectral { ... };   // Spectral-mode sub-params
};
```

### Proposed Menu Structure
```
├── Playback Mode      [ENUM: Granular / Stretch / Delay / Spectral]
├── Position           [KNOB 0.0–1.0]
├── Size               [KNOB 0.0–1.0]
├── Pitch              [KNOB, quantized semitones display]
├── Density            [KNOB 0.0–1.0]
├── Texture            [KNOB 0.0–1.0]
├── Mix (SUB)
│   ├── Dry/Wet        [KNOB 0.0–1.0]
│   ├── Stereo Spread  [KNOB 0.0–1.0]
│   ├── Feedback       [KNOB 0.0–1.0]
│   └── Reverb         [KNOB 0.0–1.0]
├── Freeze             [ENUM: ON/OFF, gate-mappable]
├── Settings (SUB)
│   ├── Quality        [ENUM: Stereo 16b / Mono 16b / Stereo 8b / Mono 8b]
│   ├── MIDI Ch        [MIDI]
│   └── Calibrate      [CALIBRATION]
├── Audio In L (SUB)   [CV input config — mapped to Clouds L input]
├── Audio In R (SUB)   [CV input config — mapped to Clouds R input]
├── CV Out 1 (SUB)     [optional modulation output]
├── CV Out 2 (SUB)     [optional modulation output]
├── Gate Out (SUB)     [optional gate output config]
├── Save               [SAVE]
└── Load               [LOAD]
```

**Key improvement over original**: On the original Clouds, Dry/Wet, Stereo Spread, Feedback, and Reverb share a single "Blend" knob that cycles between them. Here all four are always accessible.

---

## I/O Mapping

| Daisy Patch Jack | Clouds Function |
|------------------|-----------------|
| Audio In 1 | Clouds L input |
| Audio In 2 | Clouds R input (mono sum from In 1 if unplugged) |
| Audio In 3–4 | Available for extensions |
| Audio Out 1 | Clouds L output |
| Audio Out 2 | Clouds R output |
| Audio Out 3–4 | Available (dry signal, meter, etc.) |
| CV/Knob 1–4 | Mappable to any parameter via common UI |
| Gate In 1 | Freeze toggle (rising = freeze, falling = unfreeze) |
| Gate In 2 | Trigger input (for triggering grains) |
| CV Out 1–2 | Configurable (envelope follower, position readout, etc.) |
| Gate Out | Configurable |

---

## Potential Extensions (Daisy Advantages)

- **Longer buffer times** via SDRAM — tens of seconds vs ~1s original
- **All blend parameters always accessible** — no cycling through a single knob
- **MIDI control** — freeze via MIDI, parameter CC mapping, note-to-pitch
- **CV outputs** for envelope follower, grain position readout, etc.
- **Preset recall** of all parameters including playback mode and quality
- **Audio inputs 3–4** available for sidechain, modulation, etc.

---

## Implementation Steps

### Phase 1: Skeleton & Build
1. Create `clouds/` folder with `clouds_port.h`, `clouds_port.cpp`, `main.cpp`, `Makefile`
2. Write Makefile referencing `eurorack/clouds/dsp/`, `eurorack/clouds/resources.cc`, and stmlib sources
3. Create stub `clouds/drivers/debug_pin.h` (empty `TIC`/`TOC` macros)
4. Verify it compiles (no audio yet)

### Phase 2: Sample Rate Conversion
5. Implement `SampleRateConverter48_32` — wrapper that decimates 48→32 kHz (input) and interpolates 32→48 kHz (output)
6. Handle block size adaptation (96 samples @ 48 kHz → 64 samples @ 32 kHz, fed as 2×32 to `GranularProcessor`)

### Phase 3: Core DSP Integration
7. Implement `CloudsPort : ModuleBase` — init `GranularProcessor` with buffers, define Parameters, wire `Process()` with SRC + format conversion
8. Wire `Prepare()` call into main loop update callback
9. Handle Freeze via Gate 1, Trigger via Gate 2

### Phase 4: UI & Presets
10. Define parameter layout — map all Clouds parameters to common UI `Parameter` array
11. Write `main.cpp` following plaits pattern: init hardware, SD, preset manager, ApplicationContext, callbacks, main loop
12. Test parameter mapping, preset save/load

### Phase 5: Extensions & Polish
13. Implement SDRAM-backed larger buffers (optional, significant upgrade)
14. Add CV output options (envelope follower, etc.)
15. Test & tune — verify audio quality through SRC, check CPU usage (spectral mode is heaviest)
16. Create logo bitmap for boot screen

---

## CPU Budget Estimate

Original Clouds on STM32F405 @ 168 MHz used most of the CPU. Daisy's STM32H750 @ 480 MHz is ~3× faster, plus the FPU is stronger. Expected headroom:
- **Granular mode**: comfortable
- **Stretch (WSOLA)**: comfortable
- **Looping Delay**: comfortable
- **Spectral (phase vocoder)**: needs testing — FFT is the heaviest operation, but H7 has much more compute

The sample rate conversion adds some overhead but should be negligible compared to the DSP itself.
