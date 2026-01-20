## Mutable Daisies
This project ports selected Mutable Instruments modules by Émilie Gillet to the Electro-Smith Daisy platform, with a focus on Daisy Patch and Patch.Init Eurorack hardware.

Daisy Patch is the main target because its screen and encoder enable a richer UI, and it also provides more I/O.

## Patch common UI library
Even on Daisy Patch (which has the most I/O), it’s impossible to match the number of dedicated knobs and inputs found on the original MI modules.

To compensate for this, I’m building a common UI library that lets you edit module parameters using an on-screen menu and the encoder. The UI also lets you configure I/O (e.g., map parameters to inputs, assign roles to outputs, etc.).

Because configuration can be tedious, a preset system lets you save and recall complete configurations from an SD card.
> **tip**: The presets named "default" is loaded at start if it exists.

Each module has its own menu layout, but the goal is to keep the UI largely shared across all ports.
See [[#Common UI documentation]] for details.

## Original module extensions
This “hardware virtualization” layer also enables features that were not available on the original hardware.
For example, any parameter treated as a “knob” can optionally include a virtual attenuverter, even if the original module didn’t provide one.

Daisy Patch also unlocks additional capabilities:
-  MIDI support
-  more CPU and RAM
-  extra I/O compared to the original hardware

Ports may add features to take advantage of this. For example, in the Plaits Patch port:
-  Extra features use the available audio inputs (including audio-rate modulation)
-  The two CV outputs can be configured to output modulation signals (e.g., envelopes or LFOs)
-  The SD card allows multiple user datasets (SysEx banks, wavetables, etc.) to be loaded on demand

## Roadmap
At the moment, only a Daisy Patch port of Plaits is available.

Planned ports (not necessarily in release order):
-  Elements
-  Rings
-  Braids
-  Blades
-  Clouds
-  Beads

Most of these are audio-focused modules. Since I own an ORN8 module, I’m less interested in porting MI CV/Gate-oriented modules. Also, given the limited number of CV/Gate outputs on Daisy Patch and Patch.Init, those ports would likely require too many compromises compared to the original modules.

That said, some CV/Gate algorithms could be included as secondary features, similar to how the Plaits port includes some Warps algorithms.

## Patch.Init firmware idea
During or after the Patch ports (or possibly never—this is a personal project), I may create dedicated Patch.Init firmware builds for each module.

The idea is to load presets created for the equivalent Patch firmware. Presets could also be generated via a Python tool for users who don’t own a Patch.

Not every configuration can map cleanly to Patch.Init’s I/O, so the documentation would explain per-module compatibility rules (and the Python tool could provide compatibility hints).

This approach addresses Patch.Init’s main limitation: configuring complex modules with a minimal UI (one button and a single-color LED), while still benefiting from most features of the Patch ports (within I/O limits).

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
cd plaits
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
