# Mutable Instruments Daisy Port - Implementation Status

## Architecture Overview

### Design Principles
1. **MIDI over V/Oct**: Use MIDI for pitch/gate instead of CV, gaining polyphony and velocity
2. **Unified Attenuverter Emulation**: Since Daisy Patch sums knob+CV before ADC, we capture offset on "plugged" activation
3. **Original Firmware Compatibility**: Pass plugged/attenuverter status to original code when supported
4. **Flexible Mapping**: Any parameter can be mapped to CV, Gate, or MIDI CC

---

## Common Library (`common/`)

### Parameter System (`parameter.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| KNOB type (continuous) | ✅ Done | Basic implementation |
| KNOB attenuverter | ⚠️ Partial | Offset capture TODO |
| KNOB velocity mod | ❌ TODO | |
| CV type (direct) | ⚠️ Partial | Read-only display TODO |
| ENUM type | ✅ Done | Gate mapping TODO |
| ENUM gate trigger modes | ❌ TODO | rise/fall/both |
| ENUM gate actions | ❌ TODO | ++/--/+-/-+ |
| MIDI type (channel) | ❌ TODO | |
| SUB type (submenu container) | ❌ TODO | |
| SAVE type | ❌ TODO | Character input UI |
| LOAD type | ❌ TODO | Preset list browser |

### CV Input Processing (`cv_input.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| Raw value reading | ✅ Done | |
| Lowpass filtering | ✅ Done | Reduces noise |
| ADC range scaling | ✅ Done | 0.03-0.96 → 0.0-1.0 |
| Snap-to-edge | ✅ Done | <0.01→0, >0.99→1 |
| Hysteresis | ✅ Done | 0.1% threshold |
| Offset capture | ⚠️ Partial | Need UI integration |
| Attenuverter calculation | ⚠️ Partial | Formula implemented |

### UI State Machine (`ui_state.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| Navigate state | ✅ Done | |
| EditValue state | ✅ Done | |
| Submenu state | ⚠️ Partial | Basic structure |
| SubmenuEdit state | ❌ TODO | |
| Parameter scrolling | ✅ Done | 4 visible parameters |
| Long press detection | ✅ Done | 500ms threshold |

### Display Rendering (`display.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| Parameter list | ✅ Done | Font_7x10 |
| Value formatting | ✅ Done | Custom X.XX format |
| Selection indicator | ✅ Done | Underline |
| Edit highlighting | ✅ Done | Inverted text |
| Mapping indicator | ⚠️ Partial | CV only, need Gate/CC |
| Boot screen | ✅ Done | 3 second splash |
| Submenu rendering | ⚠️ Partial | Basic structure |
| Character input UI | ❌ TODO | For SAVE |
| Preset list UI | ❌ TODO | For LOAD |
| Error messages | ❌ TODO | SD card errors |

### Preset Manager (`preset_manager.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| SD card detection | ❌ TODO | |
| Directory creation | ❌ TODO | `<module>/presets/` |
| Preset save | ❌ TODO | Binary serialization |
| Preset load | ❌ TODO | |
| Preset listing | ❌ TODO | |

### Module Base (`module_base.h`)

| Feature | Status | Notes |
|---------|--------|-------|
| Abstract interface | ✅ Done | |
| Parameter access | ✅ Done | |
| Gate I/O | ✅ Done | |
| MIDI handling | ⚠️ Partial | Note on/off only |

---

## Plaits Port (`plaits/`)

### DSP Integration

| Feature | Status | Notes |
|---------|--------|-------|
| All 24 engines | ✅ Done | Compiled and linked |
| Voice architecture | ✅ Done | |
| Buffer allocation | ✅ Done | 32KB |
| Audio rendering | ✅ Done | 24 samples/block |
| Engine switching | ✅ Done | Runtime |

### Parameter Mapping

| Parameter | Type | CV Map | Status |
|-----------|------|--------|--------|
| Bank | ENUM | - | ✅ Done |
| Engine | ENUM | - | ✅ Done |
| Harmonics | KNOB | CV2 | ✅ Done |
| Timbre | KNOB | CV3 | ✅ Done |
| Morph | KNOB | CV4 | ✅ Done |
| Transpose | KNOB | CV1 | ✅ Done (±12 semi) |
| LPG Colour | KNOB | - | ✅ Done |
| LPG Decay | KNOB | - | ✅ Done |
| Level | KNOB | - | ✅ Done |

### Engine Banks

| Bank | Engines | Status |
|------|---------|--------|
| Synth | VA, WavShp, FM, Grain, Addtv, WavTbl, Chord, Speech | ✅ Done |
| Drum | Swarm, Noise, Partcl, String, Modal, Kick, Snare, HiHat | ✅ Done |
| New | VA VCF, PhasDs, 6-Op×3, WavTrn, StrMch, Chip | ✅ Done |

### Hardware I/O

| Feature | Status | Notes |
|---------|--------|-------|
| Audio out 1 (Main) | ✅ Done | Plaits OUT |
| Audio out 2 (Aux) | ✅ Done | Plaits AUX |
| Audio out 3-4 | ✅ Done | Cleared (silent) |
| Gate input 1 | ✅ Done | Trigger |
| MIDI input (TRS) | ✅ Done | Note on/off |
| Encoder | ✅ Done | Fast 1ms polling |
| Display | ✅ Done | 60Hz refresh |

### MIDI Implementation

| Feature | Status | Notes |
|---------|--------|-------|
| Note On → Pitch + Gate | ✅ Done | Monophonic |
| Note Off → Gate release | ✅ Done | Same note check |
| Velocity | ❌ TODO | For accent/level |
| CC mapping | ❌ TODO | |
| Channel selection | ❌ TODO | |
| Polyphony | ❌ TODO | |

### TODO for Plaits

| Feature | Priority | Notes |
|---------|----------|-------|
| MIDI channel parameter | High | Add MIDI type param |
| Velocity → Level/Accent | High | |
| CC → Parameters | Medium | |
| CV Output config (SUB) | Medium | Envelope/LFO selection |
| Gate mapping for Bank/Engine | Medium | |
| SAVE/LOAD presets | Medium | |
| Click reduction | Low | Investigate further |

---

## Implementation Priority

### Phase 1: Core Parameter System (Current)
- [x] Basic parameter types working
- [x] MIDI note input (TRS jack)
- [x] Bank/Engine system
- [ ] **KNOB submenu: plugged + offset capture**
- [ ] **KNOB submenu: attenuverter editing**
- [ ] **Mapping indicator display (Gate/CC)**

### Phase 2: Enhanced Mapping
- [ ] ENUM gate trigger modes (rise/fall/both)
- [ ] ENUM gate actions (++/--/+-/-+)
- [ ] MIDI CC mapping
- [ ] MIDI type parameter (channel selection)
- [ ] Velocity modulation

### Phase 3: Preset System
- [ ] SD card initialization
- [ ] SAVE parameter type + character input UI
- [ ] LOAD parameter type + preset browser UI
- [ ] Preset serialization

### Phase 4: Advanced Features
- [ ] SUB parameter type
- [ ] CV output configuration for Plaits
- [ ] Polyphonic MIDI
- [ ] MIDI clock → Gate output

---

## Memory Usage (Plaits)

| Region | Used | Total | % |
|--------|------|-------|---|
| QSPI Flash | 271KB | 7936KB | 3.3% |
| SRAM | 87KB | 512KB | 16.6% |
| RAM_D2_DMA | 17KB | 32KB | 52% |

---

## Testing Status

### ✅ Verified Working
- Audio output (out 1 & 2)
- Encoder navigation
- Display rendering
- Gate input triggering
- MIDI input (TRS jack)
- All 24 engines
- Bank switching
- Parameter editing
- Transpose (±12 semitones)

### 🔄 Needs Testing
- CV input with attenuverter emulation
- Long envelope retrigger (clicks reported)
- Harmonics parameter effect (reported not working - may be engine-dependent)

### ❌ Not Yet Testable
- Preset save/load
- Gate mapping for enums
- MIDI CC control
- Velocity modulation
