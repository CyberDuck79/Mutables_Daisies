# Plaits for Daisy Patch.Init()

Port of Mutable Instruments Plaits to Electrosmith Daisy Patch.Init() hardware.

## Status

**Phase 1: Minimal Working Example** ✅

This is a basic port that demonstrates:
- All 24 Plaits synthesis engines running on Daisy
- 4 knobs mapped to core parameters (Frequency, Harmonics, Timbre, Morph)
- Stereo audio output (Main + Aux)

## Hardware Mapping

### Current Mapping (Phase 1)
| Control | Parameter |
|---------|-----------|
| CV_1 (Knob 1) | Frequency/Pitch (24-96 MIDI notes) |
| CV_2 (Knob 2) | Harmonics |
| CV_3 (Knob 3) | Timbre |
| CV_4 (Knob 4) | Morph |
| Audio Out L | Main Output |
| Audio Out R | Aux Output |

### Future Expansion (Phase 2+)
- CV_5-8: Additional CV inputs for modulation
- Gate inputs: Trigger/Accent
- Engine selection via UI

## Building

### Prerequisites

1. ARM GCC Toolchain installed (`arm-none-eabi-gcc`)
2. libDaisy and DaisySP libraries built:

```bash
cd ../libDaisy && git submodule update --init --recursive && make
cd ../DaisySP && make
cd ../eurorack && git submodule update --init --recursive
```

### Compile

```bash
make
```

### Flash (requires Daisy bootloader)

This firmware uses SRAM boot mode (too large for internal flash).
You need the Daisy bootloader installed first.

```bash
# Via DFU (bootloader mode)
make program-dfu
```

## Project Structure

```
plaits_daisy/
├── Makefile              # Build configuration
├── plaits_daisy.cpp      # Main application
├── plaits/
│   └── user_data.h       # Stub for Plaits user data (Daisy-specific)
└── build/
    └── plaits_daisy.bin  # Compiled firmware
```

## Technical Notes

### Memory Usage
- Uses SRAM boot mode (bootloader required)
- ~227 KB firmware size
- Plaits engines share a 16KB buffer

### Porting Approach
The port keeps the original Plaits DSP code unchanged and only:
1. Replaces hardware drivers with Daisy SDK
2. Stubs out user data flash storage (future: use QSPI flash)
3. Maps CV inputs to Plaits parameters

### stmlib Dependencies
Required stmlib modules:
- `dsp/atan.cc` - Arctangent approximation
- `dsp/units.cc` - Unit conversion utilities
- `utils/random.cc` - Random number generator

## License

Plaits code: MIT License (Mutable Instruments / Emilie Gillet)
This port: MIT License

## Credits

- Original Plaits firmware: [Emilie Gillet / Mutable Instruments](https://github.com/pichenettes/eurorack)
- Daisy Platform: [Electrosmith](https://github.com/electro-smith/libDaisy)
