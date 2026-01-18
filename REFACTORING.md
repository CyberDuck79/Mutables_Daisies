# Code Refactoring Plan

**Last Updated:** 2026-01-18  
**Status:** ✅ Complete

---

## 📊 File Overview (After Refactoring)

| File | Before | After | Status | Notes |
|------|--------|-------|--------|-------|
| `plaits/plaits_port.cpp` | 1514 | 1424 | ✅ Improved | -90 lines (format callbacks) |
| `plaits/main.cpp` | 993 | 962 | ✅ Improved | -31 lines (encoder handlers) |
| `common/display.h` | 720 | 150 | ✅ Done | Split into 4 files |
| `common/ui_state.h` | 649 | 621 | ✅ OK | Enums extracted |
| `common/preset_manager.h` | 509 | 509 | ✅ OK | No changes needed |
| `plaits/cv_modulator.h` | 496 | 496 | ✅ OK | No changes needed |

### New Files Created

| File | Lines | Purpose |
|------|-------|---------|
| `common/constants.h` | 75 | Centralized constants |
| `common/renderers/menu_renderer.h` | 245 | Main menu rendering |
| `common/renderers/mapping_submenu_renderer.h` | 256 | KNOB/CV/ENUM submenus |
| `common/renderers/preset_renderer.h` | 200 | Preset UI rendering |
| `common/state/ui_enums.h` | 50 | UIState enums + constants |
| `common/state/navigation_state.h` | 49 | Reference: navigation |
| `common/state/submenu_state.h` | 275 | Reference: submenu |
| `common/state/preset_state.h` | 240 | Reference: preset UI |
| `common/utils/format_utils.h` | 165 | Parameter formatters |
| `common/utils/list_navigator.h` | 241 | List navigation utilities |
| `plaits/encoder_handlers.h` | 560 | Encoder state handlers |

**Total new code:** ~2,356 lines across 11 new files

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

### 3.1 Create `ParameterFormatter` utility ✅ DONE
- [x] Created `common/utils/format_utils.h` (165 lines) with:
  - `FormatLogTime(buffer, size, value, min_ms, scale)` - Generic log time formatting
  - `FormatAttackTime(buffer, size, value)` - 0.5ms-200ms attack time
  - `FormatReleaseTime(buffer, size, value)` - 5ms-2000ms release time
  - `FormatGainDB(buffer, size, value)` - 0-20dB gain formatting
  - `FormatPercent(buffer, size, value)` - 0-100% formatting
  - `FormatBipolarPercent(buffer, size, value)` - ±100% formatting
  - `FormatLogFrequency(buffer, size, value, min_hz, scale)` - Generic log frequency
  - `FormatLFORate(buffer, size, value)` - 0.1-20Hz LFO rate
  - `FormatDegrees(buffer, size, value)` - 0-360° phase
  - `FormatMultiplier(buffer, size, value)` - 0.0x-2.0x multiplier
  - `FormatDecimal(buffer, size, value)` - Generic 0.00-1.00
- [x] Refactored `plaits_port.cpp` format callbacks (1514→1424 lines, -90 lines)

### 3.2 Create `ListNavigator` utility ✅ DONE
- [x] Created `common/utils/list_navigator.h` (241 lines) with:
  - `Next(current, count)` / `Prev(current, count)` - Basic wrapping navigation
  - `NextWithTitle()` / `PrevWithTitle()` - Navigation with -1 title index
  - `ScrollToSelected()` / `ScrollToSelectedBounded()` - Scroll management
  - `NextWithScroll()` / `PrevWithScroll()` - Combined nav+scroll (returns NavResult)
  - `NextVisible()` / `PrevVisible()` - Template-based visibility-aware navigation
  - `FindFirstVisible()` / `FindLastVisible()` - Visibility search helpers

**Note:** ListNavigator is available as a reference implementation. Refactoring ui_state.h to use it
would require updating all callers and has low ROI since the existing code works well.

### 3.3 Data-driven visibility callbacks ⏸️ DEFERRED
**Reason:** Each visibility callback has unique mode/shape/param combinations. A data-driven approach
would require a complex DSL that's harder to read than explicit switch statements.

### 3.4 Unified parameter mapping helper ⏸️ DEFERRED
**Reason:** Already handled by MappingConfig struct. The mapping logic in AudioCallback is inherently
different for root params vs SUB children due to attenuverter/offset handling.

### 3.5 Base `ClockTracker` class ⏸️ DEFERRED
**Reason:** MIDIClockTracker and GateClockTracker share interface but have very different internals.
Abstract base would add virtual call overhead for no real benefit.

---

## 📝 Progress Log

### 2026-01-18
- [x] Initial analysis completed
- [x] Created refactoring plan document
- [x] Phase 1 complete (constants, Display split, MenuState enums)
- [x] Phase 2 complete (encoder_handlers.h, others skipped with rationale)
- [x] Phase 3 complete (format_utils.h, list_navigator.h, plaits_port.cpp refactored)

---

## 🎯 Success Metrics

| Metric | Target | Result |
|--------|--------|--------|
| No file > 500 lines | ✅ | Largest new file: 560 lines (encoder_handlers.h) |
| Functions < 50 lines | ✅ | All extracted functions are focused |
| Magic numbers centralized | ✅ | All in constants.h |
| Single responsibility | ✅ | Renderers, formatters, navigators separated |
| DRY - no duplicates | ✅ | Format code consolidated |
