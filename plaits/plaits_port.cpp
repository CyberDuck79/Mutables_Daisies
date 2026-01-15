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

// CV Output mode names
static const char* cv_out_mode_names[] = {
    "LPG Env",   // Follows internal LPG envelope
    "AD",        // AD envelope triggered on gate
    "LFO"        // Low frequency oscillator
};
static constexpr int kNumCVOutModes = 3;

// LFO shape names
static const char* lfo_shape_names[] = {
    "Sine",
    "Tri",
    "Saw",
    "Square",
    "S&H",
    "RndSmth"
};
static constexpr int kNumLFOShapes = 6;

// Clock sync on/off
static const char* sync_names[] = {
    "Free",
    "MIDI"
};
static constexpr int kNumSyncModes = 2;

// Clock ratio names for display when MIDI sync is enabled
static const char* clock_ratio_names[] = {
    "1/16", "1/16T", "1/16D",
    "1/8", "1/8T", "1/8D",
    "1/4", "1/4T", "1/4D",
    "1/2", "1/2T", "1/2D",
    "1", "2", "4", "8"
};
static constexpr int kNumClockRatios = 16;

// CV Out parameter indices within SUB
enum CVOutParamIndex {
    CVOUT_MODE = 0,
    CVOUT_ATTACK = 1,
    CVOUT_RELEASE = 2,
    CVOUT_SHAPE = 3,
    CVOUT_SYNC = 4,
    CVOUT_RATE = 5,
    CVOUT_AMP = 6,
    CVOUT_PHASE = 7
};

// Visibility callback for CV Out params
// Mode 0 = LPG Env: only show Amp
// Mode 1 = AD: show Attack, Release, Amp
// Mode 2 = LFO: show Shape, Sync, Rate, Amp, Phase
static bool CVOutVisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[CVOUT_MODE].GetIndex();
    
    switch (param_index) {
        case CVOUT_MODE:
            return true;  // Always visible
        case CVOUT_ATTACK:
        case CVOUT_RELEASE:
            return (mode == 1);  // Only for AD mode
        case CVOUT_SHAPE:
        case CVOUT_SYNC:
        case CVOUT_PHASE:
            return (mode == 2);  // Only for LFO mode
        case CVOUT_RATE:
            return (mode == 2);  // Only for LFO mode
        case CVOUT_AMP:
            return true;  // Always visible
        default:
            return true;
    }
}

// Format callback for CV Out params
static void CVOutFormatCallback(const mutables_ui::Parameter* param, 
                                const mutables_ui::Parameter* siblings, 
                                uint8_t sibling_count, uint8_t param_index,
                                char* buffer, size_t buffer_size) {
    // Handle ENUM types
    if (param->type == mutables_ui::ParamType::ENUM) {
        snprintf(buffer, buffer_size, "%.6s", param->GetEnumLabel());
        return;
    }
    
    float value = param->value;
    
    switch (param_index) {
        case CVOUT_ATTACK:
            // 0.5ms to 200ms log scaled
            {
                float ms = 0.5f * powf(400.0f, value);
                int ms_int = static_cast<int>(ms * 10.0f + 0.5f);  // tenths of ms
                if (ms < 10.0f) {
                    // Show one decimal place: X.Xms
                    snprintf(buffer, buffer_size, "%d.%dms", ms_int / 10, ms_int % 10);
                } else {
                    // Show integer ms
                    snprintf(buffer, buffer_size, "%dms", static_cast<int>(ms + 0.5f));
                }
            }
            break;
            
        case CVOUT_RELEASE:
            // 5ms to 2000ms log scaled
            {
                float ms = 5.0f * powf(400.0f, value);
                if (ms < 1000.0f) {
                    snprintf(buffer, buffer_size, "%dms", static_cast<int>(ms + 0.5f));
                } else {
                    // Show as seconds with one decimal
                    int s_int = static_cast<int>(ms / 100.0f + 0.5f);  // tenths of seconds
                    snprintf(buffer, buffer_size, "%d.%ds", s_int / 10, s_int % 10);
                }
            }
            break;
            
        case CVOUT_RATE:
            // Check if MIDI sync is enabled
            if (sibling_count > CVOUT_SYNC && siblings[CVOUT_SYNC].GetIndex() == 1) {
                // MIDI sync mode: show ratio name
                int ratio_idx = static_cast<int>(value * (kNumClockRatios - 1) + 0.5f);
                if (ratio_idx < 0) ratio_idx = 0;
                if (ratio_idx >= kNumClockRatios) ratio_idx = kNumClockRatios - 1;
                snprintf(buffer, buffer_size, "%s", clock_ratio_names[ratio_idx]);
            } else {
                // Free mode: show Hz (0.1 to 20 Hz log scaled)
                float hz = 0.1f * powf(200.0f, value);
                int hz_int = static_cast<int>(hz * 100.0f + 0.5f);  // hundredths of Hz
                if (hz < 1.0f) {
                    // Show two decimals: 0.XXHz
                    snprintf(buffer, buffer_size, "0.%02dHz", hz_int);
                } else if (hz < 10.0f) {
                    // Show one decimal: X.XHz
                    int hz_tenths = static_cast<int>(hz * 10.0f + 0.5f);
                    snprintf(buffer, buffer_size, "%d.%dHz", hz_tenths / 10, hz_tenths % 10);
                } else {
                    // Show integer Hz
                    snprintf(buffer, buffer_size, "%dHz", static_cast<int>(hz + 0.5f));
                }
            }
            break;
            
        case CVOUT_AMP:
            // Percentage 0-100%
            snprintf(buffer, buffer_size, "%d%%", static_cast<int>(value * 100.0f + 0.5f));
            break;
            
        case CVOUT_PHASE:
            // Degrees 0-360
            snprintf(buffer, buffer_size, "%d", static_cast<int>(value * 360.0f + 0.5f));
            break;
            
        default:
            // Default: show as 0.00
            {
                int val_int = static_cast<int>(value * 100.0f);
                int whole = val_int / 100;
                int frac = val_int % 100;
                snprintf(buffer, buffer_size, "%d.%02d", whole, frac);
            }
            break;
    }
}

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
    , sample_rate_(48000.0f)
    , midi_clock_hz_(0.0f)
    , sample_counter_(0) {
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
    
    // Initialize CV modulators
    cv_modulator_1_.Init();
    cv_modulator_1_.SetSampleRate(sample_rate, kBlockSize);
    cv_modulator_2_.Init();
    cv_modulator_2_.SetSampleRate(sample_rate, kBlockSize);
    midi_clock_tracker_.Init();
    
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
    
    // User Data submenu - allows selecting custom user data files from SD card
    // Target indices match UserDataManager::Target enum
    user_data_params_[0] = mutables_ui::Parameter::UserData("6-Op Bk 1", 0);  // TARGET_SIX_OP_1
    user_data_params_[1] = mutables_ui::Parameter::UserData("6-Op Bk 2", 1);  // TARGET_SIX_OP_2
    user_data_params_[2] = mutables_ui::Parameter::UserData("6-Op Bk 3", 2);  // TARGET_SIX_OP_3
    user_data_params_[3] = mutables_ui::Parameter::UserData("WavTerrain", 3); // TARGET_WAVE_TERRAIN
    user_data_params_[4] = mutables_ui::Parameter::UserData("Wavetable", 4);  // TARGET_WAVETABLE
    params_[12] = mutables_ui::Parameter::Sub("User Data", user_data_params_.data(), kNumUserDataParams);
    
    // CV Output 1 submenu
    SetupCVOutParams(cv_out1_params_, "CV1");
    params_[13] = mutables_ui::Parameter::Sub("CV Out 1", cv_out1_params_.data(), kNumCVOutParams);
    
    // CV Output 2 submenu
    SetupCVOutParams(cv_out2_params_, "CV2");
    params_[14] = mutables_ui::Parameter::Sub("CV Out 2", cv_out2_params_.data(), kNumCVOutParams);
    
    // Save/Load presets
    params_[15] = mutables_ui::Parameter::Save();
    params_[16] = mutables_ui::Parameter::Load();
}

void PlaitsPort::SetupCVOutParams(std::array<mutables_ui::Parameter, kNumCVOutParams>& params, const char* name_prefix) {
    // Mode: LPG Env, AD, LFO
    params[0] = mutables_ui::Parameter::Enum("Mode", cv_out_mode_names, kNumCVOutModes);
    
    // AD envelope params (used when mode = AD)
    params[1] = mutables_ui::Parameter::Knob("Attack", 0.0f, 1.0f, 0.01f);    // 0.5ms-200ms log scaled
    params[2] = mutables_ui::Parameter::Knob("Release", 0.0f, 1.0f, 0.3f);    // 5ms-2000ms log scaled
    
    // LFO params (used when mode = LFO)
    params[3] = mutables_ui::Parameter::Enum("Shape", lfo_shape_names, kNumLFOShapes);
    params[4] = mutables_ui::Parameter::Enum("Sync", sync_names, kNumSyncModes);
    params[5] = mutables_ui::Parameter::Knob("Rate", 0.0f, 1.0f, 0.3f);       // 0.1-20Hz free, or ratio index when synced
    
    // Common params
    params[6] = mutables_ui::Parameter::Knob("Amp", 0.0f, 1.0f, 1.0f);        // Output amplitude scaling
    params[7] = mutables_ui::Parameter::Knob("Phase", 0.0f, 1.0f, 0.0f);      // LFO initial phase offset (0-360°)
    
    // Assign visibility and format callbacks to all params
    for (int i = 0; i < kNumCVOutParams; i++) {
        params[i].visibility_callback = CVOutVisibilityCallback;
        params[i].format_callback = CVOutFormatCallback;
    }
}

void PlaitsPort::UpdateEngineListForBank(int bank) {
    if (bank == current_bank_) return;
    
    current_bank_ = bank;
    
    // Save the current mapping before recreating the parameter
    auto saved_mapping = params_[1].mapping;
    float saved_value = params_[1].value;
    
    // Update engine list based on bank
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
    
    // Restore the mapping (preserves CV assignment, etc.)
    params_[1].mapping = saved_mapping;
    // Clamp the value to the new valid range
    if (saved_value < params_[1].min) saved_value = params_[1].min;
    if (saved_value > params_[1].max) saved_value = params_[1].max;
    params_[1].value = saved_value;
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

void PlaitsPort::ReloadUserData() {
    if (voice_) {
        voice_->ReloadUserData();
    }
}

void PlaitsPort::Process(float** in, float** out, size_t size) {
    if (!voice_ || !patch_ || !modulations_) return;
    
    UpdatePatchFromParams();
    UpdateCVModulatorsFromParams();
    
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
        
        // Process CV modulators (once per audio block)
        // Get LPG envelope from voice for LPG_ENV mode
        float lpg_gain = voice_->GetLPGGain();
        cv_modulator_1_.Process(lpg_gain, midi_clock_hz_);
        cv_modulator_2_.Process(lpg_gain, midi_clock_hz_);
        
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
        // Detect rising edge for CV modulator triggers
        if (state && !gate_state_) {
            cv_modulator_1_.Trigger();
            cv_modulator_2_.Trigger();
        }
        gate_state_ = state;
    }
}

float PlaitsPort::GetCVOutput(int cv_index) {
    if (cv_index == 0) {
        return cv_modulator_1_.GetOutput();
    } else if (cv_index == 1) {
        return cv_modulator_2_.GetOutput();
    }
    return 0.0f;
}

void PlaitsPort::UpdateCVModulatorsFromParams() {
    // Update CV modulator 1 from its params
    cv_modulator_1_.SetMode(static_cast<CVOutMode>(cv_out1_params_[0].GetIndex()));
    cv_modulator_1_.SetAttack(cv_out1_params_[1].value);
    cv_modulator_1_.SetRelease(cv_out1_params_[2].value);
    cv_modulator_1_.SetLFOShape(static_cast<LFOShape>(cv_out1_params_[3].GetIndex()));
    cv_modulator_1_.SetClockSync(cv_out1_params_[4].GetIndex() == 1);
    cv_modulator_1_.SetRate(cv_out1_params_[5].value);
    cv_modulator_1_.SetAmp(cv_out1_params_[6].value);
    cv_modulator_1_.SetPhaseOffset(cv_out1_params_[7].value);
    
    // Update CV modulator 2 from its params
    cv_modulator_2_.SetMode(static_cast<CVOutMode>(cv_out2_params_[0].GetIndex()));
    cv_modulator_2_.SetAttack(cv_out2_params_[1].value);
    cv_modulator_2_.SetRelease(cv_out2_params_[2].value);
    cv_modulator_2_.SetLFOShape(static_cast<LFOShape>(cv_out2_params_[3].GetIndex()));
    cv_modulator_2_.SetClockSync(cv_out2_params_[4].GetIndex() == 1);
    cv_modulator_2_.SetRate(cv_out2_params_[5].value);
    cv_modulator_2_.SetAmp(cv_out2_params_[6].value);
    cv_modulator_2_.SetPhaseOffset(cv_out2_params_[7].value);
}

void PlaitsPort::OnMIDIClock() {
    midi_clock_tracker_.OnClock(sample_counter_);
    // Update clock Hz for modulators
    if (midi_clock_tracker_.IsActive(sample_counter_, sample_rate_)) {
        midi_clock_hz_ = midi_clock_tracker_.GetClockHz(sample_rate_);
    } else {
        midi_clock_hz_ = 0.0f;
    }
}

void PlaitsPort::UpdateSampleCounter(size_t samples) {
    sample_counter_ += samples;
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
