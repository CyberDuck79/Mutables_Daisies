# Mutable Instruments Ports for Daisy

Ports of [Mutable Instruments](https://mutable-instruments.net/) Eurorack module firmwares to the [Electrosmith Daisy](https://www.electro-smith.com/daisy) platform.

## Overview

This repository contains ports of open-source Mutable Instruments DSP code to run on Daisy hardware, specifically targeting the **Daisy Patch.Init()** module.

## Repository Structure

```
mutables_daisies/
├── eurorack/          # Mutable Instruments source (submodule)
│   ├── plaits/        # Macro Oscillator 2
│   ├── clouds/        # Texture Synthesizer
│   ├── rings/         # Resonator
│   ├── braids/        # Macro Oscillator
│   ├── elements/      # Modal Synthesizer
│   ├── warps/         # Meta Modulator
│   ├── stmlib/        # Shared DSP library
│   └── ...
├── libDaisy/          # Daisy hardware abstraction (submodule)
├── DaisySP/           # Daisy DSP library (submodule)
└── plaits_daisy/      # Plaits port for Patch.Init()
```

## Ported Modules

| Module | Status | Target Hardware |
|--------|--------|-----------------|
| **Plaits** | 🟡 Phase 1 (Basic) | Patch.Init() |
| Clouds | 🔴 Not started | - |
| Rings | 🔴 Not started | - |
| Braids | 🔴 Not started | - |
| Elements | 🔴 Not started | - |

### Legend
- 🟢 Complete
- 🟡 In Progress
- 🔴 Not Started

## Target Hardware

### Daisy Patch.Init()
- **MCU**: STM32H750 (Cortex-M7, 480 MHz)
- **RAM**: 64 KB internal + 64 MB SDRAM
- **Flash**: 128 KB internal + 8 MB QSPI
- **Audio**: 24-bit stereo codec @ 48kHz
- **CV I/O**: 4 knobs, 4 CV inputs, 2 CV outputs, 2 gate inputs

## Getting Started

### Prerequisites

- ARM GCC Toolchain (`arm-none-eabi-gcc`)
- Make
- Git

### Clone with Submodules

```bash
git clone --recursive https://github.com/YOUR_USERNAME/mutables_daisies.git
cd mutables_daisies
```

Or if already cloned:

```bash
git submodule update --init --recursive
```

### Build Libraries

```bash
# Build libDaisy
cd libDaisy
git submodule update --init --recursive
make
cd ..

# Build DaisySP
cd DaisySP
make
cd ..

# Initialize eurorack submodules
cd eurorack
git submodule update --init --recursive
cd ..
```

### Build a Port

```bash
cd plaits_daisy
make
```

### Flash to Hardware

Requires the Daisy bootloader for larger firmwares:

```bash
make program-dfu
```

## Porting Approach

The porting strategy keeps Mutable Instruments' original DSP code **unchanged** wherever possible:

1. **DSP Core**: Used as-is from `eurorack/` repository
2. **Hardware Abstraction**: Replaced with libDaisy SDK
3. **stmlib**: Shared utilities compiled directly
4. **UI/Controls**: Remapped to Daisy hardware

This approach ensures:
- Audio quality matches the original modules
- Bug fixes upstream can be easily integrated
- Clear separation between platform code and DSP

## License

- **Mutable Instruments code**: MIT License (Emilie Gillet)
- **libDaisy / DaisySP**: MIT License (Electrosmith)
- **Port-specific code**: MIT License

## Credits

- **Emilie Gillet** - Original Mutable Instruments firmware
- **Electrosmith** - Daisy platform and libraries
- **Mutable Instruments Community** - Inspiration and documentation

## Resources

- [Mutable Instruments GitHub](https://github.com/pichenettes/eurorack)
- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [Daisy Forum](https://forum.electro-smith.com/)
