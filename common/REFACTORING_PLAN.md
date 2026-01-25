# Common Library Refactoring Plan

**Created:** 2026-01-24  
**Status:** Completed. Verified with `test_parameter_templates.cpp`.  
**Goal:** Move UI and parameter-related responsibilities from `plaits/main.cpp` and `plaits/plaits_port.cpp` to the common library for reusability across future module ports.

---

## Table of Contents

1. [Current State Analysis](#current-state-analysis)
2. [Target Architecture](#target-architecture)
3. [Points of Attention (Risks)](#points-of-attention-risks)
4. [Testing Strategy](#testing-strategy)
5. [Implementation Phases](#implementation-phases)
6. [Detailed File Plans](#detailed-file-plans)
7. [Manual Testing Checklist](#manual-testing-checklist)

---

## Current State Analysis

### File Sizes (Lines of Code)

| File | Current LOC | Target LOC | Reduction |
|------|-------------|------------|-----------|
| `plaits/main.cpp` | ~1093 | ~300 | ~70% |
| `plaits/plaits_port.cpp` | ~1615 | ~800 | ~50% |

### Code in main.cpp That Should Be in Common

| Lines | Functionality | New Location |
|-------|--------------|--------------|
| 68-112 | `CVMappingCache` structure | `common/cv_mapping_processor.h` |
| 91-174 | `CalculateMappedValue`, `RebuildMappingCache` | `common/cv_mapping_processor.h` |
| 176-206 | `CalculateEnumFromCV` | `common/cv_mapping_processor.h` |
| 208-398 | AudioCallback CV/CC processing | `common/parameter_processor.h` |
| 408-700 | `UpdateEncoder` (all UI state handling) | `common/controllers/encoder_controller.h` |
| 900-930 | MIDI processing (channel filtering, thru) | `common/midi_processor.h` |

### Code in plaits_port.cpp That Should Be in Common

| Lines | Functionality | New Location |
|-------|--------------|--------------|
| 104-300 | Generic visibility/format callbacks | `common/parameter_callbacks.h` |
| 325-470 | CVOut visibility/format callbacks | `common/parameter_templates/cv_output_config.h` |
| 740-830 | `SetupCVOutParams`, `SetupAudioInParams` | `common/parameter_templates/` |

---

## Target Architecture

### New Directory Structure

```
common/
├── constants.h                      # EXISTING - shared constants
├── cv_input.h                       # EXISTING - CV input filtering
├── cv_mapping_processor.h           # NEW - CV/CC mapping cache & processing
├── display.h                        # EXISTING - display wrapper
├── midi_processor.h                 # NEW - MIDI filtering, thru, CC storage
├── module_base.h                    # EXISTING - module interface
├── parameter.h                      # EXISTING - parameter structure
├── parameter_processor.h            # NEW - applies CV/CC values to params
├── preset_manager.h                 # EXISTING - preset save/load
├── sd_dma_buffer.h                  # EXISTING - SD card DMA
├── ui_state.h                       # EXISTING - menu state (enhanced)
│
├── controllers/
│   └── encoder_controller.h         # NEW - generic encoder handling
│
├── parameter_templates/
│   ├── cv_output_config.h           # NEW - CV output param setup
│   ├── audio_input_config.h         # NEW - audio input param setup
│   ├── filter_config.h              # NEW - filter param setup
│   └── standard_callbacks.h         # NEW - reusable visibility/format callbacks
│
├── renderers/                       # EXISTING
│   ├── mapping_submenu_renderer.h
│   ├── menu_renderer.h
│   └── preset_renderer.h
│
├── state/                           # EXISTING
│   ├── navigation_state.h
│   ├── preset_state.h
│   ├── submenu_state.h
│   └── ui_enums.h
│
├── utils/
│   ├── format_utils.h               # EXISTING - value formatting
│   └── list_navigator.h             # EXISTING
│
└── tests/                           # NEW - unit tests
    ├── CMakeLists.txt
    ├── test_main.cpp
    ├── test_parameter.cpp
    ├── test_menu_state.cpp
    ├── test_cv_mapping.cpp
    ├── test_format_utils.cpp
    ├── test_encoder_controller.cpp
    └── mocks/
        ├── mock_display.h
        └── mock_hardware.h
```

---

## Points of Attention (Risks)

### 1. CV Mapping Timing & Precision

**Location:** `main.cpp` lines 215-227  
**Risk:** ADC scaling has hardware-specific calibration values

```cpp
constexpr float kADCMin = 0.03f;
constexpr float kADCMax = 0.96f;
constexpr float kDeadzone = 0.005f;
```

**Mitigation:** 
- Keep hardware-specific scaling in main.cpp
- Pass already-scaled values to common library
- Test V/Oct tracking across multiple octaves

### 2. Sample-and-Hold for Bank/Engine

**Location:** `main.cpp` lines 52-54, 303-320  
**Risk:** Timing-sensitive flag that must be set on NoteOn and cleared after CV processing

**Mitigation:**
- Keep sample_hold_pending flag in module-specific code
- Or pass a "trigger sample" flag to CVMappingProcessor

### 3. Mapping Cache Invalidation

**Locations where `mapping_cache_dirty_` must be set:**
- Line 505: Plugged toggle in Submenu
- Line 525: Plugged toggle for ENUM
- Line 635-668: Mapping source/CC changes in SubmenuEdit
- Line 779: After preset load

**Mitigation:**
- CVMappingProcessor has `MarkDirty()` method
- EncoderController calls it when mappings change
- Return dirty flag from preset load

### 4. UI State Machine Transitions

**Valid transitions:**
```
Navigate ↔ EditValue
Navigate → Submenu → SubmenuEdit → Submenu → Navigate
Navigate → CharInput → Navigate
Navigate → PresetList → Navigate  
Navigate → FileBrowser → Navigate
Navigate ↔ SUB children (EnterSub/ExitSub)
```

**Edge cases to test:**
- Long press while in SUB child → should open mapping submenu
- Selecting SUB title → should exit SUB
- Cancel in CharInput → should return to Navigate
- Load preset while in SUB → should work correctly

### 5. Parameter Serialization

**Serialized fields:**
- `Parameter.value`
- `MappingConfig.source`
- `MappingConfig.cc_number`
- `MappingConfig.plugged`
- `MappingConfig.offset`
- `MappingConfig.attenuverter`
- `MappingConfig.velocity_amount`
- `MappingConfig.trigger`
- `MappingConfig.action`
- `Parameter.user_data_filename` (for USER_DATA type)

**Mitigation:** Do not change serialization format during refactoring

### 6. Audio Interrupt Safety

**AudioCallback must NOT:**
- Allocate memory (no new, malloc, std::vector resize)
- Do file I/O
- Use printf/logging
- Have unbounded loops

**Mitigation:** Review all code paths in refactored classes

---

## Testing Strategy

### Unit Test Framework

Using a lightweight test framework that compiles on host (macOS/Linux):

```cpp
// Simple test macros (or use GoogleTest if preferred)
#define TEST(suite, name) void suite##_##name()
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_FLOAT_EQ(a, b) assert(fabs((a) - (b)) < 0.0001f)
#define EXPECT_TRUE(x) assert(x)
#define EXPECT_FALSE(x) assert(!(x))
#define EXPECT_STREQ(a, b) assert(strcmp(a, b) == 0)
```

### Test Categories

#### 1. Parameter Tests (`test_parameter.cpp`)

```cpp
// Normalization
TEST(Parameter, GetNormalizedReturnsCorrectValue)
TEST(Parameter, SetNormalizedClamps)
TEST(Parameter, SetNormalizedWithHysteresisFilters)

// Enum
TEST(Parameter, EnumGetIndexRounds)
TEST(Parameter, EnumSetIndexClamps)
TEST(Parameter, EnumGetLabelReturnsCorrect)

// Type helpers
TEST(Parameter, HasSubmenuForKnob)
TEST(Parameter, HasSubmenuForSub)
TEST(Parameter, IsEditableForKnob)
TEST(Parameter, IsEditableForSave)

// Visibility
TEST(Parameter, IsVisibleWithoutCallback)
TEST(Parameter, IsVisibleWithCallback)
```

#### 2. Menu State Tests (`test_menu_state.cpp`)

```cpp
// Navigation
TEST(MenuState, NextParamWraps)
TEST(MenuState, PrevParamWraps)
TEST(MenuState, ScrollToSelectedUpdatesOffset)

// SUB navigation
TEST(MenuState, EnterSubSetsParent)
TEST(MenuState, ExitSubClearsParent)
TEST(MenuState, NextSubChildSkipsHidden)
TEST(MenuState, PrevSubChildSkipsHidden)
TEST(MenuState, IsSubTitleSelectedWhenMinusOne)

// Submenu
TEST(MenuState, EnterSubmenuSetsState)
TEST(MenuState, ExitSubmenuRestoresNavigate)
TEST(MenuState, GetSubmenuItemCountForKnob)
TEST(MenuState, IsSubmenuItemVisibleForCCNumber)

// CharInput
TEST(MenuState, EnterCharInputInitializes)
TEST(MenuState, NextCharWrapsToTitle)
TEST(MenuState, ConfirmCharAddsToName)
TEST(MenuState, ConfirmCharBackspaceOnSpace)

// PresetList
TEST(MenuState, EnterPresetListSetsCount)
TEST(MenuState, NextPresetWrapsToTitle)
TEST(MenuState, PrevPresetFromTitleGoesToLast)

// FileBrowser
TEST(MenuState, EnterFileBrowserSetsCount)
TEST(MenuState, IsDefaultSelectedAtZero)
```

#### 3. CV Mapping Tests (`test_cv_mapping.cpp`)

```cpp
// MappingConfig helpers
TEST(MappingConfig, IsCVSourceForCV1)
TEST(MappingConfig, IsCVSourceForGate)
TEST(MappingConfig, GetCVIndexReturnsCorrect)
TEST(MappingConfig, GetGateIndexReturnsCorrect)

// CVMappingProcessor
TEST(CVMappingProcessor, CalculateMappedValueUnplugged)
TEST(CVMappingProcessor, CalculateMappedValuePluggedPositive)
TEST(CVMappingProcessor, CalculateMappedValuePluggedNegative)
TEST(CVMappingProcessor, CalculateMappedValueClamps)

TEST(CVMappingProcessor, CalculateEnumIndexQuantizes)
TEST(CVMappingProcessor, CalculateEnumIndexWithPlugged)
TEST(CVMappingProcessor, CalculateEnumIndexClamps)

TEST(CVMappingProcessor, RebuildCacheFindsAllMapped)
TEST(CVMappingProcessor, RebuildCacheFindsSubChildren)
TEST(CVMappingProcessor, CacheIsDirtyAfterMark)
```

#### 4. Format Utils Tests (`test_format_utils.cpp`)

```cpp
TEST(FormatUtils, FormatAttackTimeMin)
TEST(FormatUtils, FormatAttackTimeMid)
TEST(FormatUtils, FormatAttackTimeMax)

TEST(FormatUtils, FormatReleaseTimeMin)
TEST(FormatUtils, FormatReleaseTimeMax)

TEST(FormatUtils, FormatGainDBZero)
TEST(FormatUtils, FormatGainDBMax)

TEST(FormatUtils, FormatPercentZero)
TEST(FormatUtils, FormatPercentHalf)
TEST(FormatUtils, FormatPercentFull)

TEST(FormatUtils, FormatBipolarPercentNegative)
TEST(FormatUtils, FormatBipolarPercentPositive)
TEST(FormatUtils, FormatBipolarPercentZero)

TEST(FormatUtils, FormatLFORateMin)
TEST(FormatUtils, FormatLFORateMax)

TEST(FormatUtils, FormatDegreesZero)
TEST(FormatUtils, FormatDegreesFull)

TEST(FormatUtils, FormatMultiplierZero)
TEST(FormatUtils, FormatMultiplierMax)
```

#### 5. Encoder Controller Tests (`test_encoder_controller.cpp`)

```cpp
// State transitions
TEST(EncoderController, ShortPressOnKnobEntersEdit)
TEST(EncoderController, ShortPressInEditExits)
TEST(EncoderController, LongPressOnKnobEntersSubmenu)
TEST(EncoderController, LongPressInSubmenuExits)

TEST(EncoderController, ShortPressOnSubEntersSub)
TEST(EncoderController, ShortPressOnSubTitleExitsSub)

TEST(EncoderController, ShortPressOnSaveEntersCharInput)
TEST(EncoderController, ShortPressOnLoadEntersPresetList)

// Value editing
TEST(EncoderController, IncrementInEditChangesValue)
TEST(EncoderController, IncrementOnEnumChangesIndex)
TEST(EncoderController, IncrementBlockedWhenCVPlugged)

// Mapping editing
TEST(EncoderController, CycleMappingSourceForKnob)
TEST(EncoderController, CycleMappingSourceForEnum)
TEST(EncoderController, CycleMappingSourceSkipsGateForKnob)
```

---

## Implementation Phases

### Phase 0: Test Infrastructure (Completed)

**Goal:** Create test framework before any refactoring

**Files created:**
1. `common/tests/Makefile`
2. `common/tests/test_main.cpp`
3. `common/tests/test_parameter.cpp`
4. `common/tests/test_menu_state.cpp`
5. `common/tests/test_format_utils.cpp`

**Status:** Done. Tests passing.

### Phase 1: Extract Pure Functions (Completed)

**Goal:** Move functions with no side effects

**Functions extracted:**
1. `CycleMappingSource()` → `common/cv_mapping_processor.h`
2. `CalculateMappedValue()` → `common/cv_mapping_processor.h`
3. `CalculateEnumFromCV()` → `common/cv_mapping_processor.h`

**Status:** Done. Verified with `test_cv_mapping.cpp`.

### Phase 2: Extract CV Mapping Processor (Completed)

**Goal:** Move cache management and processing

**Created:** `common/cv_mapping_processor.h`

**Status:** Done. Verified with `test_cv_mapping.cpp` (stateful tests).

### Phase 3: Extract MIDI Processor (Completed)

**Goal:** Move MIDI channel filtering and CC storage

**Status:** Completed. Verified with `test_midi_processor.cpp`.

### Phase 4: Extract Parameter Templates (Completed)

**Goal:** Move reusable parameter configurations

**Create:**
1. `common/parameter_templates/standard_callbacks.h`
2. `common/parameter_templates/cv_output_config.h`
3. `common/parameter_templates/audio_input_config.h`

**Status:** Completed. Verified with `test_parameter_templates.cpp`.

### Phase 5: Extract Encoder Controller (Completed)

**Goal:** Move all encoder handling to common library

**Create:** `common/controllers/encoder_controller.h`

**Status:** Completed. Verified with `test_encoder_controller.cpp`.

### Phase 6: Update main.cpp

**Goal:** Use new common components

**Changes:**
1. Replace inline CV processing with `CVMappingProcessor`
2. Replace inline encoder handling with `EncoderController`
3. Replace CC storage with `MIDIProcessor`
4. Keep only hardware-specific code

### Phase 7: Update plaits_port.cpp

**Goal:** Use parameter templates

**Changes:**
1. Replace visibility callbacks with `standard_callbacks.h`
2. Replace format callbacks with `standard_callbacks.h`
3. Replace `SetupCVOutParams` with template
4. Replace `SetupAudioInParams` with template

---

## Detailed File Plans

### `common/cv_mapping_processor.h`

```cpp
#pragma once

#include "parameter.h"
#include "cv_input.h"
#include <cstdint>
#include <algorithm>

namespace mutables_ui {

// Maximum parameters that can be mapped to a single CV/CC source
static constexpr int kMaxMappingsPerSource = 8;

// Cache entry for fast parameter lookup during audio processing
struct CVMappingCache {
    Parameter* mapped_params[kMaxMappingsPerSource];
    uint8_t count = 0;
    
    void Clear() {
        count = 0;
        for (int i = 0; i < kMaxMappingsPerSource; i++) {
            mapped_params[i] = nullptr;
        }
    }
    
    void Add(Parameter* param) {
        if (count < kMaxMappingsPerSource) {
            mapped_params[count++] = param;
        }
    }
};

class CVMappingProcessor {
public:
    CVMappingProcessor() = default;
    
    // Mark cache as needing rebuild (call when mappings change)
    void MarkDirty() { cache_dirty_ = true; }
    bool IsDirty() const { return cache_dirty_; }
    
    // Rebuild mapping cache from parameters
    // Call this when cache_dirty_ is true (typically at start of audio callback)
    void RebuildCache(Parameter* params, size_t param_count);
    
    // Process all CV-mapped parameters
    // cv_values: array of 4 filtered CV values (0.0-1.0)
    // hysteresis: threshold for value change detection
    void ProcessCVMappings(const float* cv_values, float hysteresis);
    
    // Process all CC-mapped parameters  
    // cc_values: array of 128 CC values (0.0-1.0)
    // hysteresis: threshold for value change detection
    void ProcessCCMappings(const float* cc_values, float hysteresis);
    
    // Calculate final value for a parameter with CV mapping applied
    // base_value: the parameter's stored value
    // cv_value: current CV input value (filtered)
    // Returns: final value with attenuverter emulation applied
    static float CalculateMappedValue(const Parameter& param, 
                                      float base_value, 
                                      float cv_value);
    
    // Calculate enum index from CV value with attenuverter
    // Returns: quantized index (0 to enum_count-1)
    static int CalculateEnumFromCV(const Parameter& param, float cv_value);
    
private:
    CVMappingCache cv_caches_[4];    // One per CV input
    CVMappingCache cc_caches_[128];  // One per CC number
    bool cache_dirty_ = true;
    
    void AddParameterToCache(Parameter* param);
};

// Implementation of static methods (in header for inlining in audio callback)

inline float CVMappingProcessor::CalculateMappedValue(
    const Parameter& param, 
    float base_value, 
    float cv_value) 
{
    const MappingConfig& m = param.mapping;
    
    if (m.source == MappingSource::NONE) {
        return base_value;
    }
    
    if (m.plugged) {
        // Attenuverter emulation: result = offset + (cv - offset) * attenuverter
        float cv_signal = cv_value - m.offset;
        return std::clamp(m.offset + (cv_signal * m.attenuverter), 0.0f, 1.0f);
    } else {
        // Not plugged: direct CV pass-through
        return cv_value;
    }
}

inline int CVMappingProcessor::CalculateEnumFromCV(
    const Parameter& param, 
    float cv_value) 
{
    const MappingConfig& m = param.mapping;
    
    if (!m.IsCVSource()) {
        return param.GetIndex();
    }
    
    float scaled;
    if (m.plugged) {
        // With plugged: offset-based attenuverter
        float cv_signal = cv_value - m.offset;
        scaled = 0.5f + cv_signal * m.attenuverter;
    } else {
        // Without plugged: simple centered scaling
        scaled = 0.5f + (cv_value - 0.5f) * m.attenuverter;
    }
    scaled = std::clamp(scaled, 0.0f, 1.0f);
    
    // Quantize to enum count
    int index = static_cast<int>(scaled * param.enum_count);
    return std::clamp(index, 0, static_cast<int>(param.enum_count) - 1);
}

} // namespace mutables_ui
```

### `common/midi_processor.h`

```cpp
#pragma once

#include <cstdint>

namespace mutables_ui {

class MIDIProcessor {
public:
    MIDIProcessor() = default;
    
    // Initialize with default MIDI channel
    // channel: 0 = Omni (all), 1-16 = specific channel
    void Init(int channel = 0) {
        midi_channel_ = channel;
        for (int i = 0; i < 128; i++) {
            cc_values_[i] = 0.0f;
        }
    }
    
    // Set MIDI channel filter
    void SetChannel(int channel) { midi_channel_ = channel; }
    int GetChannel() const { return midi_channel_; }
    
    // Check if event on given channel should be processed
    // event_channel: 0-15 (MIDI channel - 1)
    bool ShouldProcess(int event_channel) const {
        return (midi_channel_ == 0) || (event_channel == midi_channel_ - 1);
    }
    
    // CC value storage (normalized 0.0-1.0)
    float GetCC(int cc_num) const {
        if (cc_num < 0 || cc_num >= 128) return 0.0f;
        return cc_values_[cc_num];
    }
    
    void SetCC(int cc_num, uint8_t value) {
        if (cc_num >= 0 && cc_num < 128) {
            cc_values_[cc_num] = static_cast<float>(value) / 127.0f;
        }
    }
    
    // Get pointer to CC array (for CVMappingProcessor)
    const float* GetCCValues() const { return cc_values_; }
    
    // Build MIDI thru message bytes
    // Returns number of bytes written (0 if not a passthrough message type)
    static size_t BuildThruMessage(uint8_t status_type,  // NoteOn=0x90, etc
                                   uint8_t channel,      // 0-15
                                   uint8_t data0,
                                   uint8_t data1,
                                   uint8_t* out_bytes) {
        if (status_type == 0x80 || status_type == 0x90 || status_type == 0xB0) {
            out_bytes[0] = status_type | channel;
            out_bytes[1] = data0;
            out_bytes[2] = data1;
            return 3;
        }
        return 0;
    }
    
private:
    int midi_channel_ = 0;
    float cc_values_[128] = {0};
};

} // namespace mutables_ui
```

### `common/controllers/encoder_controller.h`

See Phase 5 description for interface design. Full implementation will be done during that phase.

---

## Manual Testing Checklist

After each phase, perform these manual tests on hardware:

### Basic Navigation
- [ ] Rotate encoder: parameters scroll correctly
- [ ] Scroll wraps at top and bottom
- [ ] Selected item is always visible

### Value Editing
- [ ] Short press on KNOB enters edit mode
- [ ] Rotate changes value smoothly
- [ ] Short/long press exits edit mode
- [ ] Value is clamped to min/max

### Enum Editing
- [ ] Short press on ENUM enters edit mode
- [ ] Rotate steps through options
- [ ] Wraps at first/last option

### SUB Menus
- [ ] Short press on SUB enters children
- [ ] Title shows "< ParentName"
- [ ] Selecting title exits SUB
- [ ] Hidden children are skipped
- [ ] Scroll works within SUB

### Mapping Submenu
- [ ] Long press opens mapping submenu
- [ ] Navigate through Mapping, CC#, Plugged, Attenuverter, Velocity
- [ ] CC# only shows when source=CC
- [ ] Plugged only shows when source=CV
- [ ] Long press exits submenu

### CV Mapping
- [ ] Map param to CV1, verify response
- [ ] Enable Plugged, adjust attenuverter
- [ ] Verify offset capture on Plugged enable
- [ ] Verify attenuverter scales correctly
- [ ] Map multiple params to same CV

### CC Mapping
- [ ] Map param to CC, send MIDI CC
- [ ] Verify smooth response
- [ ] Change CC number, verify new CC works

### V/Oct
- [ ] Map V/Oct to CV input
- [ ] Verify pitch tracking across 3+ octaves
- [ ] Verify MIDI notes work when V/Oct unmapped

### Presets
- [ ] Save preset with various mappings
- [ ] Modify all parameters
- [ ] Load preset, verify all restored
- [ ] Mappings work after load

### User Data
- [ ] Enter file browser for User Data param
- [ ] Navigate file list
- [ ] Select file, verify loads
- [ ] Select Default, verify firmware default loads

### CPU Performance
- [ ] Monitor CPU meter during playback
- [ ] Enable polyphony (4 voices)
- [ ] Use complex engine + filter
- [ ] Verify no CPU overload indicator

---

## Progress Tracking

| Phase | Status | Tests Written | Tests Passing | Manual Test |
|-------|--------|---------------|---------------|-------------|
| 0: Test Infrastructure | Not Started | - | - | - |
| 1: Extract Pure Functions | Not Started | - | - | - |
| 2: CV Mapping Processor | Not Started | - | - | - |
| 3: MIDI Processor | Not Started | - | - | - |
| 4: Parameter Templates | Not Started | - | - | - |
| 5: Encoder Controller | Not Started | - | - | - |
| 6: Update main.cpp | Not Started | - | - | - |
| 7: Update plaits_port.cpp | Not Started | - | - | - |

---

## Notes & Decisions

*Add notes here as implementation progresses*

- 

---

## References

- Current `main.cpp`: 1093 lines
- Current `plaits_port.cpp`: 1615 lines
- Current common library files: `parameter.h`, `ui_state.h`, `display.h`, `cv_input.h`, `preset_manager.h`, `constants.h`, `module_base.h`
