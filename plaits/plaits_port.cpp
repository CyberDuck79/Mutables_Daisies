#include "plaits_port.h"
#include "../eurorack/plaits/dsp/voice.h"
#include "../eurorack/stmlib/utils/buffer_allocator.h"

namespace mutables_plaits {

// Bank names
const char* PlaitsPort::bank_names_[] = {
    "Synth",
    "Drum",
    "New"
};

// Frequency range names (C0-C8 individual octaves, plus full range)
const char* PlaitsPort::freq_range_names_[] = {
    "C0",      // 0: Fixed to C0 (note 12) ±7 semitones
    "C1",      // 1: Fixed to C1 (note 24) ±7 semitones
    "C2",      // 2: Fixed to C2 (note 36) ±7 semitones
    "C3",      // 3: Fixed to C3 (note 48) ±7 semitones
    "C4",      // 4: Fixed to C4 (note 60) ±7 semitones
    "C5",      // 5: Fixed to C5 (note 72) ±7 semitones
    "C6",      // 6: Fixed to C6 (note 84) ±7 semitones
    "C7",      // 7: Fixed to C7 (note 96) ±7 semitones
    "C8",      // 8: Fixed to C8 (note 108) ±7 semitones
    "C0-C8"    // 9: Full 8-octave range
};

// MIDI channel names (Omni = all channels, then 1-16)
const char* PlaitsPort::midi_channel_names_[] = {
    "Omni",    // 0: Listen to all channels
    "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};

// Synth engines (indices 8-15 in Plaits)
const char* PlaitsPort::synth_engine_names_[] = {
    "VA",        // 8: Virtual analog
    "WavShp",    // 9: Waveshaping oscillator
    "FM",        // 10: Two operator FM
    "Grain",     // 11: Granular formant oscillator
    "Addtv",     // 12: Harmonic oscillator
    "WavTbl",    // 13: Wavetable oscillator
    "Chord",     // 14: Chords
    "Speech"     // 15: Speech synthesis
};

// Drum/noise engines (indices 16-23 in Plaits)
const char* PlaitsPort::drum_engine_names_[] = {
    "Swarm",     // 16: Swarm of sawtooths
    "Noise",     // 17: Filtered noise
    "Partcl",    // 18: Particle noise
    "String",    // 19: Inharmonic string modeling
    "Modal",     // 20: Modal resonator
    "Kick",      // 21: Analog kick drum
    "Snare",     // 22: Analog snare drum
    "HiHat"      // 23: Analog hi-hat
};

// New engines (indices 0-7 in Plaits - engine2)
const char* PlaitsPort::new_engine_names_[] = {
    "VA VCF",    // 0: Virtual analog with VCF
    "PhasDs",    // 1: Phase distortion
    "6-Op 1",    // 2: Six operator FM (patch 1)
    "6-Op 2",    // 3: Six operator FM (patch 2)
    "6-Op 3",    // 4: Six operator FM (patch 3)
    "WavTrn",    // 5: Wave terrain
    "StrMch",    // 6: String machine
    "Chip"       // 7: Chiptune
};

PlaitsPort::PlaitsPort() 
    : voice_(nullptr)
    , patch_(nullptr)
    , modulations_(nullptr)
    , allocator_(nullptr)
    , current_bank_(0)
    , midi_note_(60.0f)
    , midi_velocity_(0.8f)
    , midi_gate_(false)
    , gate_state_(false)
    , previous_gate_(false)
    , sample_rate_(48000.0f) {
}

PlaitsPort::~PlaitsPort() {
    if (voice_) delete voice_;
    if (patch_) delete patch_;
    if (modulations_) delete modulations_;
    if (allocator_) delete allocator_;
}

void PlaitsPort::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    
    // Allocate Plaits objects
    voice_ = new plaits::Voice;
    patch_ = new plaits::Patch;
    modulations_ = new plaits::Modulations;
    allocator_ = new stmlib::BufferAllocator(buffer_, kBufferSize);
    
    // Initialize CV modulation values
    frequency_cv_ = 0.0f;
    timbre_cv_ = 0.0f;
    morph_cv_ = 0.0f;
    
    // Initialize voice with buffer allocator
    voice_->Init(allocator_);
    
    // Initialize modulations to zero
    modulations_->engine = 0.0f;
    modulations_->note = 0.0f;
    modulations_->frequency = 0.0f;
    modulations_->harmonics = 0.0f;
    modulations_->timbre = 0.0f;
    modulations_->morph = 0.0f;
    modulations_->trigger = 0.0f;
    modulations_->level = 0.8f;
    modulations_->frequency_patched = false;
    modulations_->timbre_patched = false;
    modulations_->morph_patched = false;
    modulations_->trigger_patched = false;
    modulations_->level_patched = false;
    
    // Setup parameters
    SetupParameters();
    
    // Initialize patch with default values
    UpdatePatchFromParams();
}

void PlaitsPort::SetupParameters() {
    // Bank and Engine selection (ENUM type)
    params_[0] = mutables_ui::Parameter::Enum("Bank", bank_names_, kNumBanks);
    params_[1] = mutables_ui::Parameter::Enum("Engine", synth_engine_names_, kNumSynthEngines);
    current_bank_ = 0;
    
    // ==========================================================================
    // PARAMETER ORDER: Matches original Plaits knob layout
    // MODEL (Bank/Engine) -> FREQUENCY -> HARMONICS -> TIMBRE -> MORPH
    // ==========================================================================
    
    // Frequency Range - selects octave range for the Frequency knob
    // C0-C8: individual octaves with ±7 semitone fine tuning
    // C0-C8 (full): full 8-octave range
    params_[2] = mutables_ui::Parameter::Enum("Freq. Rng", freq_range_names_, kNumFreqRanges);
    params_[2].SetIndex(4);  // Default to C4 (middle C)
    
    // Frequency - HAS native attenuverter (patch->frequency_modulation_amount)
    params_[3] = mutables_ui::Parameter::Knob("Frequency", 0.0f, 1.0f, 0.5f);
    
    // Harmonics - NO native attenuverter in Plaits, we handle it
    params_[4] = mutables_ui::Parameter::Knob("Harmonics", 0.0f, 1.0f, 0.5f);
    
    // Timbre - HAS native attenuverter (patch->timbre_modulation_amount)
    params_[5] = mutables_ui::Parameter::Knob("Timbre", 0.0f, 1.0f, 0.5f);
    
    // Morph - HAS native attenuverter (patch->morph_modulation_amount)
    params_[6] = mutables_ui::Parameter::Knob("Morph", 0.0f, 1.0f, 0.5f);
    
    // Output level - CV input type (direct input, no attenuverter emulation)
    params_[7] = mutables_ui::Parameter::CV("Level");
    params_[7].value = 0.8f;  // Default level
    
    // LPG Color - NO native attenuverter, we handle it
    params_[8] = mutables_ui::Parameter::Knob("LPG Color", 0.0f, 1.0f, 0.5f);
    
    // LPG Decay - NO native attenuverter, we handle it
    params_[9] = mutables_ui::Parameter::Knob("LPG Decay", 0.0f, 1.0f, 0.5f);
    
    // Volume - scales output level (1.0 = full, useful for eurorack compatibility)
    // Can add velocity mod for standard velocity->volume behavior
    params_[10] = mutables_ui::Parameter::Knob("Volume", 0.0f, 1.0f, 1.0f);  // Default to full
    
    // MIDI Channel - Omni (all) or specific channel 1-16
    params_[11] = mutables_ui::Parameter::Enum("MIDI Ch", midi_channel_names_, kNumMidiChannels);
    params_[11].SetIndex(0);  // Default to Omni
    
    // Save/Load presets
    params_[12] = mutables_ui::Parameter::Save();
    params_[13] = mutables_ui::Parameter::Load();
}

void PlaitsPort::UpdateEngineListForBank(int bank) {
    if (bank == current_bank_) return;
    
    current_bank_ = bank;
    
    // Reset engine selection to 0 when changing banks
    switch (bank) {
        case 0:  // Synth
            params_[1] = mutables_ui::Parameter::Enum("Engine", synth_engine_names_, kNumSynthEngines);
            break;
        case 1:  // Drum
            params_[1] = mutables_ui::Parameter::Enum("Engine", drum_engine_names_, kNumDrumEngines);
            break;
        case 2:  // New
            params_[1] = mutables_ui::Parameter::Enum("Engine", new_engine_names_, kNumNewEngines);
            break;
    }
}

int PlaitsPort::GetActualEngineIndex(int bank, int engine_in_bank) {
    switch (bank) {
        case 0:  // Synth bank -> engines 8-15
            return 8 + engine_in_bank;
        case 1:  // Drum bank -> engines 16-21
            return 16 + engine_in_bank;
        case 2:  // New bank -> engines 0-7
            return engine_in_bank;
        default:
            return 8;  // Default to first synth engine
    }
}

void PlaitsPort::UpdatePatchFromParams() {
    if (!patch_) return;
    
    // Check if bank changed
    int bank = params_[0].GetIndex();
    UpdateEngineListForBank(bank);
    
    // Engine selection based on bank + engine
    int engine_in_bank = params_[1].GetIndex();
    patch_->engine = GetActualEngineIndex(bank, engine_in_bank);
    
    // ==========================================================================
    // FREQUENCY CALCULATION (matching original Plaits behavior)
    // ==========================================================================
    // Original Plaits:
    // - FREQUENCY knob sets base note within the selected octave range
    // - V/Oct CV is ADDED to this base note by the voice engine
    // 
    // Our implementation:
    // - Freq. Rng selects the octave range (C0-C8 or full)
    // - Frequency knob sets fine tuning (±7 semitones in Cn mode)
    // - MIDI note acts as V/Oct: added to base note as offset from C4 (note 60)
    // ==========================================================================
    
    int freq_range = params_[2].GetIndex();  // 0-8 = C0-C8, 9 = full range
    float frequency_knob = params_[3].value;
    
    float base_note;
    if (freq_range == 9) {
        // Full C0-C8 range: knob sweeps entire range (notes 12-108)
        base_note = 60.0f + (frequency_knob - 0.5f) * 96.0f;  // C0=12 to C8=108, centered on C4
    } else {
        // Fixed octave with ±7 semitone fine tuning
        // C0=note 12, C1=24, ..., C8=108
        float octave_note = static_cast<float>((freq_range + 1) * 12);  // +1 because C0=12
        float fine_tune = (frequency_knob - 0.5f) * 14.0f;  // ±7 semitones
        base_note = octave_note + fine_tune;
    }
    
    // MIDI note acts as V/Oct offset from C4 (middle C = note 60)
    // When MIDI note 60 is received, it adds 0 to base_note
    // MIDI note 72 adds +12 (one octave up), MIDI note 48 adds -12 (one octave down)
    float voct_offset = midi_note_ - 60.0f;
    
    patch_->note = base_note + voct_offset;
    
    // For parameters with CV mapping: use offset as base value when plugged
    // This lets Plaits add the CV modulation on top
    auto& harmonics = params_[4];
    auto& timbre = params_[5];
    auto& morph = params_[6];
    auto& frequency = params_[3];
    
    // Apply velocity modulation: vel_mod = velocity * velocity_amount
    // This is applied when USING values, not when storing them (to avoid feedback)
    float harm_vel = midi_velocity_ * harmonics.mapping.velocity_amount;
    float timb_vel = midi_velocity_ * timbre.mapping.velocity_amount;
    float morph_vel = midi_velocity_ * morph.mapping.velocity_amount;
    float lpg_col_vel = midi_velocity_ * params_[8].mapping.velocity_amount;
    float lpg_dec_vel = midi_velocity_ * params_[9].mapping.velocity_amount;
    
    float harm_base = harmonics.mapping.plugged ? harmonics.mapping.offset : harmonics.value;
    float timb_base = timbre.mapping.plugged ? timbre.mapping.offset : timbre.value;
    float morph_base = morph.mapping.plugged ? morph.mapping.offset : morph.value;
    
    patch_->harmonics = std::clamp(harm_base + harm_vel, 0.0f, 1.0f);
    patch_->timbre = std::clamp(timb_base + timb_vel, 0.0f, 1.0f);
    patch_->morph = std::clamp(morph_base + morph_vel, 0.0f, 1.0f);
    
    // Modulation amounts - use the attenuverter from each parameter's mapping
    // These control internal envelope routing (when unplugged) or external CV scaling (when plugged)
    // CC mapping just replaces the base value, doesn't affect modulation
    patch_->frequency_modulation_amount = frequency.mapping.attenuverter;
    patch_->timbre_modulation_amount = timbre.mapping.attenuverter;
    patch_->morph_modulation_amount = morph.mapping.attenuverter;
    
    // LPG parameters with velocity modulation
    patch_->lpg_colour = std::clamp(params_[8].value + lpg_col_vel, 0.0f, 1.0f);
    patch_->decay = std::clamp(params_[9].value + lpg_dec_vel, 0.0f, 1.0f);
}

void PlaitsPort::SetCVModulations(float frequency_cv, float timbre_cv, float morph_cv) {
    frequency_cv_ = frequency_cv;
    timbre_cv_ = timbre_cv;
    morph_cv_ = morph_cv;
}

void PlaitsPort::Process(float** in, float** out, size_t size) {
    if (!voice_ || !patch_ || !modulations_) return;
    
    UpdatePatchFromParams();
    
    // Plaits processes in blocks
    plaits::Voice::Frame frames[kBlockSize];
    
    for (size_t i = 0; i < size; i += kBlockSize) {
        size_t block_size = (i + kBlockSize <= size) ? kBlockSize : (size - i);
        
        // Use MIDI gate OR hardware gate
        bool active_gate = midi_gate_ || gate_state_;
        
        // Set modulations - keep trigger high while gate is active
        // Plaits does its own edge detection internally
        modulations_->trigger = active_gate ? 1.0f : 0.0f;
        modulations_->level = params_[7].value;
        
        // CV modulation values (set by main.cpp via SetCVModulations)
        // For CC: the CC value is used directly as modulation (scaled 0-1)
        modulations_->frequency = frequency_cv_;
        modulations_->timbre = timbre_cv_;
        modulations_->morph = morph_cv_;
        
        // Patched status - true only when CV is mapped+plugged
        // This tells Plaits whether to use external modulation or internal envelope
        // CC mapping just replaces base value, doesn't count as patched
        auto& frequency = params_[3];
        auto& timbre = params_[5];
        auto& morph = params_[6];
        
        modulations_->frequency_patched = frequency.mapping.IsCVSource() && frequency.mapping.plugged;
        modulations_->timbre_patched = timbre.mapping.IsCVSource() && timbre.mapping.plugged;
        modulations_->morph_patched = morph.mapping.IsCVSource() && morph.mapping.plugged;
        modulations_->trigger_patched = true;  // Always patched via MIDI/Gate
        modulations_->level_patched = params_[7].mapping.IsCVSource();
        
        // Render audio
        voice_->Render(*patch_, *modulations_, frames, block_size);
        
        // Get volume with velocity modulation
        float vol_vel = midi_velocity_ * params_[10].mapping.velocity_amount;
        float volume = std::clamp(params_[10].value + vol_vel, 0.0f, 1.0f);
        
        // Convert from short to float, apply volume, and copy to outputs
        // OUT -> channels 1 & 3, AUX -> channels 2 & 4
        for (size_t j = 0; j < block_size && (i + j) < size; j++) {
            float out_sample = (static_cast<float>(frames[j].out) / 32768.0f) * volume;
            float aux_sample = (static_cast<float>(frames[j].aux) / 32768.0f) * volume;
            out[0][i + j] = out_sample;  // OUT -> channel 1
            out[1][i + j] = aux_sample;  // AUX -> channel 2
            out[2][i + j] = out_sample;  // OUT -> channel 3
            out[3][i + j] = aux_sample;  // AUX -> channel 4
        }
    }
}

mutables_ui::Parameter* PlaitsPort::GetParameters() {
    return params_.data();
}

size_t PlaitsPort::GetParameterCount() const {
    return params_.size();
}

void PlaitsPort::ProcessGate(int gate_index, bool state) {
    if (gate_index == 0) {
        gate_state_ = state;
    }
}

float PlaitsPort::GetCVOutput(int cv_index) {
    // Could output envelope or other modulation signals
    return 0.0f;
}

void PlaitsPort::NoteOn(uint8_t note, uint8_t velocity) {
    midi_note_ = static_cast<float>(note);
    midi_velocity_ = static_cast<float>(velocity) / 127.0f;
    midi_gate_ = true;
}

void PlaitsPort::NoteOff(uint8_t note, uint8_t velocity) {
    // Only release if it's the same note (monophonic)
    if (static_cast<uint8_t>(midi_note_) == note) {
        midi_gate_ = false;
    }
}

} // namespace mutables_plaits
