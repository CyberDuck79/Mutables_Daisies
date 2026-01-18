# Code Refactoring Plan

**Last Updated:** 2026-01-18  
**Status:** Planning Phase

---

## 📊 File Overview

| File | Lines | Status | Notes |
|------|-------|--------|-------|
| `plaits/plaits_port.cpp` | 1511 | ⚠️ Too large | Main module logic |
| `plaits/main.cpp` | 990 | ⚠️ Too large | Entry point + UI handling |
| `common/display.h` | 717 | ⚠️ Too large | God class |
| `common/ui_state.h` | 649 | ⚠️ Consider splitting | God class |
| `common/preset_manager.h` | 509 | ✅ OK | Could extract helpers |
| `plaits/cv_modulator.h` | 496 | ✅ OK | Minor improvements |
| `common/parameter.h` | 357 | ✅ OK | - |
| `plaits/audio_processors.h` | 330 | ✅ OK | - |

---

## 🔴 Phase 1: Constants & Foundation

### 1.1 Create `common/constants.h` ✅ DONE
- [x] Audio constants (sample rate, buffer size, scaling factors)
- [x] MIDI constants (C4 note, CC limits)
- [x] UI constants (screen dimensions, line heights, column positions)
- [x] Timing constants (trigger duration, delays)
- [x] Mathematical constants (ln10, etc.)

**Files updated:**
- `common/constants.h` - Created with all constants
- `plaits/plaits_port.cpp` - Uses kMidiNoteC4, kDefaultSampleRate, kLn10, kAudioScaleInt16, kTriggerDurationS, kCVCenter, kVOctRange, kEnvThreshold
- `plaits/main.cpp` - Uses kCVHysteresis, kMessageDisplayDelayMs, kEncoderStepMedium
- `plaits/audio_processors.h` - Uses kMsToSeconds, kDefaultSampleRate, kDefaultRelease, kMinThreshold
- `plaits/cv_modulator.h` - Uses kEnvNearZero
- `common/display.h` - Includes constants.h

### 1.2 Split `Display` class (720 lines → 4 files) ✅ DONE
- [x] `common/display.h` - Main Display class with composition (150 lines)
- [x] `common/renderers/menu_renderer.h` - Main menu + SUB rendering (245 lines)
- [x] `common/renderers/mapping_submenu_renderer.h` - KNOB/CV/ENUM mapping submenus (256 lines)
- [x] `common/renderers/preset_renderer.h` - Char input, preset list, file browser (200 lines)

**Result:** Single 720-line file → 4 focused files, largest is 256 lines

### 1.3 Extract `MenuState` enums & constants ✅ DONE
- [x] `common/state/ui_enums.h` - Extracted enums (UIState, KnobSubmenuItem, etc.) + constants (50 lines)
- [x] `common/state/navigation_state.h` - Reference implementation for navigation (49 lines)
- [x] `common/state/submenu_state.h` - Reference implementation for submenu/SUB (275 lines)
- [x] `common/state/preset_state.h` - Reference implementation for preset UI (240 lines)
- [x] `common/ui_state.h` - Updated to include ui_enums.h, re-exports constants (621 lines)

**Note:** Full struct decomposition deferred - existing code uses direct member access (`menu.selected_param`).
The component files serve as **reference implementations** for future modular ports.

---

## 🟡 Phase 2: Function Extraction

### 2.1 `plaits_port.cpp` - SetupParameters() ✅ SKIPPED
- Already well-organized (128 lines) with helper functions:
  - `SetupCVOutParams()` - CV Output submenu setup
  - `SetupAudioInParams()` - Audio Input submenu setup
- Further splitting would add artificial complexity

### 2.2 `plaits_port.cpp` - Process() ✅ SKIPPED
- Well-organized (236 lines) with clear section comments
- Tightly coupled stages share state (`frames[]`, `patch_`, `modulations_`)
- Extraction would require passing 5+ parameters per function
- Decision: Keep as-is, code is already readable

### 2.3 `main.cpp` - UpdateEncoder() ✅ DONE
- [x] Created `plaits/encoder_handlers.h` (560 lines) with:
  - `CycleMappingSource()` - Mapping source cycling helper
  - `HandleNavigate()` - Main menu navigation
  - `HandleEditValue()` - Value editing
  - `HandleSubmenu()` - Submenu navigation
  - `HandleSubmenuEdit()` - Submenu value editing
  - `HandleCharInput()` - Preset name input
  - `HandlePresetList()` - Preset loading
  - `HandleFileBrowser()` - File browsing

**Note:** Handlers are available for reuse but main.cpp still uses inline switch for audio restart control.

### 2.4 `main.cpp` - AudioCallback() ✅ SKIPPED
- Already well-organized with parameter mapping in-place
- Moving to separate function would require passing many globals
- Audio callback needs minimal latency, inline is appropriate

---

## 🟢 Phase 3: Deduplication

### 3.1 Create `ParameterFormatter` utility
**Duplicated in:** plaits_port.cpp lines 174-227, 317-350, 520-573
- [ ] `FormatLogTime(value, min_ms, scale)` - Attack/Release time formatting
- [ ] `FormatGainDB(value)` - Decibel formatting
- [ ] `FormatPercent(value)` - Percentage formatting

### 3.2 Create `ListNavigator` utility
**Duplicated in:** ui_state.h (6 pairs of Prev/Next functions)
- [ ] Generic `Next(count, wrap)` / `Prev(count, wrap)`
- [ ] `ScrollToSelected(visible_count)`
- [ ] Use for: params, submenu items, SUB children, presets, files, characters

### 3.3 Data-driven visibility callbacks
**Duplicated in:** plaits_port.cpp (6 similar visibility callbacks)
- [ ] Create visibility configuration table
- [ ] Single generic visibility checker function

### 3.4 Unified parameter mapping helper
**Duplicated in:** main.cpp lines 138-230 (root params and SUB children)
- [ ] `ApplyParameterMapping(param, cv_value, cc_value)`

### 3.5 Base `ClockTracker` class
**Duplicated in:** cv_modulator.h (MIDIClockTracker & GateClockTracker)
- [ ] Abstract base with `OnTick()`, `GetHz()`, `IsActive()`
- [ ] Derived classes for MIDI and Gate edge detection

---

## 📝 Progress Log

### 2026-01-18
- [x] Initial analysis completed
- [x] Created refactoring plan document
- [ ] Phase 1 not started

---

## 🎯 Success Metrics

After refactoring:
- No file > 500 lines (except generated code)
- No function > 50 lines
- All magic numbers in constants.h
- Single responsibility per class
- DRY - no copy-pasted code blocks

---

## 📝 Progress Log

### 2026-01-18
- [x] Initial analysis completed
- [x] Created refactoring plan document
- [x] **Phase 1.1 DONE** - Created `common/constants.h` and updated 6 files

---

## ⚠️ Risk Notes

- **Audio callback changes** - Must maintain real-time safety, no allocations
- **Preset compatibility** - Don't break existing preset format
- **Testing** - Build and test after each phase
- **Git commits** - Commit after each sub-task for easy rollback
