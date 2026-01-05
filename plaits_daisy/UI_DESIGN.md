# Plaits Daisy - UI Design Document

## Hardware Available (Patch.Init())

### Inputs
| Control | Type | Notes |
|---------|------|-------|
| CV_1-4 | Knob + CV | 4 knobs with normalled CV jacks |
| CV_5-8 | CV Input | 4 additional CV input jacks |
| B7 | Momentary Button | User button |
| B8 | Toggle Switch | 2-position switch |
| Gate In 1 | Gate | Digital input |
| Gate In 2 | Gate | Digital input |
| Audio In L | Audio | Left audio input |
| Audio In R | Audio | Right audio input |

### Outputs
| Output | Type | Notes |
|--------|------|-------|
| Audio Out L | Audio | Main output |
| Audio Out R | Audio | Aux output |
| CV Out | CV | Analog output (0-5V) |
| Gate Out 1 | Gate | Digital output |
| Gate Out 2 | Gate | Digital output |
| LED | RGB? | Status indicator |

---

## Plaits Engine Structure

### Banks and Engines (24 total)
| Bank | Engine | Name | Description |
|------|--------|------|-------------|
| **0: Classic** | 0 | Virtual Analog | VA oscillator with crossfading waveforms |
| | 1 | Waveshaping | Waveshaper with wavetable |
| | 2 | FM | 2-operator FM |
| | 3 | Grain | Granular formant oscillator |
| | 4 | Additive | Harmonic oscillator |
| | 5 | Wavetable | Wavetable with morph |
| | 6 | Chord | Chord generator |
| | 7 | Speech | Speech synthesis |
| **1: New** | 8 | Swarm | Swarm of 8 sawtooth waves |
| | 9 | Noise | Filtered noise |
| | 10 | Particle | Particle noise |
| | 11 | String | Inharmonic string model |
| | 12 | Modal | Modal resonator |
| | 13 | Bass Drum | Analog bass drum |
| | 14 | Snare Drum | Analog snare drum |
| | 15 | Hi-Hat | Analog hi-hat |
| **2: Aux** | 16-23 | (Additional engines) | User wavetables, etc. |

---

## UI Modes

### Mode Selection: B8 Toggle Switch

```
┌─────────────────────────────────────────────────────────────┐
│  B8 UP (Play Mode)          │  B8 DOWN (Parameters Mode)   │
├─────────────────────────────┼───────────────────────────────┤
│  Performance-focused        │  Deep editing / modulation   │
│  Quick engine selection     │  Page-based parameter access │
└─────────────────────────────┴───────────────────────────────┘
```

---

## Play Mode (B8 Up)

### Knob Mapping
| Knob | Parameter | Range | Notes |
|------|-----------|-------|-------|
| CV_1 | Frequency | C0-C8 | Pitch control |
| CV_2 | Harmonics | 0-1 | Engine-specific |
| CV_3 | Timbre | 0-1 | Engine-specific |
| CV_4 | Morph | 0-1 | Engine-specific |

### CV Input Mapping (CV_5-8)
| Input | Parameter | Notes |
|-------|-----------|-------|
| CV_5 | V/Oct Pitch | 1V/octave tracking |
| CV_6 | Timbre CV | Added to knob (with attenuverter) |
| CV_7 | Morph CV | Added to knob (with attenuverter) |
| CV_8 | Harmonics / Level | Harmonics in Drone/Ping, Level CV in External mode |

### Gate Input Mapping
| Input | Function |
|-------|----------|
| Gate In 1 | Trigger (strikes internal envelope in Ping/External modes) |
| Gate In 2 | (Reserved for future use) |

### Button Actions (B7)
| Action | Function |
|--------|----------|
| Short press | Next engine (0-7 within bank) |
| Long press (2s) | Next bank (0-2) |
| Double press? | Previous engine? |

### LED Feedback
| State | LED Behavior |
|-------|--------------|
| Bank 0 (Classic) | Slow pulse (2s period) |
| Bank 1 (New) | Medium pulse (1s period) |
| Bank 2 (Aux) | Fast pulse (0.5s period) |
| Engine change | Brief flash |

---

## Parameters Mode (B8 Down)

### Page Navigation
- **Short press B7**: Next page (cycles through pages)
- **Long press B7**: Page-specific toggle action

### Page 0: Attenuverters
Controls CV modulation amounts (how much CV_6-8 affect parameters)

| Knob | Parameter | Range | Notes |
|------|-----------|-------|-------|
| CV_1 | FM Amount | -1 to +1 | Frequency modulation depth (internal) |
| CV_2 | Timbre Mod | -1 to +1 | CV_6 → Timbre attenuverter |
| CV_3 | Morph Mod | -1 to +1 | CV_7 → Morph attenuverter |
| CV_4 | Harmonics Mod | -1 to +1 | CV_8 → Harmonics attenuverter (Drone/Ping only) |

**Long press toggle**: Reset attenuverters to defaults (0)

### Page 1: LPG / Envelope / Output
Controls the internal Low-Pass Gate, envelope mode, and output level

| Knob | Parameter | Range | Notes |
|------|-----------|-------|-------|
| CV_1 | Decay | 0-1 | Envelope decay time |
| CV_2 | LPG Colour | 0-1 | VCA (0) → VCFA → VCF (1) |
| CV_3 | Output Level | 0-1 | Master volume (0.1x to 1.0x) |
| CV_4 | (Reserved) | | |

**Long press toggle**: Cycle envelope mode (Drone → Ping → External)

#### Envelope Modes
| Mode | Gate In 1 | CV_8 | Behavior |
|------|-----------|------|----------|
| **Drone** | Ignored | Harmonics CV | Always plays, no envelope (LPG bypassed) |
| **Ping** | Trigger | Harmonics CV | Internal ping envelope, percussive response |
| **External** | Trigger | Level CV | LPG follows CV_8 level, for external envelope/VCA |

### Page 2: Tuning / Calibration
| Knob | Parameter | Range | Notes |
|------|-----------|-------|-------|
| CV_1 | Octave Range | 0-8 | Knob range setting (see below) |
| CV_2 | Fine Tune | -1 to +1 | Fine pitch adjustment (±1 semitone) |
| CV_3 | (Reserved) | | |
| CV_4 | (Reserved) | | |

**Long press toggle**: Enter calibration mode -> output continuous sine 

#### Frequency Knob Range Settings
| Value | Root Note | Knob Range | Notes |
|-------|-----------|------------|-------|
| 0 | C0 (MIDI 12) | ±7 semitones | LFO / Sub-bass |
| 1 | C1 (MIDI 24) | ±7 semitones | Bass |
| 2 | C2 (MIDI 36) | ±7 semitones | Low |
| 3 | C3 (MIDI 48) | ±7 semitones | Mid-low |
| 4 | **C4 (MIDI 60)** | **±7 semitones** | **Default - Middle C** |
| 5 | C5 (MIDI 72) | ±7 semitones | Mid-high |
| 6 | C6 (MIDI 84) | ±7 semitones | High |
| 7 | C7 (MIDI 96) | ±7 semitones | Very high |
| 8 | Full Range | C0 to C8 | 8 octaves, no V/Oct needed |

With V/Oct (CV_5): Adds -3 to +7 octaves relative to root note.

### Page 3: Extended Features (Daisy-specific)
New features using Daisy's extra I/O

| Knob | Parameter | Range | Notes |
|------|-----------|-------|-------|
| CV_1 | Ext FM Amount | 0-1 | External FM from Audio In L |
| CV_2 | Ext FM Ratio | 0.5-4 | FM frequency ratio |
| CV_3 | LFO Rate | 0.1-20 Hz | Internal LFO for CV Out |
| CV_4 | LFO Shape | 0-1 | Sine → Triangle → Saw → Square |

**Long press toggle**: LFO sync to Gate In 1?

---

## Additional Outputs

### CV Out - Internal LFO
- Configurable LFO output (Page 3 parameters)
- Rate: 0.1 Hz to 20 Hz
- Shapes: Sine, Triangle, Saw, Square, S&H
- Optional sync to Gate In 1

### Gate Out 1 - Envelope EOC
- End-of-cycle pulse from internal envelope
- Useful for self-patching or sequencing

### Gate Out 2 - Comparator / Clock
Options:
- Audio-rate comparator (turns audio into gate)
- Divided clock from Gate In 1
- LFO square wave output

---

## Extended Features (Daisy Advantages)

### External FM (Audio Input)
Use Audio In L/R for frequency modulation:
- Audio In L → External FM carrier
- Amount controlled in Parameters Mode Page 3

### Through-Zero FM
The extra CPU power could enable:
- True through-zero FM
- Linear FM option

### Stereo Processing
- Audio In L/R could be mixed with Plaits output
- Or used as external exciter for physical models

---

## Implementation Phases

### Phase 1: Core (Current)
- [x] Basic 4-knob control
- [x] Single engine working
- [ ] All engines accessible
- [ ] Basic LED feedback

### Phase 2: Play Mode Complete
- [ ] Engine/bank selection with B7
- [ ] Gate inputs (trigger/accent)
- [ ] V/Oct CV input (CV_5)
- [ ] LED bank indication

### Phase 3: Parameters Mode
- [ ] Mode switching with B8
- [ ] Page navigation
- [ ] Attenuverter page
- [ ] LPG page

### Phase 4: Extended Features
- [ ] CV Out LFO
- [ ] Gate outputs
- [ ] External FM input
- [ ] Tuning/calibration page

### Phase 5: Polish
- [ ] Settings save/recall (flash storage)
- [ ] LED animations
- [ ] Fine-tuning of all parameters

---

## Open Questions

1. **LED capabilities**: Is it RGB or single color? This affects feedback options.

2. **CV_5-8 normalization detection**: Can we detect if a jack is patched? 
   (Original Plaits does this to disable internal modulation when CV is patched)

3. **Audio input gain staging**: What's the expected input level? Need to match for FM.

4. **Trigger threshold**: What voltage threshold for gate inputs?

5. **Additional pages**: What other parameters would be useful?
   - Engine-specific parameters?
   - Randomization?
   - Preset save/load?

---

## Control Summary Table

### Play Mode
| Control | Short | Long |
|---------|-------|------|
| B7 | Next engine | Next bank |
| CV_1 | Frequency | - |
| CV_2 | Harmonics | - |
| CV_3 | Timbre | - |
| CV_4 | Morph | - |

### Parameters Mode  
| Control | Page 0 | Page 1 | Page 2 | Page 3 |
|---------|--------|--------|--------|--------|
| CV_1 | FM Amt | Decay | Octave Range | Ext FM Amt |
| CV_2 | Timbre Mod | LPG Colour | Fine Tune | FM Ratio |
| CV_3 | Morph Mod | Output Level | - | LFO Rate |
| CV_4 | Harm Mod | - | - | LFO Shape |
| B7 short | Next page | Next page | Next page | Next page |
| B7 long | Reset Atten | Env Mode | V/Oct Cal | LFO Sync |