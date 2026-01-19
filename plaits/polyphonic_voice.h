#pragma once

#include "daisy_patch.h"
#include "../eurorack/plaits/dsp/voice.h"
#include "../eurorack/plaits/dsp/oscillator/oscillator.h"
#include "../eurorack/plaits/dsp/oscillator/variable_saw_oscillator.h"
#include "../eurorack/plaits/dsp/oscillator/variable_shape_oscillator.h"
#include "../eurorack/plaits/dsp/oscillator/wavetable_oscillator.h"
#include "../eurorack/stmlib/dsp/filter.h"
#include "../eurorack/stmlib/utils/buffer_allocator.h"
#include <algorithm>
#include <cstring>

namespace mutables_plaits {

// Maximum number of polyphonic voices
static constexpr int kMaxPolyVoices = 4;
static constexpr size_t kPolyBlockSize = 24;

// Engines that support polyphony (lightweight engines only)
// These indices match the engine registration order in voice.cc
enum PolyphonicEngineIndex {
    POLY_ENGINE_NONE = -1,
    POLY_ENGINE_VA_VCF = 0,           // Virtual Analog with VCF (engine2)
    POLY_ENGINE_PHASE_DISTORTION = 1, // Phase Distortion (engine2)
    POLY_ENGINE_WAVE_TERRAIN = 5,     // Wave Terrain (engine2)
    POLY_ENGINE_VIRTUAL_ANALOG = 8,   // Virtual Analog
    POLY_ENGINE_WAVESHAPING = 9,      // Waveshaping
    POLY_ENGINE_FM_2OP = 10,          // 2-operator FM
    POLY_ENGINE_WAVETABLE = 13,       // Wavetable
};

// Check if an engine index supports polyphony
inline bool IsPolyphonicEngine(int engine_index) {
    switch (engine_index) {
        case POLY_ENGINE_VA_VCF:
        case POLY_ENGINE_PHASE_DISTORTION:
        case POLY_ENGINE_WAVE_TERRAIN:
        case POLY_ENGINE_VIRTUAL_ANALOG:
        case POLY_ENGINE_WAVESHAPING:
        case POLY_ENGINE_FM_2OP:
        case POLY_ENGINE_WAVETABLE:
            return true;
        default:
            return false;
    }
}

// Voice slot state for polyphony management  
struct VoiceSlot {
    int8_t midi_note;      // MIDI note number (-1 = free)
    uint32_t trigger_time; // When this voice was triggered (for steal-oldest)
    float velocity;        // Note velocity (0-1)
    bool gate;             // Gate state (true = held)
    bool just_triggered;   // True on first render after NoteOn
    
    VoiceSlot() : midi_note(-1), trigger_time(0), velocity(0.0f), gate(false), just_triggered(false) {}
    
    bool IsFree() const { return midi_note < 0; }
    bool IsActive() const { return midi_note >= 0; }  // Active = playing or releasing
    void Clear() { 
        midi_note = -1; 
        trigger_time = 0; 
        velocity = 0.0f; 
        gate = false; 
        just_triggered = false;
    }
};

//=============================================================================
// Polyphonic Voice Manager
// Manages note allocation and mixing for polyphonic engines
// Uses "steal-oldest" allocation strategy
//=============================================================================
class PolyphonicVoiceManager {
public:
    PolyphonicVoiceManager() 
        : num_voices_(kMaxPolyVoices)
        , polyphony_enabled_(false)
        , global_time_(0) {}
    
    void Init() {
        for (int i = 0; i < kMaxPolyVoices; i++) {
            slots_[i].Clear();
        }
        global_time_ = 0;
    }
    
    // Enable/disable polyphony (based on current engine)
    void SetPolyphonyEnabled(bool enabled) {
        polyphony_enabled_ = enabled;
        if (!enabled) {
            // Release all voices except 0
            for (int i = 1; i < kMaxPolyVoices; i++) {
                slots_[i].Clear();
            }
        }
    }
    
    bool IsPolyphonyEnabled() const { return polyphony_enabled_; }
    
    // Set number of voices (1-4)
    void SetVoiceCount(int count) {
        num_voices_ = std::clamp(count, 1, kMaxPolyVoices);
        // Clear excess voices
        for (int i = num_voices_; i < kMaxPolyVoices; i++) {
            slots_[i].Clear();
        }
    }
    
    int GetVoiceCount() const { return num_voices_; }
    
    // Get number of currently active voices
    int GetActiveVoiceCount() const {
        int count = 0;
        for (int i = 0; i < num_voices_; i++) {
            if (slots_[i].IsActive()) count++;
        }
        return count;
    }
    
    // Note On - allocate a voice using steal-oldest strategy
    // Returns the voice index allocated
    int NoteOn(int midi_note, float velocity) {
        global_time_++;
        
        if (!polyphony_enabled_ || num_voices_ == 1) {
            // Monophonic mode - use voice 0
            slots_[0].midi_note = midi_note;
            slots_[0].velocity = velocity;
            slots_[0].trigger_time = global_time_;
            slots_[0].gate = true;
            slots_[0].just_triggered = true;
            return 0;
        }
        
        // Check if this note is already playing (retrigger)
        for (int i = 0; i < num_voices_; i++) {
            if (slots_[i].midi_note == midi_note) {
                slots_[i].velocity = velocity;
                slots_[i].trigger_time = global_time_;
                slots_[i].gate = true;
                slots_[i].just_triggered = true;
                return i;
            }
        }
        
        // Find a free voice first
        int voice_idx = -1;
        for (int i = 0; i < num_voices_; i++) {
            if (slots_[i].IsFree()) {
                voice_idx = i;
                break;
            }
        }
        
        // If no free voice, steal the oldest released note
        if (voice_idx < 0) {
            uint32_t oldest_time = UINT32_MAX;
            for (int i = 0; i < num_voices_; i++) {
                // Prefer stealing released notes (gate=false)
                if (!slots_[i].gate && slots_[i].trigger_time < oldest_time) {
                    oldest_time = slots_[i].trigger_time;
                    voice_idx = i;
                }
            }
        }
        
        // If all notes are held, steal the oldest held note
        if (voice_idx < 0) {
            uint32_t oldest_time = UINT32_MAX;
            for (int i = 0; i < num_voices_; i++) {
                if (slots_[i].trigger_time < oldest_time) {
                    oldest_time = slots_[i].trigger_time;
                    voice_idx = i;
                }
            }
        }
        
        // Assign the note to the voice
        if (voice_idx >= 0) {
            slots_[voice_idx].midi_note = midi_note;
            slots_[voice_idx].velocity = velocity;
            slots_[voice_idx].trigger_time = global_time_;
            slots_[voice_idx].gate = true;
            slots_[voice_idx].just_triggered = true;
        }
        
        return voice_idx;
    }
    
    // Note Off - release the voice playing this note
    // Returns the voice index released, or -1 if not found
    int NoteOff(int midi_note) {
        for (int i = 0; i < num_voices_; i++) {
            if (slots_[i].midi_note == midi_note && slots_[i].gate) {
                slots_[i].gate = false;
                // Note: Don't clear midi_note yet - voice continues in release phase
                return i;
            }
        }
        return -1;
    }
    
    // All Notes Off - release all voices (let them decay naturally)
    void AllNotesOff() {
        for (int i = 0; i < num_voices_; i++) {
            slots_[i].gate = false;
        }
    }
    
    // Panic - immediately stop all voices
    void Panic() {
        for (int i = 0; i < num_voices_; i++) {
            slots_[i].Clear();
        }
    }
    
    // Get slot for a voice
    VoiceSlot& GetSlot(int index) { 
        return slots_[std::clamp(index, 0, kMaxPolyVoices - 1)]; 
    }
    const VoiceSlot& GetSlot(int index) const { 
        return slots_[std::clamp(index, 0, kMaxPolyVoices - 1)]; 
    }
    
    // Clear the just_triggered flag after rendering
    void ClearTriggerFlags() {
        for (int i = 0; i < num_voices_; i++) {
            slots_[i].just_triggered = false;
        }
    }
    
    // Mark a voice as fully decayed (to be freed)
    void MarkDecayed(int index) {
        if (index >= 0 && index < num_voices_ && !slots_[index].gate) {
            slots_[index].Clear();
        }
    }

private:
    VoiceSlot slots_[kMaxPolyVoices];
    int num_voices_;
    bool polyphony_enabled_;
    uint32_t global_time_;
};

//=============================================================================
// Polyphonic output mixer
// Mixes multiple voice outputs with proper gain staging
//=============================================================================
class PolyphonicMixer {
public:
    void Init() {
        std::memset(out_accumulator_, 0, sizeof(out_accumulator_));
        std::memset(aux_accumulator_, 0, sizeof(aux_accumulator_));
    }
    
    void Clear() {
        std::memset(out_accumulator_, 0, sizeof(out_accumulator_));
        std::memset(aux_accumulator_, 0, sizeof(aux_accumulator_));
    }
    
    // Accumulate a voice's output
    void Accumulate(const float* out, const float* aux, size_t size) {
        for (size_t i = 0; i < size; i++) {
            out_accumulator_[i] += out[i];
            aux_accumulator_[i] += aux[i];
        }
    }
    
    // Accumulate from short* buffer (Plaits Voice::Frame format)
    void AccumulateShort(const short* out, const short* aux, size_t size, size_t stride = 1) {
        const float scale = 1.0f / 32768.0f;
        for (size_t i = 0; i < size; i++) {
            out_accumulator_[i] += out[i * stride] * scale;
            aux_accumulator_[i] += aux[i * stride] * scale;
        }
    }
    
    // Write mixed output with gain compensation
    // Uses sqrt(N) scaling to preserve perceived loudness
    void WriteOutput(float* out, float* aux, size_t size, int active_voices) {
        float gain = (active_voices > 1) 
            ? (1.0f / std::sqrt(static_cast<float>(active_voices))) 
            : 1.0f;
        
        for (size_t i = 0; i < size; i++) {
            out[i] = out_accumulator_[i] * gain;
            aux[i] = aux_accumulator_[i] * gain;
        }
    }
    
    // Write to short* buffer (for compatibility with existing code)
    void WriteOutputShort(short* out, short* aux, size_t size, int active_voices, size_t stride = 1) {
        float gain = (active_voices > 1) 
            ? (32767.0f / std::sqrt(static_cast<float>(active_voices))) 
            : 32767.0f;
        
        for (size_t i = 0; i < size; i++) {
            out[i * stride] = static_cast<short>(std::clamp(out_accumulator_[i] * gain, -32767.0f, 32767.0f));
            aux[i * stride] = static_cast<short>(std::clamp(aux_accumulator_[i] * gain, -32767.0f, 32767.0f));
        }
    }

private:
    float out_accumulator_[kPolyBlockSize];
    float aux_accumulator_[kPolyBlockSize];
};

} // namespace mutables_plaits

