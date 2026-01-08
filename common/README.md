# Mutable Instruments Daisy Port - Common Library

## Overview

The common library provides a unified framework for porting Mutable Instruments Eurorack modules to the Electrosmith Daisy Patch hardware. It abstracts parameter management, CV/Gate inputs, display rendering, and preset management.

## Hardware Context: Daisy Patch Limitations

The Daisy Patch hardware has a key limitation: **knobs and CV inputs are summed before the ADC**, meaning we cannot distinguish between knob position and CV input. This affects how we emulate attenuverters.

### Attenuverter Emulation Strategy

To emulate a knob + CV + attenuverter system:

1. **Capture offset**: When `plugged` is enabled, save the current knob position as `offset`
2. **Calculate CV**: `cv_signal = current_value - offset`
3. **Apply attenuversion**: `result = offset + (cv_signal * attenuverter)`

**Limitation**: If the knob position changes while `plugged` is active, attenuversion will be incorrect relative to the new knob position.

---

## Parameter Types

### KNOB - Continuous Parameter

For parameters controlled by a knob with optional CV modulation and attenuverter.

**Display:**
```
(name) (value) (mapping) >
Harmonics 0.45   [2]    >
```

**Mapping indicator**: `[1-4]` for CV, `#` for MIDI CC, empty if none

**Submenu options:**
| Option | Values | Description |
|--------|--------|-------------|
| `mapping` | None, CV1-4, CC1-127 | Modulation source |
| `plugged` | on/off + offset | Captures offset when enabled. Disabled if CC or no mapping. Sends "plugged" status to firmware if supported |
| `attenuverter` | ±100% | Scales CV around offset point. Sent to firmware if supported |
| `velocity` | ±100% | Adds velocity modulation (our layer) |

**Value calculation:**
```
if plugged:
    cv_signal = raw_value - saved_offset
    final = saved_offset + (cv_signal * attenuverter) + (velocity_amount * midi_velocity)
else:
    final = raw_value
```

---

### CV - Direct CV Input

For direct CV signals (envelopes, LFOs) where the knob acts only as an offset. No attenuverter emulation.

**Display:**
```
(name) (value) (mapping) >
Env In  0.72   [3]    >
```

**Note**: Value cannot be edited with encoder (read-only display)

**Submenu options:**
| Option | Values | Description |
|--------|--------|-------------|
| `mapping` | None, CV1-4 | CV source |
| `plugged` | on/off | Auto-enabled when mapping is set. Not user-editable. Sends status to firmware |

---

### ENUM - Enumerated Values

For selecting from a list of discrete options (on/off, engine selection, etc.)

**Display:**
```
(name) (value) (mapping) >
Engine  VA VCF  [G1]   >
Bank    Synth          >
```

**Mapping indicator**: `[G1-2]` for Gate, `[1-4]` for CV, `#` for CC

**Submenu options:**
| Option | Values | Description |
|--------|--------|-------------|
| `mapping` | None, Gate1-2, CV1-4, CC1-127 | Control source |

**If Gate mapped:**
| Option | Values | Description |
|--------|--------|-------------|
| `trigger` | rise, fall, rise & fall | Edge detection |
| `action` | ++, --, +-, -+ | Increment/decrement behavior |

**Gate action examples:**

| Use case | Trigger | Action | Behavior |
|----------|---------|--------|----------|
| Toggle on/off | rise | ++ | Each gate rising edge toggles |
| Momentary | rise & fall | +- | On when high, off when low |
| Alternate engines | rise & fall | +- | Alternate between adjacent values |
| Dual gate nav | G1:rise ++, G2:rise -- | Navigate list with two gates |

**If CV or CC mapped:**
- Values are quantized: `index = floor(cv * num_values)` or `index = floor(cc / (128 / num_values))`

---

### MIDI - Channel Selection

For configuring MIDI channel (useful for modules using V/Oct + Gate, converted to MIDI).

**Display:**
```
(name) (value)
MIDI Ch  CH1   >
```

**No submenu** - direct value edit (CH1-16)

**Benefits of MIDI over V/Oct:**
- Polyphony support
- Velocity modulation
- Frees Gate + V/Oct inputs for other uses
- Standardized note handling

---

### SUB - Submenu Container

Groups related parameters in a collapsible submenu.

**Display:**
```
(name)
CV Outputs    >
```

**Submenu**: Contains child parameters (KNOB, ENUM, etc.)

**Restriction**: SUB can only appear in root menu (no nested SUB)

**Use cases:**
- CV output configuration (envelope vs LFO, parameters)
- Modulator settings
- Secondary parameter groups

---

### SAVE - Preset Save

Saves current configuration to SD card.

**Display:**
```
Save          >
```

**Behavior:**
1. Select → Enter character input mode
2. Character selection: rotate through `a-z`, `0-9`, `-`, `_`, `.`
3. Short press → confirm character, move to next
4. Long press → save preset and return to root

**Storage location:** `<module_name>/presets/<preset_name>.bin`

**Error**: Shows message if SD card not present

---

### LOAD - Preset Load

Loads a preset from SD card.

**Display:**
```
Load          >
```

**Behavior:**
1. Select → Show list of presets from SD card
2. Navigate with encoder
3. Short press → load preset, show success, return to root

**Storage location:** `<module_name>/presets/`

**Error**: Shows message if SD card not present or no presets found

---

## Mapping Indicators

| Indicator | Meaning |
|-----------|---------|
| `[1]`-`[4]` | Mapped to CV 1-4 |
| `[G1]`-`[G2]` | Mapped to Gate 1-2 |
| `#` | Mapped to MIDI CC |
| (empty) | No mapping |

---

## Data Structures

### Parameter

```cpp
struct Parameter {
    const char* name;
    ParamType type;           // KNOB, CV, ENUM, MIDI, SUB, SAVE, LOAD
    
    float value;              // Current value (0.0-1.0 for continuous)
    float min, max;           // Range
    
    // For ENUM
    const char** enum_labels;
    int enum_count;
    
    // Mapping
    MappingConfig mapping;
    
    // For SUB
    Parameter* children;
    int child_count;
};

struct MappingConfig {
    MappingSource source;     // NONE, CV1-4, GATE1-2, CC
    int cc_number;            // If source is CC
    
    // For KNOB
    bool plugged;
    float offset;             // Captured when plugged enabled
    float attenuverter;       // -1.0 to +1.0
    float velocity_amount;    // -1.0 to +1.0
    
    // For ENUM with Gate
    TriggerMode trigger;      // RISE, FALL, RISE_AND_FALL
    EnumAction action;        // INCREMENT, DECREMENT, TOGGLE_PLUS, TOGGLE_MINUS
};
```

---

## File Structure

```
common/
├── README.md           # This documentation
├── parameter.h         # Parameter types and structures
├── ui_state.h          # Menu state machine
├── cv_input.h          # CV input processing with attenuverter
├── display.h           # OLED display rendering
├── module_base.h       # Abstract module interface
└── preset_manager.h    # SD card preset system
```

---

## Implementation Notes

### Firmware Integration

Some original firmwares support:
- **Plugged status**: Triggers alternate behavior when jack is inserted
- **Attenuverter**: Hardware attenuverter on CV input

When supported by original firmware, pass these values through to get authentic behavior. When not supported, handle in our abstraction layer.

### MIDI Advantages

Converting V/Oct + Gate to MIDI provides:
1. **Polyphony**: Multiple voices from single MIDI input
2. **Velocity**: Natural expression control
3. **Freed inputs**: Gate and CV inputs available for other mappings
4. **Standardization**: Consistent note handling across modules

### Gate Output

MIDI clock can be output as Gate signal for synchronization with other modules.
