Flavien Henrion: OK so, I'm an experienced audio programmer and I want try to port mutable instruments firmwares (source code: https://github.com/pichenettes/eurorack/tree/master) to the Electrosmith daisy patch hardware.
Hardware doc: (https://daisy.audio/product/Daisy-Patch/)
Hardware Library: (https://github.com/electro-smith/libDaisy)
Daisy Patch Cheat Sheet: (https://github.com/algoritmarte/algodaisy/blob/main/DaisyPatchCheatSheet.md)

Without targeting any specific module, I would like to design a module agnostic UI design.
The idea would be to have a menu and submenu design to:
- tweak the module parameters with the encoder
- assign parameter to one of the 4 knobs/inputs CV (they are the same, input CV is added to knob value)
- to imitate attenuverter with the input CV implementation -> be able to set an offset from the current knob value (so input CV not plugged) to get a origin value from which apply attenuversion
- be able to set "enum" values parameters from the modules like engine / algorythm selection
- be able to set toggle values parameters
- being able to save current CV mapping configuration and others parameters values in the SD card as presets with path (/<module_name>/presets/<preset_name>)

Other features and configuration could be module specific and fixed like audio inputs/outputs mapping, CV ouputs mapping, gate inputs/outputs mapping and even midi handling as well as user data reading from SD card (for samples, waveforms, etc...).

I see the UI in BW OLED screen like that:
name of the parameter - value - (CV number if mapped / > symbol to signal mapping submenu)
encoder rotation change parameter selection then push select parameter for value change, then rotation change value then push validate.
sub menu navigation can be done with long encoder press -> if parameter with sub menu selected go into submenu, if already in a submenu, go back to main menu.

This is already a lot of informations, can you do a resume and give ideas and advices on implementation ?

Raycast AI: ## Summary: Mutable Instruments → Daisy Patch Port with Module-Agnostic UI

### Project Overview

You want to create a **universal UI framework** for porting Mutable Instruments DSP code to the **Electrosmith Daisy Patch** hardware, enabling parameter control, CV mapping with attenuverter simulation, and preset management.

### Hardware Resources (Daisy Patch)

| Resource | Details |
|----------|---------|
| **OLED Display** | 128×64 BW, SSD130x controller |
| **Encoder** | Push + rotation |
| **CV/Knobs** | 4× combined (CV input ÷5 + knob, clamped 0-1) |
| **CV Outputs** | 2× (12-bit DAC, 0-5V) |
| **Audio I/O** | 4× in (normalized 1→2→3→4), 4× out |
| **Gate I/O** | 2× in, 2× out |
| **MIDI** | In/Out |
| **SD Card** | Available for presets/samples |
| **Memory** | 64MB SDRAM, 8MB QSPI Flash |

### Proposed UI Architecture

#### Parameter Types to Support

```cpp
enum class ParamType {
    Continuous,    // Float 0.0-1.0 (most MI params)
    Bipolar,       // Float -1.0 to 1.0
    Enum,          // Discrete selection (engine, algorithm)
    Toggle,        // Boolean on/off
    Integer        // Stepped values
};
```

#### CV Mapping Structure

```cpp
struct CVMapping {
    int8_t cv_input;        // -1 = unmapped, 0-3 = CV1-4
    float attenuverter;     // -1.0 to 1.0
    float origin_offset;    // Captured knob value as center point
    bool active;
};

struct Parameter {
    const char* name;
    ParamType type;
    float value;
    float min, max;
    CVMapping cv_mapping;
    // For enums:
    const char** enum_labels;
    uint8_t enum_count;
};
```

#### UI State Machine

```cpp
enum class UIState {
    Navigate,       // Encoder rotation scrolls parameters
    EditValue,      // Encoder rotation changes value
    Submenu,        // CV mapping options
    SubmenuEdit     // Editing submenu values
};
```

#### Display Layout

```
┌────────────────────────────────┐
│ HARMONICS        0.75    CV2 > │  ← Parameter with CV assigned
│ TIMBRE           0.50      >   │  ← Has submenu (no CV)
│>MORPH            0.33    CV1   │  ← Currently selected
│ ENGINE           Modal     >   │  ← Enum parameter
└────────────────────────────────┘
```

### Implementation Recommendations

**1. Attenuverter Simulation**
The MI-style attenuverter with origin capture works like this:
```cpp
float ProcessCVInput(Parameter& p, float knob_value, float cv_value) {
    if (p.cv_mapping.cv_input < 0) return knob_value;
    
    // CV scaled by attenuverter, applied relative to origin
    float cv_contribution = cv_value * p.cv_mapping.attenuverter;
    float result = p.cv_mapping.origin_offset + cv_contribution;
    return std::clamp(result, p.min, p.max);
}
```
When the user "captures" the origin, store the current knob position. This allows the CV to swing ± around that point. [Source](https://github.com/algoritmarte/algodaisy/blob/main/DaisyPatchCheatSheet.md)

**2. Encoder Navigation Logic**
```cpp
// Short press: toggle between Navigate ↔ EditValue
// Long press: Enter/exit submenu
// Rotation: scroll params (Navigate) or change value (EditValue)
```

**3. Preset System with SD Card**
```cpp
// Path format: /<module_name>/presets/<preset_name>.json
struct Preset {
    std::vector<float> param_values;
    std::vector<CVMapping> cv_mappings;
    // Optional module-specific data
};
```

Use libDaisy's `FatFsInterface` for SD operations. JSON or binary format both work—binary is faster, JSON is human-readable. [Source](https://daisy.audio/product/Daisy-Patch/)

**4. Module Abstraction Layer**

Create a base class that each ported module implements:

```cpp
class MIModuleBase {
public:
    virtual void Init(float sample_rate) = 0;
    virtual void Process(AudioBuffer& in, AudioBuffer& out) = 0;
    virtual const char* GetName() = 0;
    virtual std::span<Parameter> GetParameters() = 0;
    
    // Fixed per-module (not in generic UI)
    virtual void ConfigureIO(DaisyPatch& hw) = 0;
};
```

**5. Important MI Porting Notes**

Mutable Instruments code is MIT-licensed for STM32F projects, making it compatible with Daisy. The DSP code in modules like Plaits, Rings, Clouds, Elements is largely hardware-agnostic—you mainly need to:
- Replace hardware abstraction (`stmlib` GPIO, ADC, DAC)
- Adapt sample rate (Daisy runs at 48kHz by default, some MI modules use 96kHz)
- Handle the parameter scaling (MI uses various internal ranges)

Many have successfully ported MI code to other platforms including Max/MSP and SuperCollider, confirming the DSP algorithms are portable. [^1] [^2]

### Suggested Development Order

1. **Core UI Framework**: Parameter container, encoder handling, OLED rendering
2. **CV Processing**: Input reading with LP filtering (hardware can float), mapping system
3. **Preset Manager**: SD card read/write with path management
4. **First Port**: Start with Plaits (well-documented, widely ported)
5. **Refinement**: Add module-specific features as needed

This modular approach lets you reuse 80%+ of the UI code across all ported modules while allowing module-specific I/O configuration.

[^1]: https://vboehm.net/2020/02/mutable-instruments-max-port/
[^2]: https://forum.electro-smith.com/t/porting-mutable-instruments-code/131
[^3]: https://pichenettes.github.io/mutable-instruments-documentation/modules/elements/firmware/
[^4]: https://pichenettes.github.io/mutable-instruments-documentation/third_party_firmware/