# Plaits Daisy - Development TODO

## Phase 1: Core Foundation ✓
- [x] Project setup with Makefile
- [x] Plaits DSP integration
- [x] Basic 4-knob control (freq, harmonics, timbre, morph)
- [x] Audio output working
- [x] Proper parameter scaling (matching original Plaits)
- [ ] Verify audio quality matches original
- [ ] Test all 24 engines work correctly

## Phase 2: Play Mode Complete
### Engine Selection ✓
- [x] Read B7 button state (GPIO on PORTB Pin 8)
- [x] Implement short press detection (< 2000ms)
- [x] Implement long press detection (> 2000ms)
- [x] Short press: increment engine (0-7, wrap within bank)
- [x] Long press: increment bank (0-2, wrap)
- [x] Store current engine/bank selection

### LED Feedback ✓
- [x] Single color LED (WP132XND)
- [x] Gamma-corrected 8-level brightness for engine display
- [x] Bank indication via pulse speed (3 different rates)
- [x] 2 second bank indication display on bank change

### Gate Inputs (TODO)
- [ ] Configure Gate In 1 as trigger input
- [ ] Configure Gate In 2 as accent/level input
- [ ] Map Gate In 1 to `modulations.trigger`
- [ ] Map Gate In 2 to `modulations.level`
- [ ] Set `modulations.trigger_patched` and `modulations.level_patched`

### CV Inputs - V/Oct + Modulation (TODO)
- [ ] Configure CV_5 as V/Oct input
- [ ] Calibrate V/Oct tracking (1V per octave)
- [ ] Add CV_5 pitch to patch.note
- [ ] Configure CV_6-8 as modulation inputs
- [ ] Add CV inputs to respective parameters

## Phase 3: Parameters Mode
### Mode Switching
- [ ] Read B8 switch state
- [ ] Implement mode state machine
- [ ] Visual feedback for mode change

### Page System
- [ ] Implement page counter (0-3)
- [ ] Page navigation with B7 short press
- [ ] Per-page long press actions
- [ ] LED indication of current page?

### Page 0: Attenuverters
- [ ] Map CV_1 to frequency_modulation_amount
- [ ] Map CV_2 to harmonics modulation (new feature)
- [ ] Map CV_3 to timbre_modulation_amount
- [ ] Map CV_4 to morph_modulation_amount
- [ ] Implement catch-up behavior (pot_controller style)

### Page 1: LPG / Envelope
- [ ] Map CV_1 to patch.decay
- [ ] Map CV_2 to patch.lpg_colour
- [ ] Implement envelope enable/disable toggle

### Page 2: Tuning
- [ ] Implement octave shift (-2 to +2)
- [ ] Implement fine tune control
- [ ] Long press: calibration mode entry

### Page 3: Extended Features
- [ ] External FM amount control
- [ ] FM ratio control
- [ ] LFO rate control
- [ ] LFO shape control

## Phase 4: Extended Features
### CV Output - LFO
- [ ] Implement basic LFO (sine)
- [ ] Add triangle waveform
- [ ] Add sawtooth waveform
- [ ] Add square waveform
- [ ] Add sample & hold
- [ ] Implement rate control
- [ ] Implement shape morphing
- [ ] Optional sync to Gate In 1

### Gate Outputs
- [ ] Gate Out 1: Envelope end-of-cycle
- [ ] Gate Out 2: Options (comparator/clock divider/LFO)

### External FM
- [ ] Read Audio In L
- [ ] Implement FM modulation of pitch
- [ ] FM amount and ratio controls
- [ ] Gain staging / level matching

### Audio Input Processing
- [ ] Audio In as external exciter (for physical models)
- [ ] Mix with Plaits output option

## Phase 5: Polish
### Settings Persistence
- [ ] Define settings structure
- [ ] Implement flash storage (QSPI)
- [ ] Save on parameter change (with debounce)
- [ ] Load on startup
- [ ] Factory reset option

### Calibration
- [ ] V/Oct calibration procedure
- [ ] Store calibration data
- [ ] CV input offset calibration

### LED Animations
- [ ] Smooth breathing effect
- [ ] Engine change animation
- [ ] Bank change animation
- [ ] Error indication
- [ ] Calibration mode indication

### Code Quality
- [ ] Refactor into separate files (ui.h, cv_scaler.h, etc.)
- [ ] Add comments and documentation
- [ ] Optimize for performance
- [ ] Memory usage analysis

## Future Ideas
- [ ] MIDI input support (USB or TRS)
- [ ] Preset system (save/recall patches)
- [ ] Randomization feature
- [ ] Micro-tuning / alternative scales
- [ ] Stereo output modes
- [ ] Expression pedal input
- [ ] Firmware update via audio

## Hardware Questions to Resolve
- [ ] LED type and capabilities
- [ ] CV input voltage ranges
- [ ] Gate input thresholds
- [ ] Audio input gain staging
- [ ] Normalization detection possibility

## Bugs / Issues
(Add issues as they're discovered)
- [ ] ...