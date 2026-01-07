# Mutable Instruments Daisy Port - Implementation Status

## Common Library (`common/`)

### ✅ Fully Implemented

#### `parameter.h` - Parameter System
- **Features:**
  - 5 parameter types: Continuous, Bipolar, Enum, Toggle, Integer
  - CV mapping structure with attenuverter simulation
  - Normalized value conversion (0.0-1.0)
  - Enum label retrieval
  - Parameter index calculation
- **Status:** Complete and functional

#### `ui_state.h` - UI State Machine
- **Features:**
  - 4 UI states: Navigate, EditValue, Submenu, SubmenuEdit
  - Menu scrolling with visible window (4 parameters)
  - Parameter navigation (next/prev with wraparound)
  - Submenu enter/exit logic
- **Status:** Complete

#### `cv_input.h` - CV Input Processing
- **Features:**
  - Attenuverter simulation with origin offset
  - CV scaling relative to captured origin point
  - One-pole lowpass filtering for noise reduction
  - CVInputBank managing 4 CV inputs
  - Filtered and raw value access
- **Status:** Complete and functional

#### `display.h` - OLED Display Rendering
- **Features:**
  - Main parameter menu rendering
  - CV mapping submenu rendering
  - Parameter value formatting (float, enum, toggle, bipolar)
  - Selection indicators (> symbol)
  - CV assignment display (CV1-4)
  - Editing mode highlighting (inverted text)
- **Status:** Complete

#### `module_base.h` - Module Interface
- **Features:**
  - Abstract base class for all module ports
  - Module identification (name, short name)
  - Lifecycle methods (Init, Process)
  - Parameter access interface
  - Optional gate/CV output handling
  - Optional MIDI handling
- **Status:** Complete

### ⚠️ Partially Implemented / Stubs

#### `preset_manager.h` - SD Card Preset System
- **Implemented:**
  - Preset structure definition
  - Init/Save/Load/List interface
- **Missing:**
  - ❌ SD card initialization
  - ❌ Directory creation/scanning
  - ❌ Binary or JSON serialization
  - ❌ File I/O operations
- **Status:** Interface defined, implementation TODO

---

## Plaits Port (`plaits/`)

### ✅ Fully Implemented

#### DSP Integration
- **Features:**
  - All 24 Plaits engines compiled and linked
  - Complete Plaits voice architecture
  - Buffer allocation system (32KB buffer)
  - Frame-based rendering (24 samples per block)
  - Audio conversion (int16 → float)
  - Engine switching at runtime
- **Engines:**
  - Original 16: Pair, WavTrn, FM, Grain, Addtv, WavTbl, Chord, Speech, Swarm, Noise, Partcl, String, Modal, Kick, Snare, HiHat
  - Extended 8: String Machine, VCF, Phase Dist, Six-Op, Wave Terrain, Chiptune
- **Status:** Complete

#### Parameter Mapping
- **Parameters (8 total):**
  1. Engine (enum, 24 options)
  2. Harmonics (continuous 0-1)
  3. Timbre (continuous 0-1)
  4. Morph (continuous 0-1)
  5. Frequency (continuous 0-1, maps to MIDI note)
  6. LPG Colour (continuous 0-1)
  7. LPG Decay (continuous 0-1)
  8. Level (continuous 0-1)
- **Status:** All core Plaits parameters mapped

#### Hardware Integration
- **Audio:**
  - ✅ 48kHz sample rate
  - ✅ 24-sample block processing
  - ✅ Stereo output (main + aux)
  - ✅ Audio callback integration
- **Gates:**
  - ✅ Gate input 1 → trigger
  - ❌ Gate input 2 not used
  - ❌ Gate outputs not implemented
- **Status:** Basic audio I/O working

#### UI Implementation (`main.cpp`)
- **Encoder Navigation:**
  - ✅ Rotate: scroll parameters
  - ✅ Short press: enter edit mode
  - ✅ Long press: enter/exit submenu
  - ✅ Parameter value editing with encoder
- **Display:**
  - ✅ Parameter list rendering
  - ✅ Real-time value updates
  - ✅ Selection indicators
- **Status:** Basic navigation functional

#### Build System
- **Features:**
  - ✅ Makefile with .cc file compilation
  - ✅ Bootloader support (BOOT_QSPI)
  - ✅ All Plaits source files included
  - ✅ Resource files (lookup tables)
  - ✅ User data stub (hardware-independent)
  - ✅ Optimization flags
- **Memory:**
  - QSPI Flash: 266KB / 7936KB (3.28%)
  - SRAM: 87KB / 512KB (16.59%)
- **Status:** Complete and optimized

### ⚠️ Partially Implemented

#### CV Input Integration
- **Implemented:**
  - ✅ Reading 4 knob values
  - ✅ CV filtering
  - ✅ Parameter update from CV mappings
- **Missing:**
  - ❌ Actual CV input reading (only knobs currently)
  - ❌ CV input normalization/scaling
  - ❌ Note: Hardware API shows `GetKnobValue()` but CV inputs need different API
- **Status:** Framework ready, hardware integration incomplete

#### CV Mapping Submenu
- **Implemented:**
  - ✅ Submenu state machine
  - ✅ Submenu rendering
  - ✅ Long press to enter
- **Missing:**
  - ❌ CV source selection (None/CV1-4)
  - ❌ Attenuverter editing
  - ❌ Origin capture button
  - ❌ Submenu navigation (up/down items)
  - ❌ Submenu value editing
- **Status:** UI designed, interaction logic TODO

#### Modulation Inputs
- **Implemented:**
  - ✅ Basic modulation structure
  - ✅ Trigger/level modulation
- **Missing:**
  - ❌ Frequency modulation from CV
  - ❌ Timbre modulation from CV
  - ❌ Morph modulation from CV
  - ❌ Modulation depth control
- **Status:** Structure ready, CV routing TODO

### ❌ Not Implemented

#### CV Outputs
- **Missing:**
  - ❌ CV output 1 & 2 not assigned
  - ❌ Envelope follower output
  - ❌ LFO output
  - ❌ Modulation signals output
- **Status:** Not started

#### Gate Outputs
- **Missing:**
  - ❌ Gate output 1 & 2 not assigned
  - ❌ Trigger output
  - ❌ Clock output
- **Status:** Not started

#### MIDI
- **Missing:**
  - ❌ MIDI input processing
  - ❌ Note on/off handling
  - ❌ CC parameter control
  - ❌ Pitch bend
  - ❌ MIDI clock
- **Status:** Not started

#### Presets
- **Missing:**
  - ❌ SD card initialization
  - ❌ Preset save/load
  - ❌ Preset browsing
  - ❌ Quick recall system
- **Status:** Not started

#### User Sample Loading
- **Missing:**
  - ❌ SD card sample reading
  - ❌ Sample bank loading
  - ❌ Wavetable loading
  - ❌ User wavetable support
- **Note:** Plaits can use user samples for some engines
- **Status:** Stubbed out

#### Additional Features
- **Missing:**
  - ❌ Calibration routine
  - ❌ Settings menu
  - ❌ Firmware version display
  - ❌ Parameter value smoothing
  - ❌ Screen saver
- **Status:** Not started

---

## Priority Implementation Order

### High Priority (Core Functionality)
1. **CV Input Hardware Integration** - Connect actual CV inputs to parameters
2. **CV Mapping Submenu Logic** - Make attenuverter configuration functional
3. **Gate Output Assignment** - Basic trigger/envelope output
4. **Parameter Smoothing** - Reduce zipper noise on parameter changes

### Medium Priority (Enhanced Functionality)
5. **MIDI Input** - Note on/off, CC control
6. **CV Outputs** - Envelope/modulation signals
7. **Preset System** - Save/load configurations
8. **Settings Menu** - Global configuration

### Low Priority (Nice to Have)
9. **User Sample Loading** - Custom wavetables/samples
10. **MIDI Clock** - Tempo sync
11. **Calibration** - Fine-tune CV scaling
12. **Screen Saver** - OLED burn-in protection

---

## Testing Checklist

### ✅ Verified Working
- Compilation and linking
- Bootloader integration
- Memory allocation
- Parameter system
- UI state machine
- Display rendering

### 🔄 Needs Testing on Hardware
- Audio output quality
- Encoder responsiveness
- Display refresh rate
- Gate input detection
- Knob reading
- Engine switching
- Parameter updates

### ❌ Cannot Test Yet (Not Implemented)
- CV input reading
- CV mapping workflow
- MIDI functionality
- Preset save/load
- Gate outputs
- CV outputs
