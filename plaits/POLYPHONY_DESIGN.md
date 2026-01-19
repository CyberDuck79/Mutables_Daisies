# Polyphony Implementation Design for Plaits Port

## Overview

This document outlines the polyphony implementation strategy for lightweight Plaits engines on the Daisy Patch platform.

## Memory Budget

**STM32H750 Memory Available:**
- RAM_D1 (AXI SRAM): 512KB @ 0x24000000
- RAM_D2: 256KB @ 0x30000000  
- DTCM: 128KB @ 0x20000000
- SDRAM: 64MB (external)

**Current Usage:**
- Plaits Voice buffer: 32KB
- UI/Display: ~20KB
- Audio buffers: ~10KB
- Other state: ~30KB
- **Total: ~90KB (17% of RAM_D1)**

**Polyphony Budget (4 voices):**
- 4 × Voice buffer: 128KB
- Voice manager state: <1KB
- Output mixing buffers: <1KB
- **Total: ~130KB additional**

**Post-polyphony: ~220KB (~43% of RAM_D1)** ✅

## Lightweight Engines (Support Polyphony)

These engines have minimal memory requirements (~96-256 bytes allocator usage):

| Engine Index | Name | Bank | Buffer Usage |
|--------------|------|------|--------------|
| 0 | VA+VCF | New | 96B |
| 1 | Phase Distortion | New | 96B |
| 5 | Wave Terrain | New | 96B |
| 8 | Virtual Analog | Synth | 96B |
| 9 | Waveshaping | Synth | 96B |
| 10 | FM 2-op | Synth | 96B |
| 13 | Wavetable | Synth | 256B |

## Heavy Engines (Monophonic Only)

| Engine Index | Name | Buffer Usage | Reason |
|--------------|------|--------------|--------|
| 18 | Particle | ~16KB | Diffuser delay line |
| 19 | String | ~12KB | Multiple delay lines |
| 20 | Modal | ~8KB | Resonator banks |

## Architecture

### Option A: Multiple Voice Instances (Recommended)
- Create 4 `plaits::Voice` instances
- Each voice has its own 32KB buffer
- Manager routes NoteOn/NoteOff to voices
- Final output is mixed with gain normalization

**Pros:**
- Full engine compatibility
- Clean separation
- All engine features preserved

**Cons:**
- 128KB buffer memory
- Need to sync engine parameters across voices

### Option B: Single Voice with Note Buffer
- Keep single Voice instance
- Buffer note events internally
- Render each note sequentially, mix outputs

**Pros:**
- Minimal memory
- Simpler integration

**Cons:**
- 4× CPU usage for rendering
- Complex timing management
- Engine state conflicts

## Implementation Plan

### Phase 1: Voice Manager Infrastructure ✅
- [x] Create `PolyphonicVoiceManager` class
- [x] Implement steal-oldest allocation
- [x] Create `PolyphonicMixer` for output mixing

### Phase 2: Multi-Voice Integration ✅
- [x] Add 4 Voice instances to PlaitsPort
- [x] Initialize with separate buffers (32KB each)
- [x] Route NoteOn/NoteOff through manager
- [x] Enable/disable based on current engine

### Phase 3: Render Pipeline ✅
- [x] Modify Process() for multi-voice render (RenderPolyphonicVoices)
- [x] Mix outputs with sqrt(N) gain normalization  
- [x] Handle per-voice parameter setup

### Phase 4: UI & Settings ✅
- [x] Add "Voices" parameter to Settings menu (1-4)
- [x] AllNotesOff and Panic methods for MIDI
- [ ] Display active voice count on UI (optional)
- [ ] Save/load polyphony setting in presets (automatic)

## Voice Allocation Strategy: Steal-Oldest

**Why steal-oldest over round-robin:**
1. More musical for sustained playing
2. Preserves recently played notes
3. Standard in classic polysynths (DX7, etc.)
4. Better for pads and chords

**Algorithm:**
```
NoteOn(note):
  1. Check if note already playing → retrigger that voice
  2. Find free voice → use it
  3. Find oldest RELEASED voice → steal it
  4. Find oldest HELD voice → steal it (last resort)
```

## Gain Normalization

Using sqrt(N) scaling:
```cpp
float gain = 1.0f / sqrt(active_voice_count);
```

This preserves perceived loudness while avoiding clipping:
- 1 voice: gain = 1.0
- 2 voices: gain = 0.707
- 4 voices: gain = 0.5

## Performance Considerations

**CPU Budget @ 48kHz, 24-sample blocks:**
- 1 voice: ~25-30% CPU (baseline)
- 2 voices: ~50-60% CPU
- 3 voices: ~75-85% CPU ⚠️
- 4 voices: ~90-100% CPU ⚠️⚠️

**Recommendations:**
- Use **2 voices max** for complex patches with long decays
- Use **3-4 voices** only for simple engines (VA VCF, Phase Distortion)
- **Arpeggiator + long decay** = many overlapping notes → reduce voice count
- CPU overload alert threshold: **80%** (to catch issues before crackling)

**Optimizations Applied:**
- Precomputed gain table (avoids sqrt() per block)
- Sparse decay detection (check 3 samples instead of all 24)
- Static accumulator buffers (no stack allocation)

## API Changes

### New Settings Parameter
```
Settings > Voices: 1/2/3/4 (default: 1)
```

### Automatic Detection
Polyphony automatically disabled for heavy engines:
```cpp
bool IsPolyphonicEngine(int engine) {
    return engine == 0 || engine == 1 || engine == 5 ||
           engine == 8 || engine == 9 || engine == 10 || engine == 13;
}
```

## Testing Checklist

- [ ] NoteOn allocates voices correctly
- [ ] NoteOff releases correct voice
- [ ] Steal-oldest works under voice pressure
- [ ] Heavy engines force monophonic
- [ ] No clicks on voice stealing
- [ ] CPU usage acceptable at 4 voices
- [ ] Memory usage within budget
- [ ] Presets save/load voice count

## Files Modified

- `plaits_port.h` - Add Voice instances and manager
- `plaits_port.cpp` - Multi-voice rendering
- `polyphonic_voice.h` - Voice manager implementation ✅
- `main.cpp` - No changes needed (manager is internal)
