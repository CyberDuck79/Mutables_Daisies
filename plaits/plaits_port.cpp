#include "plaits_port.h"
#include "../eurorack/plaits/dsp/voice.h"
#include "../eurorack/stmlib/utils/buffer_allocator.h"
#include "../common/constants.h"
#include "../common/utils/format_utils.h"
#include <cstring>
#include <cmath>

namespace mutables_plaits {

using namespace mutables;

// Bank names
const char* PlaitsPort::bank_names_[] = {
    "Synth",
    "Drum",
    "New"
};

// Octave names (C0-C8 base octave, Freq knob adds ±7 semitones fine tuning)
const char* PlaitsPort::octave_names_[] = {
    "C0",      // 0: Base C0 (note 12)
    "C1",      // 1: Base C1 (note 24)
    "C2",      // 2: Base C2 (note 36)
    "C3",      // 3: Base C3 (note 48)
    "C4",      // 4: Base C4 (note 60)
    "C5",      // 5: Base C5 (note 72)
    "C6",      // 6: Base C6 (note 84)
    "C7",      // 7: Base C7 (note 96)
    "C8"       // 8: Base C8 (note 108)
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

// Voice count names (for polyphony setting)
const char* PlaitsPort::voice_count_names_[] = {
    "1", "2", "3", "4"
};

// CV Output mode names
static const char* cv_out_mode_names[] = {
    "LPG",       // Follows internal LPG envelope
    "AD",        // AD envelope triggered on gate
    "LFO",       // Low frequency oscillator
    "Foll.3",    // Follow audio input 3 envelope
    "Foll.4"     // Follow audio input 4 envelope
};
static constexpr int kNumCVOutModes = 5;

// Gate output mode names (for physical Gate Out jack)
static const char* gate_out_mode_names[] = {
    "Trig",   // Trigger on Gate 1 rise OR MIDI note on
    "EndEnv",    // Trigger on envelope end
    "TrigPrb",  // Trigger with probability
    "ClkDiv",    // Clock divider (MIDI clock or Gate 2)
    "ClkPrb"    // Clock tick with probability
};
static constexpr int kNumGateOutModes = 5;

// Clock divider ratio names (for Gate Out ClkDiv mode)
static const char* clk_div_names[] = {
    "/1", "/2", "/3", "/4", "/6", "/8", "/12", "/16", "/24", "/32"
};
static constexpr int kNumClkDivs = 10;
// Actual divider values (MIDI clock = 24 ppq)
static const int clk_div_values[] = {
    24, 48, 72, 96, 144, 192, 288, 384, 576, 768  // 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 quarter notes
};

// Gate Out parameter indices
enum GateOutParamIndex {
    GATEOUT_MODE = 0,
    GATEOUT_CLK_DIV = 1,
    GATEOUT_PROB = 2
};

// Visibility callback for Gate Out params
static bool GateOutVisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[GATEOUT_MODE].GetIndex();
    
    switch (param_index) {
        case GATEOUT_MODE:
            return true;  // Always visible
        case GATEOUT_CLK_DIV:
            return (mode == 3);  // Only for ClkDiv mode
        case GATEOUT_PROB:
            return (mode == 2 || mode == 4);  // Only for TrigProb and ClkProb modes
        default:
            return true;
    }
}

// Format callback for Gate Out probability parameter
static void GateOutFormatCallback(const mutables_ui::Parameter* param, const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index, char* buffer, size_t buffer_size) {
    (void)siblings;
    (void)sibling_count;
    (void)param_index;
    snprintf(buffer, buffer_size, "%d%%", static_cast<int>(param->value * 100.0f));
}

// Audio Input (envelope/trigger) mode names
static const char* audio_in_mode_names[] = {
    "OFF",
    "ENV",    // Envelope follower
    "TRIG"    // Transient detector
};
static constexpr int kNumAudioInModes = 3;

// Audio In parameter indices
enum AudioInParamIndex {
    AUDIOIN_MODE = 0,
    AUDIOIN_GAIN = 1,         // Input gain (1x-10x for line level signals)
    AUDIOIN_TIMBRE_AMT = 2,
    AUDIOIN_MORPH_AMT = 3,
    AUDIOIN_ATTACK = 4,
    AUDIOIN_RELEASE = 5,
    AUDIOIN_THRESHOLD = 6,
    AUDIOIN_HOLDOFF = 7
};

// Visibility callback for Audio In params
static bool AudioInVisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[AUDIOIN_MODE].GetIndex();
    
    switch (param_index) {
        case AUDIOIN_MODE:
            return true;  // Always visible
        case AUDIOIN_GAIN:
            return (mode != 0);  // Visible in ENV and TRIG modes
        case AUDIOIN_TIMBRE_AMT:
        case AUDIOIN_MORPH_AMT:
        case AUDIOIN_ATTACK:
        case AUDIOIN_RELEASE:
            return (mode == 1);  // Only for ENV mode
        case AUDIOIN_THRESHOLD:
        case AUDIOIN_HOLDOFF:
            return (mode == 2);  // Only for TRIG mode
        default:
            return true;
    }
}

// Format callback for bipolar percentage display (e.g. ±100%)
static void BipolarPercentFormatCallback(const mutables_ui::Parameter* param, 
                                         const mutables_ui::Parameter* siblings, 
                                         uint8_t sibling_count, uint8_t param_index,
                                         char* buffer, size_t buffer_size) {
    mutables_ui::format::FormatBipolarPercent(buffer, buffer_size, param->value);
}

// Format callback for Audio In params (Attack, Release in ms, Holdoff in ms, Threshold in %)
static void AudioInFormatCallback(const mutables_ui::Parameter* param, 
                                  const mutables_ui::Parameter* siblings, 
                                  uint8_t sibling_count, uint8_t param_index,
                                  char* buffer, size_t buffer_size) {
    float value = param->value;
    
    switch (param_index) {
        case AUDIOIN_GAIN:
            mutables_ui::format::FormatGainDB(buffer, buffer_size, value);
            break;
            
        case AUDIOIN_ATTACK:
            mutables_ui::format::FormatAttackTime(buffer, buffer_size, value);
            break;
            
        case AUDIOIN_RELEASE:
            mutables_ui::format::FormatReleaseTime(buffer, buffer_size, value);
            break;
            
        case AUDIOIN_THRESHOLD:
            mutables_ui::format::FormatPercent(buffer, buffer_size, value);
            break;
            
        case AUDIOIN_HOLDOFF:
            // 20ms to 200ms linear
            {
                float ms = 20.0f + value * 180.0f;
                snprintf(buffer, buffer_size, "%dms", static_cast<int>(ms + 0.5f));
            }
            break;
            
        default:
            // Default: show as percentage
            snprintf(buffer, buffer_size, "%d%%", static_cast<int>(value * 100.0f + 0.5f));
            break;
    }
}

// Warps Lite Stage 1 (Audio In 1) mode names - 7 algorithms (no Vocoder)
static const char* audio_in1_mode_names[] = {
    "OFF",
    "XFADE",   // Crossfade between synth and external
    "FOLD",    // Wavefolding
    "AnaRM",   // Analog ring modulation (diode)
    "DigRM",   // Digital ring modulation
    "XOR",     // Bitwise XOR
    "COMP",    // Comparator modes
    "FM"       // Phase modulation (true FM)
};
static constexpr int kNumAudioIn1Modes = 8;

// Warps Lite Stage 2 (Audio In 2) mode names - 8 algorithms (with Vocoder)
static const char* audio_in2_mode_names[] = {
    "OFF",
    "XFADE",   // Crossfade between synth and external
    "FOLD",    // Wavefolding
    "AnaRM",   // Analog ring modulation (diode)
    "DigRM",   // Digital ring modulation
    "XOR",     // Bitwise XOR
    "COMP",    // Comparator modes
    "FM",      // Phase modulation (true FM)
    "VOCOD"    // Vocoder (Stage 2 only, CPU intensive)
};
static constexpr int kNumAudioIn2Modes = 9;

// Warps Lite parameter indices (same for both stages)
enum AudioModParamIndex {
    AUDIOMOD_MODE = 0,
    AUDIOMOD_GAIN = 1,
    AUDIOMOD_LEVEL = 2,
    AUDIOMOD_TIMBRE = 3
};

// Visibility callback for Warps Lite Stage 1 params
static bool AudioIn1VisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[AUDIOMOD_MODE].GetIndex();
    
    switch (param_index) {
        case AUDIOMOD_MODE:
            return true;  // Always visible
        case AUDIOMOD_GAIN:
        case AUDIOMOD_LEVEL:
        case AUDIOMOD_TIMBRE:
            return (mode != 0);  // Only visible when modulation is enabled
        default:
            return true;
    }
}

// Visibility callback for Warps Lite Stage 2 params
static bool AudioIn2VisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[AUDIOMOD_MODE].GetIndex();
    
    switch (param_index) {
        case AUDIOMOD_MODE:
            return true;  // Always visible
        case AUDIOMOD_GAIN:
        case AUDIOMOD_LEVEL:
        case AUDIOMOD_TIMBRE:
            return (mode != 0);  // Only visible when modulation is enabled
        default:
            return true;
    }
}

// Format callback for Warps Lite params (Gain in dB, Level and Timbre in %)
static void AudioModFormatCallback(const mutables_ui::Parameter* param, 
                                   const mutables_ui::Parameter* siblings, 
                                   uint8_t sibling_count, uint8_t param_index,
                                   char* buffer, size_t buffer_size) {
    float value = param->value;
    
    switch (param_index) {
        case AUDIOMOD_GAIN:
            mutables_ui::format::FormatGainDB(buffer, buffer_size, value);
            break;
        case AUDIOMOD_LEVEL:
        case AUDIOMOD_TIMBRE:
        default:
            mutables_ui::format::FormatPercent(buffer, buffer_size, value);
            break;
    }
}

// Moog Ladder Filter mode names
static const char* filter_mode_names[] = {
    "OFF",
    "LP12",   // 12 dB/oct lowpass (2-pole)
    "LP24",   // 24 dB/oct lowpass (4-pole, classic Moog)
    "BP12",   // 12 dB/oct bandpass
    "BP24",   // 24 dB/oct bandpass
    "HP12",   // 12 dB/oct highpass
    "HP24"    // 24 dB/oct highpass
};
static constexpr int kNumFilterModes = 7;

// Filter parameter indices
enum FilterParamIndex {
    FILTER_MODE = 0,
    FILTER_FREQ = 1,     // Cutoff frequency (20-20000 Hz, log scaled)
    FILTER_RESO = 2,     // Resonance (0-100%, self-oscillates at high values)
    FILTER_DRIVE = 3,    // Input drive (0-100%)
    FILTER_TRACK = 4     // Key tracking (0-100%)
};

// Visibility callback for Filter params
static bool FilterVisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[FILTER_MODE].GetIndex();
    
    switch (param_index) {
        case FILTER_MODE:
            return true;  // Always visible
        case FILTER_FREQ:
        case FILTER_RESO:
        case FILTER_DRIVE:
        case FILTER_TRACK:
            return (mode != 0);  // Only visible when filter is enabled
        default:
            return true;
    }
}

// Format callback for Filter params
static void FilterFormatCallback(const mutables_ui::Parameter* param, 
                                 const mutables_ui::Parameter* siblings, 
                                 uint8_t sibling_count, uint8_t param_index,
                                 char* buffer, size_t buffer_size) {
    float value = param->value;
    
    switch (param_index) {
        case FILTER_FREQ:
            // 20Hz to 20000Hz log scaled
            {
                float freq = 20.0f * powf(1000.0f, value);  // 20 * 1000^value
                if (freq < 100.0f) {
                    snprintf(buffer, buffer_size, "%dHz", static_cast<int>(freq + 0.5f));
                } else if (freq < 1000.0f) {
                    snprintf(buffer, buffer_size, "%dHz", static_cast<int>(freq + 0.5f));
                } else {
                    int khz_int = static_cast<int>(freq / 100.0f + 0.5f);
                    snprintf(buffer, buffer_size, "%d.%dkHz", khz_int / 10, khz_int % 10);
                }
            }
            break;
        case FILTER_RESO:
        case FILTER_DRIVE:
        case FILTER_TRACK:
            // Show as percentage
            snprintf(buffer, buffer_size, "%d%%", static_cast<int>(value * 100.0f + 0.5f));
            break;
        default:
            snprintf(buffer, buffer_size, "%d%%", static_cast<int>(value * 100.0f + 0.5f));
            break;
    }
}

// LFO shape names
static const char* lfo_shape_names[] = {
    "Sine",
    "Tri",
    "Saw",
    "Square",
    "S&H",
    "Smooth"
};
static constexpr int kNumLFOShapes = 6;

// Clock sync modes
static const char* sync_names[] = {
    "Free",
    "MIDI",
    "Gate2"
};
static constexpr int kNumSyncModes = 3;

// Retrig (on/off toggle)
static const char* retrig_names[] = {
    "OFF",
    "ON"
};
static constexpr int kNumRetrigOptions = 2;

// S&H source names
static const char* sh_source_names[] = {
    "Random",
    "CV1",
    "CV2",
    "CV3",
    "CV4"
};
static constexpr int kNumSHSources = 5;

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
    CVOUT_SLEW = 4,      // Slew amount for RndSmth mode (right after Shape)
    CVOUT_SH_SRC = 5,    // S&H source (right after Shape)
    CVOUT_SYNC = 6,
    CVOUT_RATE = 7,
    CVOUT_RETRIG = 8,    // Reset LFO phase on note trigger
    CVOUT_AMP = 9,
    CVOUT_PHASE = 10,
    CVOUT_SCALE3 = 11,   // Scale for Foll.3 mode (0-2x)
    CVOUT_SCALE4 = 12    // Scale for Foll.4 mode (0-2x)
};

// Visibility callback for CV Out params
// Mode 0 = LPG Env: only show Amp
// Mode 1 = AD: show Attack, Release, Amp
// Mode 2 = LFO: show Shape, Slew/SH_Src (based on shape), Sync, Rate, Amp, Phase
// Mode 3 = Foll.3: show Scale3, Amp
// Mode 4 = Foll.4: show Scale4, Amp
static bool CVOutVisibilityCallback(const mutables_ui::Parameter* siblings, uint8_t sibling_count, uint8_t param_index) {
    if (sibling_count < 1) return true;
    
    int mode = siblings[CVOUT_MODE].GetIndex();
    int shape = (sibling_count > CVOUT_SHAPE) ? siblings[CVOUT_SHAPE].GetIndex() : 0;
    switch (param_index) {
        case CVOUT_MODE:
            return true;  // Always visible
        case CVOUT_ATTACK:
        case CVOUT_RELEASE:
            return (mode == 1);  // Only for AD mode
        case CVOUT_SHAPE:
            return (mode == 2);  // Only for LFO mode
        case CVOUT_SLEW:
            // Only visible for LFO mode AND RndSmth shape (index 5)
            return (mode == 2) && (shape == 5);
        case CVOUT_SH_SRC:
            // Only visible for LFO mode AND S&H shape (index 4)
            return (mode == 2) && (shape == 4);
        case CVOUT_SYNC:
        case CVOUT_RATE:
        case CVOUT_RETRIG:
        case CVOUT_PHASE:
            return (mode == 2);  // Only for LFO mode
        case CVOUT_AMP:
            return true;  // Always visible (all modes use it)
        case CVOUT_SCALE3:
            return (mode == 3);  // Only for Foll.3 mode
        case CVOUT_SCALE4:
            return (mode == 4);  // Only for Foll.4 mode
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
            mutables_ui::format::FormatAttackTime(buffer, buffer_size, value);
            break;
            
        case CVOUT_RELEASE:
            mutables_ui::format::FormatReleaseTime(buffer, buffer_size, value);
            break;
            
        case CVOUT_RATE:
            // Check if MIDI or Gate2 sync is enabled (index 1 or 2)
            if (sibling_count > CVOUT_SYNC && siblings[CVOUT_SYNC].GetIndex() >= 1) {
                // Clock sync mode: show ratio name
                int ratio_idx = static_cast<int>(value * (kNumClockRatios - 1) + 0.5f);
                if (ratio_idx < 0) ratio_idx = 0;
                if (ratio_idx >= kNumClockRatios) ratio_idx = kNumClockRatios - 1;
                snprintf(buffer, buffer_size, "%s", clock_ratio_names[ratio_idx]);
            } else {
                // Free mode: show Hz (0.1 to 20 Hz log scaled)
                mutables_ui::format::FormatLFORate(buffer, buffer_size, value);
            }
            break;
            
        case CVOUT_AMP:
        case CVOUT_SLEW:
            mutables_ui::format::FormatPercent(buffer, buffer_size, value);
            break;
            
        case CVOUT_PHASE:
            mutables_ui::format::FormatDegrees(buffer, buffer_size, value);
            break;
            
        case CVOUT_SCALE3:
        case CVOUT_SCALE4:
            mutables_ui::format::FormatMultiplier(buffer, buffer_size, value);
            break;
            
        default:
            mutables_ui::format::FormatDecimal(buffer, buffer_size, value);
            break;
    }
}

PlaitsPort::PlaitsPort() 
    : voice_(nullptr)
    , patch_(nullptr)
    , modulations_(nullptr)
    , allocator_(nullptr)
    , voice_count_(1)
    , current_bank_(0)
    , midi_note_(kMidiNoteC4)
    , midi_velocity_(0.8f)
    , midi_gate_(false)
    , gate_state_(false)
    , previous_gate_(false)
    , sample_rate_(kDefaultSampleRate)
    , midi_clock_hz_(0.0f)
    , gate2_clock_hz_(0.0f)
    , sample_counter_(0) {
    // Initialize polyphonic voice pointers to null
    for (int i = 0; i < kMaxPolyVoices - 1; i++) {
        poly_voices_[i] = nullptr;
        poly_allocators_[i] = nullptr;
    }
}

PlaitsPort::~PlaitsPort() {
    if (voice_) delete voice_;
    if (patch_) delete patch_;
    if (modulations_) delete modulations_;
    if (allocator_) delete allocator_;
    
    // Clean up polyphonic voices
    for (int i = 0; i < kMaxPolyVoices - 1; i++) {
        if (poly_voices_[i]) delete poly_voices_[i];
        if (poly_allocators_[i]) delete poly_allocators_[i];
    }
}

void PlaitsPort::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    
    // Allocate Plaits objects - primary voice (voice 0)
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
    
    // Initialize polyphonic voices (voices 1-3)
    for (int i = 0; i < kMaxPolyVoices - 1; i++) {
        poly_voices_[i] = new plaits::Voice;
        poly_allocators_[i] = new stmlib::BufferAllocator(poly_buffers_[i], kBufferSize);
        poly_voices_[i]->Init(poly_allocators_[i]);
    }
    
    // Initialize voice manager and mixer
    voice_manager_.Init();
    mixer_.Init();
    voice_count_ = 1;  // Default to monophonic
    
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
    
    // Initialize gate output state
    prev_lpg_gain_ = 0.0f;
    gate_out_trigger_counter_ = 0;
    clock_div_counter_ = 0;
    gate_out_state_ = false;
    
    // Initialize audio envelope processors (IN3 and IN4)
    audio_env_processor_3_.Init(sample_rate);
    audio_env_processor_4_.Init(sample_rate);
    
    // Initialize Moog ladder filter
    filter_.Init(sample_rate);
    
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
    
    // Frequency - HAS native attenuverter (patch->frequency_modulation_amount)
    params_[2] = mutables_ui::Parameter::Knob("Frequency", 0.0f, 1.0f, 0.5f);
    
    // Harmonics - NO native attenuverter in Plaits, we handle it
    params_[3] = mutables_ui::Parameter::Knob("Harmonics", 0.0f, 1.0f, 0.5f);
    
    // Timbre - HAS native attenuverter (patch->timbre_modulation_amount)
    params_[4] = mutables_ui::Parameter::Knob("Timbre", 0.0f, 1.0f, 0.5f);
    
    // Morph - HAS native attenuverter (patch->morph_modulation_amount)
    params_[5] = mutables_ui::Parameter::Knob("Morph", 0.0f, 1.0f, 0.5f);
    
    // Output level - CV input type (direct input, no attenuverter emulation)
    params_[6] = mutables_ui::Parameter::CV("Level");
    params_[6].value = 0.8f;  // Default level
    
    // V/Oct - CV input for pitch control (0-5V = ±30 semitones, 2.5V = 0)
    // Uses raw ADC for maximum precision
    params_[7] = mutables_ui::Parameter::CV("V/Oct");
    params_[7].value = 0.5f;  // Default to center (2.5V = 0 semitones)

    // Volume - scales output level (1.0 = full, useful for eurorack compatibility)
    // Can add velocity mod for standard velocity->volume behavior
    params_[8] = mutables_ui::Parameter::Knob("Volume", 0.0f, 1.0f, 1.0f);  // Default to full

    // Filter submenu (Moog ladder filter, post Stage 2)
    // Signal flow: Plaits -> Stage 1 -> Stage 2 -> Filter -> Output
    filter_params_[FILTER_MODE] = mutables_ui::Parameter::Enum("Mode", filter_mode_names, kNumFilterModes);
    filter_params_[FILTER_FREQ] = mutables_ui::Parameter::Knob("Freq", 0.0f, 1.0f, 0.7f);      // 20-20000 Hz (default ~2kHz)
    filter_params_[FILTER_FREQ].format_callback = FilterFormatCallback;
    filter_params_[FILTER_RESO] = mutables_ui::Parameter::Knob("Reso", 0.0f, 1.0f, 0.0f);      // 0-100%
    filter_params_[FILTER_RESO].format_callback = FilterFormatCallback;
    filter_params_[FILTER_DRIVE] = mutables_ui::Parameter::Knob("Drive", 0.0f, 1.0f, 0.25f);   // 0-100% (default 25%)
    filter_params_[FILTER_DRIVE].format_callback = FilterFormatCallback;
    filter_params_[FILTER_TRACK] = mutables_ui::Parameter::Knob("Track", 0.0f, 1.0f, 0.0f);    // Key tracking 0-100%
    filter_params_[FILTER_TRACK].format_callback = FilterFormatCallback;
    for (int i = 0; i < kNumFilterParams; i++) {
        filter_params_[i].visibility_callback = FilterVisibilityCallback;
    }
    params_[9] = mutables_ui::Parameter::Sub("Filter", filter_params_.data(), kNumFilterParams);

    // Settings submenu (groups LPG and other settings from Plaits manual)
    settings_params_[0] = mutables_ui::Parameter::Knob("LPG Color", 0.0f, 1.0f, 0.5f);
    settings_params_[1] = mutables_ui::Parameter::Knob("LPG Decay", 0.0f, 1.0f, 0.5f);
    settings_params_[2] = mutables_ui::Parameter::Enum("Octave", octave_names_, kNumOctaves);
    settings_params_[2].SetIndex(4);  // Default to C4 (middle C)
    settings_params_[3] = mutables_ui::Parameter::Enum("MIDI Ch", midi_channel_names_, kNumMidiChannels);
    settings_params_[3].SetIndex(0);  // Default to Omni
    settings_params_[4] = mutables_ui::Parameter::Enum("Voices", voice_count_names_, kNumVoiceCounts);
    settings_params_[4].SetIndex(0);  // Default to 1 voice (monophonic)
    params_[10] = mutables_ui::Parameter::Sub("Settings", settings_params_.data(), kNumSettingsParams);
    
    // CV Output 1 submenu
    SetupCVOutParams(cv_out1_params_, "CV1");
    params_[11] = mutables_ui::Parameter::Sub("CV Out 1", cv_out1_params_.data(), kNumCVOutParams);
    
    // CV Output 2 submenu
    SetupCVOutParams(cv_out2_params_, "CV2");
    params_[12] = mutables_ui::Parameter::Sub("CV Out 2", cv_out2_params_.data(), kNumCVOutParams);

    // Gate Output submenu
    gate_out_params_[GATEOUT_MODE] = mutables_ui::Parameter::Enum("Mode", gate_out_mode_names, kNumGateOutModes);
    gate_out_params_[GATEOUT_CLK_DIV] = mutables_ui::Parameter::Enum("ClkDiv", clk_div_names, kNumClkDivs);
    gate_out_params_[GATEOUT_PROB] = mutables_ui::Parameter::Knob("Prob", 0.0f, 1.0f, 0.5f);  // 0-100% probability
    gate_out_params_[GATEOUT_PROB].format_callback = GateOutFormatCallback;
    for (int i = 0; i < kNumGateOutParams; i++) {
        gate_out_params_[i].visibility_callback = GateOutVisibilityCallback;
    }
    params_[13] = mutables_ui::Parameter::Sub("Gate Out 1", gate_out_params_.data(), kNumGateOutParams);

    // Stage 1 submenu (IN1 = first Warps-style processing stage, 7 algorithms)
    audio_in1_params_[AUDIOMOD_MODE] = mutables_ui::Parameter::Enum("Mode", audio_in1_mode_names, kNumAudioIn1Modes);
    audio_in1_params_[AUDIOMOD_GAIN] = mutables_ui::Parameter::Knob("Gain", 0.0f, 1.0f, 0.0f);      // 1x-10x (+0dB to +20dB)
    audio_in1_params_[AUDIOMOD_GAIN].format_callback = AudioModFormatCallback;
    audio_in1_params_[AUDIOMOD_LEVEL] = mutables_ui::Parameter::Knob("Level", 0.0f, 1.0f, 1.0f);    // Modulator level
    audio_in1_params_[AUDIOMOD_LEVEL].format_callback = AudioModFormatCallback;
    audio_in1_params_[AUDIOMOD_TIMBRE] = mutables_ui::Parameter::Knob("Timbre", 0.0f, 1.0f, 0.5f);  // Algorithm-specific
    audio_in1_params_[AUDIOMOD_TIMBRE].format_callback = AudioModFormatCallback;
    for (int i = 0; i < kNumAudioIn1Params; i++) {
        audio_in1_params_[i].visibility_callback = AudioIn1VisibilityCallback;
    }
    params_[14] = mutables_ui::Parameter::Sub("Audio In 1", audio_in1_params_.data(), kNumAudioIn1Params);

    // Stage 2 submenu (IN2 = second Warps-style processing stage, 8 algorithms with Vocoder)
    audio_in2_params_[AUDIOMOD_MODE] = mutables_ui::Parameter::Enum("Mode", audio_in2_mode_names, kNumAudioIn2Modes);
    audio_in2_params_[AUDIOMOD_GAIN] = mutables_ui::Parameter::Knob("Gain", 0.0f, 1.0f, 0.0f);      // 1x-10x (+0dB to +20dB)
    audio_in2_params_[AUDIOMOD_GAIN].format_callback = AudioModFormatCallback;
    audio_in2_params_[AUDIOMOD_LEVEL] = mutables_ui::Parameter::Knob("Level", 0.0f, 1.0f, 1.0f);    // Modulator level
    audio_in2_params_[AUDIOMOD_LEVEL].format_callback = AudioModFormatCallback;
    audio_in2_params_[AUDIOMOD_TIMBRE] = mutables_ui::Parameter::Knob("Timbre", 0.0f, 1.0f, 0.5f);  // Algorithm-specific
    audio_in2_params_[AUDIOMOD_TIMBRE].format_callback = AudioModFormatCallback;
    for (int i = 0; i < kNumAudioIn2Params; i++) {
        audio_in2_params_[i].visibility_callback = AudioIn2VisibilityCallback;
    }
    params_[15] = mutables_ui::Parameter::Sub("Audio In 2", audio_in2_params_.data(), kNumAudioIn2Params);

    // Audio Input 3 submenu (IN3 = audio-derived modulation)
    SetupAudioInParams(audio_in3_params_);
    params_[16] = mutables_ui::Parameter::Sub("Audio In 3", audio_in3_params_.data(), kNumAudioInParams);
    
    // Audio Input 4 submenu (IN4 = audio-derived modulation, normalized to IN3)
    SetupAudioInParams(audio_in4_params_);
    params_[17] = mutables_ui::Parameter::Sub("Audio In 4", audio_in4_params_.data(), kNumAudioInParams);

    // User Data submenu - allows selecting custom user data files from SD card
    // Target indices match UserDataManager::Target enum
    user_data_params_[0] = mutables_ui::Parameter::UserData("6-Op Bk 1", 0);  // TARGET_SIX_OP_1
    user_data_params_[1] = mutables_ui::Parameter::UserData("6-Op Bk 2", 1);  // TARGET_SIX_OP_2
    user_data_params_[2] = mutables_ui::Parameter::UserData("6-Op Bk 3", 2);  // TARGET_SIX_OP_3
    user_data_params_[3] = mutables_ui::Parameter::UserData("WavTerrain", 3); // TARGET_WAVE_TERRAIN
    user_data_params_[4] = mutables_ui::Parameter::UserData("Wavetable", 4);  // TARGET_WAVETABLE
    params_[18] = mutables_ui::Parameter::Sub("User Data", user_data_params_.data(), kNumUserDataParams);
    
    // Save/Load presets
    params_[19] = mutables_ui::Parameter::Save();
    params_[20] = mutables_ui::Parameter::Load();
}

void PlaitsPort::SetupCVOutParams(std::array<mutables_ui::Parameter, kNumCVOutParams>& params, const char* name_prefix) {
    // Mode: LPG Env, AD, LFO, Follow
    params[0] = mutables_ui::Parameter::Enum("Mode", cv_out_mode_names, kNumCVOutModes);
    
    // AD envelope params (used when mode = AD)
    params[1] = mutables_ui::Parameter::Knob("Attack", 0.0f, 1.0f, 0.01f);    // 0.5ms-200ms log scaled
    params[2] = mutables_ui::Parameter::Knob("Release", 0.0f, 1.0f, 0.3f);    // 5ms-2000ms log scaled
    
    // LFO params (used when mode = LFO)
    params[3] = mutables_ui::Parameter::Enum("Shape", lfo_shape_names, kNumLFOShapes);
    
    // Shape-specific params (right after Shape for visibility)
    params[4] = mutables_ui::Parameter::Knob("Slew", 0.0f, 1.0f, 0.5f);       // RndSmth slew amount (0=fast, 1=slow)
    params[5] = mutables_ui::Parameter::Enum("SH Src", sh_source_names, kNumSHSources);  // S&H source
    
    // More LFO params
    params[6] = mutables_ui::Parameter::Enum("Sync", sync_names, kNumSyncModes);
    params[7] = mutables_ui::Parameter::Knob("Rate", 0.0f, 1.0f, 0.3f);       // 0.1-20Hz free, or ratio index when synced
    params[8] = mutables_ui::Parameter::Enum("Retrig", retrig_names, kNumRetrigOptions);  // Reset LFO phase on note trigger
    
    // Common params
    params[9] = mutables_ui::Parameter::Knob("Amp", 0.0f, 1.0f, 1.0f);        // Output amplitude scaling
    params[10] = mutables_ui::Parameter::Knob("Phase", 0.0f, 1.0f, 0.0f);     // LFO initial phase offset (0-360°)
    
    // Follow mode params (used when mode = Foll.3 or Foll.4)
    params[11] = mutables_ui::Parameter::Knob("Scale3", 0.0f, 1.0f, 0.5f);    // 0.0x to 2.0x scaling for IN3
    params[12] = mutables_ui::Parameter::Knob("Scale4", 0.0f, 1.0f, 0.5f);    // 0.0x to 2.0x scaling for IN4
    
    // Assign visibility and format callbacks to all params
    for (int i = 0; i < kNumCVOutParams; i++) {
        params[i].visibility_callback = CVOutVisibilityCallback;
        params[i].format_callback = CVOutFormatCallback;
    }
}

// Helper function to setup Audio In parameters
void PlaitsPort::SetupAudioInParams(std::array<mutables_ui::Parameter, kNumAudioInParams>& params) {
    params[AUDIOIN_MODE] = mutables_ui::Parameter::Enum("Mode", audio_in_mode_names, kNumAudioInModes);
    params[AUDIOIN_GAIN] = mutables_ui::Parameter::Knob("Gain", 0.0f, 1.0f, 0.0f);            // 1x-10x (+0dB to +20dB)
    params[AUDIOIN_GAIN].format_callback = AudioInFormatCallback;
    params[AUDIOIN_TIMBRE_AMT] = mutables_ui::Parameter::Knob("Tim. Mod", -1.0f, 1.0f, 0.0f);  // Bipolar
    params[AUDIOIN_TIMBRE_AMT].format_callback = BipolarPercentFormatCallback;
    params[AUDIOIN_MORPH_AMT] = mutables_ui::Parameter::Knob("Mrph Mod", -1.0f, 1.0f, 0.0f);   // Bipolar
    params[AUDIOIN_MORPH_AMT].format_callback = BipolarPercentFormatCallback;
    params[AUDIOIN_ATTACK] = mutables_ui::Parameter::Knob("Attack", 0.0f, 1.0f, 0.1f);        // 0.5ms-200ms log
    params[AUDIOIN_ATTACK].format_callback = AudioInFormatCallback;
    params[AUDIOIN_RELEASE] = mutables_ui::Parameter::Knob("Release", 0.0f, 1.0f, 0.3f);      // 5ms-2000ms log
    params[AUDIOIN_RELEASE].format_callback = AudioInFormatCallback;
    params[AUDIOIN_THRESHOLD] = mutables_ui::Parameter::Knob("Thresh", 0.0f, 1.0f, 0.3f);     // Trigger threshold
    params[AUDIOIN_THRESHOLD].format_callback = AudioInFormatCallback;
    params[AUDIOIN_HOLDOFF] = mutables_ui::Parameter::Knob("Holdoff", 0.0f, 1.0f, 0.2f);      // 20ms-200ms
    params[AUDIOIN_HOLDOFF].format_callback = AudioInFormatCallback;
    for (int i = 0; i < kNumAudioInParams; i++) {
        params[i].visibility_callback = AudioInVisibilityCallback;
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
    // FREQUENCY CALCULATION
    // ==========================================================================
    // - Octave param selects base octave (C0-C8)
    // - Frequency knob adds ±7 semitones fine tuning
    // - MIDI note is added as offset from C4 (note 60)
    // - V/Oct CV input adds ±30 semitones (2.5V = 0)
    // ==========================================================================
    
    int octave = settings_params_[2].GetIndex();  // 0-8 = C0-C8
    float frequency_knob = params_[2].value;
    
    // Base note from octave selection: C0=note 12, C1=24, ..., C8=108
    float octave_note = static_cast<float>((octave + 1) * 12);  // +1 because C0=12
    
    // Fine tuning from Frequency knob: ±7 semitones
    float fine_tune = (frequency_knob - 0.5f) * 14.0f;
    
    float base_note = octave_note + fine_tune;
    
    // MIDI note acts as V/Oct offset from C4 (middle C = note 60)
    // When MIDI note 60 is received, it adds 0 to base_note
    // MIDI note 72 adds +12 (one octave up), MIDI note 48 adds -12 (one octave down)
    float midi_offset = midi_note_ - kMidiNoteC4;
    
    // V/Oct CV input: 0-5V maps to ±30 semitones (2.5V = 0)
    // params_[7].value contains the raw CV (0.0-1.0)
    // Only apply V/Oct if the parameter is mapped to a CV input
    float voct_cv_offset = 0.0f;
    if (params_[7].mapping.IsCVSource()) {
        // Use raw CV value for precision (0.0-1.0 corresponds to 0-5V)
        float voct_raw = params_[7].value;
        voct_cv_offset = (voct_raw - kCVCenter) * kVOctRange;  // ±30 semitones
    }
    
    patch_->note = base_note + midi_offset + voct_cv_offset;
    
    // For parameters with CV mapping: use offset as base value when plugged
    // This lets Plaits add the CV modulation on top
    auto& harmonics = params_[3];
    auto& timbre = params_[4];
    auto& morph = params_[5];
    auto& frequency = params_[2];
    
    // Apply velocity modulation: vel_mod = velocity * velocity_amount
    // This is applied when USING values, not when storing them (to avoid feedback)
    float harm_vel = midi_velocity_ * harmonics.mapping.velocity_amount;
    float timb_vel = midi_velocity_ * timbre.mapping.velocity_amount;
    float morph_vel = midi_velocity_ * morph.mapping.velocity_amount;
    float lpg_col_vel = midi_velocity_ * settings_params_[0].mapping.velocity_amount;
    float lpg_dec_vel = midi_velocity_ * settings_params_[1].mapping.velocity_amount;
    
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
    patch_->lpg_colour = std::clamp(settings_params_[0].value + lpg_col_vel, 0.0f, 1.0f);
    patch_->decay = std::clamp(settings_params_[1].value + lpg_dec_vel, 0.0f, 1.0f);
    
    // Update polyphony settings
    // Voice count from Settings menu (index 0-3 = 1-4 voices)
    int new_voice_count = settings_params_[4].GetIndex() + 1;
    if (new_voice_count != voice_count_) {
        voice_count_ = new_voice_count;
        voice_manager_.SetVoiceCount(voice_count_);
    }
    
    // Enable/disable polyphony based on engine type
    // Polyphony is only available for lightweight engines
    int engine_index = patch_->engine;
    bool can_poly = IsPolyphonicEngine(engine_index) && (voice_count_ > 1);
    voice_manager_.SetPolyphonyEnabled(can_poly);
}

void PlaitsPort::SetCVModulations(float frequency_cv, float timbre_cv, float morph_cv) {
    frequency_cv_ = frequency_cv;
    timbre_cv_ = timbre_cv;
    morph_cv_ = morph_cv;
}

void PlaitsPort::SetRawCVInputs(float cv1, float cv2, float cv3, float cv4) {
    cv_inputs_[0] = cv1;
    cv_inputs_[1] = cv2;
    cv_inputs_[2] = cv3;
    cv_inputs_[3] = cv4;
}

void PlaitsPort::ReloadUserData() {
    if (voice_) {
        voice_->ReloadUserData();
    }
    // Also reload user data for polyphonic voices
    for (int i = 0; i < kMaxPolyVoices - 1; i++) {
        if (poly_voices_[i]) {
            poly_voices_[i]->ReloadUserData();
        }
    }
}

void PlaitsPort::RenderPolyphonicVoices(plaits::Voice::Frame* frames, size_t size) {
    if (!voice_manager_.IsPolyphonyEnabled()) {
        // Monophonic mode - just render voice 0 with base patch/modulations
        voice_->Render(*patch_, *modulations_, frames, size);
        return;
    }
    
    // Polyphonic mode - render each active voice and mix
    // Accumulator buffers for mixing (static to avoid stack allocation)
    // Aligned for NEON SIMD operations (processes 4 floats at once)
    alignas(16) static float out_accum[kPolyBlockSize];
    alignas(16) static float aux_accum[kPolyBlockSize];
    
    // Temporary frame buffer for each voice
    alignas(16) static plaits::Voice::Frame temp_frames[kPolyBlockSize];
    
    // Precomputed gain table (sqrt scaling) - avoids sqrt() in hot path
    static const float gain_table[5] = {
        32767.0f,             // 0 voices (not used)
        32767.0f,             // 1 voice
        32767.0f * 0.707f,    // 2 voices (1/sqrt(2))
        32767.0f * 0.577f,    // 3 voices (1/sqrt(3))
        32767.0f * 0.5f       // 4 voices (1/sqrt(4))
    };
    
    // Clear accumulators
    std::memset(out_accum, 0, size * sizeof(float));
    std::memset(aux_accum, 0, size * sizeof(float));
    
    int active_voices = 0;
    
    for (int v = 0; v < voice_count_; v++) {
        const VoiceSlot& slot = voice_manager_.GetSlot(v);
        
        // Skip inactive voices (no note assigned)
        if (!slot.IsActive()) continue;
        
        // Prepare per-voice parameters
        plaits::Patch voice_patch = *patch_;
        plaits::Modulations voice_mod = *modulations_;
        
        // Override note from voice slot
        voice_patch.note = static_cast<float>(slot.midi_note);
        
        // Set trigger based on gate state and just_triggered flag
        voice_mod.trigger = slot.just_triggered ? 1.0f : (slot.gate ? 0.1f : 0.0f);
        
        // Level: Use CV input if patched, otherwise use velocity
        // When Level CV is patched, it replaces the internal envelope control
        if (modulations_->level_patched) {
            // Use the Level CV value (with optional velocity sensitivity)
            voice_mod.level = modulations_->level * slot.velocity;
        } else {
            // Use velocity to control level (internal envelope active)
            voice_mod.level = slot.velocity;
        }
        
        // Get the appropriate voice object (voice_ for v=0, poly_voices_[v-1] for v>0)
        plaits::Voice* voice_obj = (v == 0) ? voice_ : poly_voices_[v - 1];
        
        // Render this voice
        voice_obj->Render(voice_patch, voice_mod, temp_frames, size);
        
        // Accumulate to mixer buffers
        const float scale = 1.0f / 32768.0f;
        for (size_t i = 0; i < size; i++) {
            out_accum[i] += temp_frames[i].out * scale;
            aux_accum[i] += temp_frames[i].aux * scale;
        }
        
        active_voices++;
        
        // Note: We don't automatically free decayed voices here.
        // Voices are only stolen when needed for new notes (in voice allocation logic).
    }
    
    // Write mixed output back to frames with gain compensation
    if (active_voices > 0) {
        // Use precomputed gain table (avoids sqrt in hot path)
        float gain = gain_table[std::min(active_voices, 4)];
        for (size_t i = 0; i < size; i++) {
            frames[i].out = static_cast<short>(std::clamp(out_accum[i] * gain, -32767.0f, 32767.0f));
            frames[i].aux = static_cast<short>(std::clamp(aux_accum[i] * gain, -32767.0f, 32767.0f));
            frames[i].out_dry = frames[i].out;
            frames[i].aux_dry = frames[i].aux;
        }
    } else {
        // No active voices - output silence
        for (size_t i = 0; i < size; i++) {
            frames[i].out = 0;
            frames[i].aux = 0;
            frames[i].out_dry = 0;
            frames[i].aux_dry = 0;
        }
    }
    
    // Clear trigger flags and update decay timers
    voice_manager_.ClearTriggerFlags();
}

void PlaitsPort::Process(float** in, float** out, size_t size) {
    if (!voice_ || !patch_ || !modulations_) return;
    
    UpdatePatchFromParams();
    UpdateCVModulatorsFromParams();
    UpdateAudioEnvFromParams(audio_env_processor_3_, audio_in3_params_);
    UpdateAudioEnvFromParams(audio_env_processor_4_, audio_in4_params_);
    
    // Plaits processes in blocks
    // Static buffer to avoid stack overflow in audio interrupt (96 frames = 768 bytes)
    alignas(16) static plaits::Voice::Frame frames[kBlockSize];
    
    for (size_t i = 0; i < size; i += kBlockSize) {
        size_t block_size = (i + kBlockSize <= size) ? kBlockSize : (size - i);
        
        // Process audio inputs for envelope/transient detection
        // IN3 = in[2], IN4 = in[3] (IN4 is normalized to IN3 if unplugged)
        const float* audio_in3_block = in[2] + i;
        const float* audio_in4_block = in[3] + i;
        audio_env_processor_3_.ProcessBlock(audio_in3_block, block_size);
        audio_env_processor_4_.ProcessBlock(audio_in4_block, block_size);
        
        // Get audio-derived modulations from both processors and combine
        float audio_timbre_mod = audio_env_processor_3_.GetTimbreModulation() 
                               + audio_env_processor_4_.GetTimbreModulation();
        float audio_morph_mod = audio_env_processor_3_.GetMorphModulation() 
                              + audio_env_processor_4_.GetMorphModulation();
        
        // Apply audio envelope modulation to patch values (additive, like velocity)
        // This bypasses the patched/unpatched logic and always works
        patch_->timbre = std::clamp(patch_->timbre + audio_timbre_mod, 0.0f, 1.0f);
        patch_->morph = std::clamp(patch_->morph + audio_morph_mod, 0.0f, 1.0f);
        
        // Check if transient detector triggered (for potential gate/trigger routing)
        // Either IN3 or IN4 transient can trigger (if in TRIG mode)
        bool audio_trigger_3 = audio_env_processor_3_.GetTrigger();
        bool audio_trigger_4 = audio_env_processor_4_.GetTrigger();
        
        // Use MIDI gate OR hardware gate OR audio transient trigger
        bool active_gate = midi_gate_ || gate_state_;
        if (audio_in3_params_[AUDIOIN_MODE].GetIndex() == 2 && audio_trigger_3) {
            // IN3 in TRIG mode: audio transients can trigger
            active_gate = true;
        }
        if (audio_in4_params_[AUDIOIN_MODE].GetIndex() == 2 && audio_trigger_4) {
            // IN4 in TRIG mode: audio transients can trigger
            active_gate = true;
        }
        
        // Set modulations - keep trigger high while gate is active
        // Plaits does its own edge detection internally
        modulations_->trigger = active_gate ? 1.0f : 0.0f;
        modulations_->level = params_[6].value;  // Level parameter (CV value if mapped/plugged)
        
        // CV modulation values (set by main.cpp via SetCVModulations)
        modulations_->frequency = frequency_cv_;
        modulations_->timbre = timbre_cv_;
        modulations_->morph = morph_cv_;
        
        // Patched status - true only when CV is mapped+plugged
        // This tells Plaits whether to use external modulation or internal envelope
        // CC mapping just replaces base value, doesn't count as patched
        auto& frequency = params_[2];
        auto& timbre = params_[4];
        auto& morph = params_[5];
        auto& level = params_[6];
        
        modulations_->frequency_patched = frequency.mapping.IsCVSource() && frequency.mapping.plugged;
        modulations_->timbre_patched = timbre.mapping.IsCVSource() && timbre.mapping.plugged;
        modulations_->morph_patched = morph.mapping.IsCVSource() && morph.mapping.plugged;
        modulations_->trigger_patched = true;  // Always patched via MIDI/Gate
        modulations_->level_patched = level.mapping.IsCVSource();  // Level parameter
        
        // Warps Lite Stage 1: Audio hybridization via IN1
        // Modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM
        int stage1_mode = audio_in1_params_[AUDIOMOD_MODE].GetIndex();
        if (stage1_mode > 0) {
            modulations_->audio_mod_in1 = in[0] + i;  // IN1
            modulations_->audio_mod_mode1 = stage1_mode;
            modulations_->audio_mod_gain1 = 1.0f + audio_in1_params_[AUDIOMOD_GAIN].value * 9.0f;  // 1x-10x
            modulations_->audio_mod_level1 = audio_in1_params_[AUDIOMOD_LEVEL].value;
            modulations_->audio_mod_timbre1 = audio_in1_params_[AUDIOMOD_TIMBRE].value;
        } else {
            modulations_->audio_mod_in1 = nullptr;
            modulations_->audio_mod_mode1 = 0;
            modulations_->audio_mod_gain1 = 1.0f;
            modulations_->audio_mod_level1 = 1.0f;
            modulations_->audio_mod_timbre1 = 0.5f;
        }
        
        // Warps Lite Stage 2: Audio hybridization via IN2 (chained after Stage 1)
        // Modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM, 8=VOCODER
        int stage2_mode = audio_in2_params_[AUDIOMOD_MODE].GetIndex();
        if (stage2_mode > 0) {
            modulations_->audio_mod_in2 = in[1] + i;  // IN2
            modulations_->audio_mod_mode2 = stage2_mode;
            modulations_->audio_mod_gain2 = 1.0f + audio_in2_params_[AUDIOMOD_GAIN].value * 9.0f;  // 1x-10x
            modulations_->audio_mod_level2 = audio_in2_params_[AUDIOMOD_LEVEL].value;
            modulations_->audio_mod_timbre2 = audio_in2_params_[AUDIOMOD_TIMBRE].value;
        } else {
            modulations_->audio_mod_in2 = nullptr;
            modulations_->audio_mod_mode2 = 0;
            modulations_->audio_mod_gain2 = 1.0f;
            modulations_->audio_mod_level2 = 1.0f;
            modulations_->audio_mod_timbre2 = 0.5f;
        }
        
        // Render audio (polyphonic or monophonic)
        RenderPolyphonicVoices(frames, block_size);
        
        // Process CV modulators (once per audio block)
        // Get LPG envelope from voice for LPG_ENV mode
        float lpg_gain = voice_->GetLPGGain();
        
        // ======================================================================
        // MOOG LADDER FILTER (post Stage 1 & Stage 2)
        // Signal flow: Plaits -> Stage1 -> Stage2 -> Filter -> Output
        // ======================================================================
        int filter_mode = filter_params_[FILTER_MODE].GetIndex();
        if (filter_mode > 0) {
            // Set filter mode (maps our 1-6 to DaisySP's enum)
            static const daisysp::LadderFilter::FilterMode mode_map[] = {
                daisysp::LadderFilter::FilterMode::LP24,  // placeholder for OFF (won't be used)
                daisysp::LadderFilter::FilterMode::LP12,  // 1 = LP12
                daisysp::LadderFilter::FilterMode::LP24,  // 2 = LP24
                daisysp::LadderFilter::FilterMode::BP12,  // 3 = BP12
                daisysp::LadderFilter::FilterMode::BP24,  // 4 = BP24
                daisysp::LadderFilter::FilterMode::HP12,  // 5 = HP12
                daisysp::LadderFilter::FilterMode::HP24   // 6 = HP24
            };
            filter_.SetFilterMode(mode_map[filter_mode]);
            
            // Calculate cutoff frequency with key tracking and envelope modulation
            // Base frequency: 20Hz to 20kHz (log scaled from 0-1 parameter)
            float freq_param = filter_params_[FILTER_FREQ].value;
            
            // Cache base frequency calculation (avoid expensive powf every block)
            if (freq_param != last_filter_freq_param_) {
                cached_filter_base_freq_ = 20.0f * powf(1000.0f, freq_param);  // 20 * 1000^value
                last_filter_freq_param_ = freq_param;
            }
            float base_freq = cached_filter_base_freq_;
            
            // Key tracking: 0-100% of note pitch applied to cutoff
            // Reference: C4 (MIDI 60) = no shift, each octave up doubles freq
            float tracking = filter_params_[FILTER_TRACK].value;
            float note_offset = (patch_->note - 60.0f) / 12.0f;  // Octaves from C4
            float tracking_multiplier = powf(2.0f, note_offset * tracking);
            
            // Envelope modulation: when Freq param has no CV plugged,
            // attenuverter controls how much LPG envelope modulates cutoff
            // Range: attenuverter -1 to +1 maps to -4 to +4 octaves of env modulation
            float env_mod_amount = 0.0f;
            if (!filter_params_[FILTER_FREQ].mapping.IsCVSource() || 
                !filter_params_[FILTER_FREQ].mapping.plugged) {
                // No CV plugged: use attenuverter for envelope modulation
                env_mod_amount = filter_params_[FILTER_FREQ].mapping.attenuverter;
            }
            float env_mod_multiplier = powf(2.0f, lpg_gain * env_mod_amount * 4.0f);
            
            // Final frequency with all modulations
            float final_freq = base_freq * tracking_multiplier * env_mod_multiplier;
            final_freq = std::clamp(final_freq, 20.0f, 20000.0f);
            filter_.SetFreq(final_freq);
            
            // Resonance: 0-1 parameter maps to 0-1.8 (DaisySP max for self-oscillation)
            float reso = filter_params_[FILTER_RESO].value * 1.8f;
            filter_.SetRes(reso);
            
            // Drive: 0-1 parameter maps to 0-4 (DaisySP range)
            float drive = filter_params_[FILTER_DRIVE].value * 4.0f;
            filter_.SetInputDrive(drive);
            
            // Apply filter to wet outputs (OUT and AUX)
            for (size_t j = 0; j < block_size; j++) {
                // Filter the main output
                float out_sample = static_cast<float>(frames[j].out) / kAudioScaleInt16;
                float filtered_out = filter_.Process(out_sample);
                frames[j].out = static_cast<int16_t>(std::clamp(filtered_out * kAudioScaleInt16, -kAudioScaleInt16, kAudioMaxInt16));
                
                // Note: AUX output could use a second filter instance for stereo,
                // but for CPU efficiency we apply the same filter coefficients
                // The aux signal passes through unchanged for now (common in mono synths)
            }
        }
        
        // Pass audio envelopes to CV modulators for FOLLOW_3 and FOLLOW_4 modes
        float audio_env_3 = audio_env_processor_3_.GetEnvelope();
        float audio_env_4 = audio_env_processor_4_.GetEnvelope();
        cv_modulator_1_.SetAudioEnvelope3(audio_env_3);
        cv_modulator_1_.SetAudioEnvelope4(audio_env_4);
        cv_modulator_2_.SetAudioEnvelope3(audio_env_3);
        cv_modulator_2_.SetAudioEnvelope4(audio_env_4);
        
        cv_modulator_1_.Process(lpg_gain, midi_clock_hz_, gate2_clock_hz_, cv_inputs_);
        cv_modulator_2_.Process(lpg_gain, midi_clock_hz_, gate2_clock_hz_, cv_inputs_);
        
        // Gate Output processing
        int gate_out_mode = gate_out_params_[GATEOUT_MODE].GetIndex();
        
        // EndEnv mode (1): detect when envelope ends (lpg_gain drops below threshold)
        if (gate_out_mode == 1) {
            if (prev_lpg_gain_ > kEnvThreshold && lpg_gain <= kEnvThreshold) {
                // Envelope just ended - set trigger
                gate_out_state_ = true;
                gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
            }
            prev_lpg_gain_ = lpg_gain;
        }
        
        // Countdown trigger pulse for modes that use it (Trigger, EndEnv, TrigProb, ClkDiv, ClkProb)
        if (gate_out_trigger_counter_ > 0) {
            if (gate_out_trigger_counter_ > block_size) {
                gate_out_trigger_counter_ -= block_size;
            } else {
                gate_out_trigger_counter_ = 0;
                gate_out_state_ = false;
            }
        }
        
        // Get volume with velocity modulation
        float vol_vel = midi_velocity_ * params_[8].mapping.velocity_amount;  // Volume parameter
        float volume = std::clamp(params_[8].value + vol_vel, 0.0f, 1.0f);
        
        // Convert from short to float, apply volume, and copy to outputs
        // OUT (wet) -> channel 1, AUX (wet) -> channel 2
        // OUT_DRY -> channel 3, AUX_DRY -> channel 4
        for (size_t j = 0; j < block_size && (i + j) < size; j++) {
            float out_sample = (static_cast<float>(frames[j].out) / kAudioScaleInt16) * volume;
            float aux_sample = (static_cast<float>(frames[j].aux) / kAudioScaleInt16) * volume;
            float out_dry_sample = (static_cast<float>(frames[j].out_dry) / kAudioScaleInt16) * volume;
            float aux_dry_sample = (static_cast<float>(frames[j].aux_dry) / kAudioScaleInt16) * volume;
            out[0][i + j] = out_sample;      // OUT (wet) -> channel 1
            out[1][i + j] = aux_sample;      // AUX (wet) -> channel 2
            out[2][i + j] = out_dry_sample;  // OUT (dry) -> channel 3
            out[3][i + j] = aux_dry_sample;  // AUX (dry) -> channel 4
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
        // Gate 1: Trigger input for AD envelopes
        // Detect rising edge for CV modulator triggers
        if (state && !gate_state_) {
            cv_modulator_1_.Trigger();
            cv_modulator_2_.Trigger();
            
            // Gate Output: Trigger and TrigProb modes respond to Gate 1 rise
            int gate_out_mode = gate_out_params_[GATEOUT_MODE].GetIndex();
            if (gate_out_mode == 0) {  // Trigger mode
                gate_out_state_ = true;
                gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);  // 10ms trigger
            } else if (gate_out_mode == 2) {  // TrigProb mode
                float prob = gate_out_params_[GATEOUT_PROB].value;
                float rand_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                if (rand_val < prob) {
                    gate_out_state_ = true;
                    gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
                }
            }
        }
        gate_state_ = state;
    } else if (gate_index == 1) {
        // Gate 2: Clock input for LFO sync and ClkDiv/ClkProb modes
        bool prev_gate2 = gate2_clock_tracker_.GetLastState();
        gate2_clock_tracker_.Process(state, sample_counter_);
        // Update clock Hz for modulators
        if (gate2_clock_tracker_.IsActive(sample_counter_, sample_rate_)) {
            gate2_clock_hz_ = gate2_clock_tracker_.GetClockHz(sample_rate_);
        } else {
            gate2_clock_hz_ = 0.0f;
        }
        
        // Gate 2 rising edge: process for ClkDiv and ClkProb modes (when no MIDI clock)
        if (state && !prev_gate2 && midi_clock_hz_ == 0.0f) {
            int gate_out_mode = gate_out_params_[GATEOUT_MODE].GetIndex();
            if (gate_out_mode == 3) {  // ClkDiv mode
                clock_div_counter_++;
                int divider_idx = gate_out_params_[GATEOUT_CLK_DIV].GetIndex();
                // For Gate 2 input, divide by the selected ratio directly (not ppq-based)
                static const int gate2_div_values[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32};
                if (clock_div_counter_ >= gate2_div_values[divider_idx]) {
                    clock_div_counter_ = 0;
                    gate_out_state_ = true;
                    gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
                }
            } else if (gate_out_mode == 4) {  // ClkProb mode
                float prob = gate_out_params_[GATEOUT_PROB].value;
                float rand_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                if (rand_val < prob) {
                    gate_out_state_ = true;
                    gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
                }
            }
        }
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
    cv_modulator_1_.SetMode(static_cast<CVOutMode>(cv_out1_params_[CVOUT_MODE].GetIndex()));
    cv_modulator_1_.SetAttack(cv_out1_params_[CVOUT_ATTACK].value);
    cv_modulator_1_.SetRelease(cv_out1_params_[CVOUT_RELEASE].value);
    cv_modulator_1_.SetLFOShape(static_cast<LFOShape>(cv_out1_params_[CVOUT_SHAPE].GetIndex()));
    cv_modulator_1_.SetSlewAmount(cv_out1_params_[CVOUT_SLEW].value);
    cv_modulator_1_.SetSHSource(static_cast<SHSource>(cv_out1_params_[CVOUT_SH_SRC].GetIndex()));
    cv_modulator_1_.SetSyncMode(static_cast<SyncMode>(cv_out1_params_[CVOUT_SYNC].GetIndex()));
    cv_modulator_1_.SetRate(cv_out1_params_[CVOUT_RATE].value);
    cv_modulator_1_.SetRetrig(cv_out1_params_[CVOUT_RETRIG].GetIndex() == 1);
    cv_modulator_1_.SetAmp(cv_out1_params_[CVOUT_AMP].value);
    cv_modulator_1_.SetPhaseOffset(cv_out1_params_[CVOUT_PHASE].value);
    cv_modulator_1_.SetFollowScale3(cv_out1_params_[CVOUT_SCALE3].value * 2.0f);  // 0-1 -> 0-2x
    cv_modulator_1_.SetFollowScale4(cv_out1_params_[CVOUT_SCALE4].value * 2.0f);  // 0-1 -> 0-2x
    
    // Update CV modulator 2 from its params
    cv_modulator_2_.SetMode(static_cast<CVOutMode>(cv_out2_params_[CVOUT_MODE].GetIndex()));
    cv_modulator_2_.SetAttack(cv_out2_params_[CVOUT_ATTACK].value);
    cv_modulator_2_.SetRelease(cv_out2_params_[CVOUT_RELEASE].value);
    cv_modulator_2_.SetLFOShape(static_cast<LFOShape>(cv_out2_params_[CVOUT_SHAPE].GetIndex()));
    cv_modulator_2_.SetSlewAmount(cv_out2_params_[CVOUT_SLEW].value);
    cv_modulator_2_.SetSHSource(static_cast<SHSource>(cv_out2_params_[CVOUT_SH_SRC].GetIndex()));
    cv_modulator_2_.SetSyncMode(static_cast<SyncMode>(cv_out2_params_[CVOUT_SYNC].GetIndex()));
    cv_modulator_2_.SetRate(cv_out2_params_[CVOUT_RATE].value);
    cv_modulator_2_.SetRetrig(cv_out2_params_[CVOUT_RETRIG].GetIndex() == 1);
    cv_modulator_2_.SetAmp(cv_out2_params_[CVOUT_AMP].value);
    cv_modulator_2_.SetPhaseOffset(cv_out2_params_[CVOUT_PHASE].value);
    cv_modulator_2_.SetFollowScale3(cv_out2_params_[CVOUT_SCALE3].value * 2.0f);  // 0-1 -> 0-2x
    cv_modulator_2_.SetFollowScale4(cv_out2_params_[CVOUT_SCALE4].value * 2.0f);  // 0-1 -> 0-2x
}

void PlaitsPort::UpdateAudioEnvFromParams(AudioEnvProcessor& processor, std::array<mutables_ui::Parameter, kNumAudioInParams>& params) {
    // Update audio envelope processor from its params
    int mode = params[AUDIOIN_MODE].GetIndex();
    processor.SetMode(static_cast<AudioEnvMode>(mode));
    
    // Input gain (normalized 0-1 -> 1x-10x for line level signals)
    processor.SetGainNormalized(params[AUDIOIN_GAIN].value);
    
    // Modulation amounts (bipolar -1 to +1)
    processor.SetTimbreAmount(params[AUDIOIN_TIMBRE_AMT].value);
    processor.SetMorphAmount(params[AUDIOIN_MORPH_AMT].value);
    
    // Envelope follower params (normalized 0-1, internally converted to log-scaled ms)
    processor.SetAttack(params[AUDIOIN_ATTACK].value);
    processor.SetRelease(params[AUDIOIN_RELEASE].value);
    
    // Transient detector params
    processor.SetThreshold(params[AUDIOIN_THRESHOLD].value);
    processor.SetHoldoff(params[AUDIOIN_HOLDOFF].value);
}

void PlaitsPort::OnMIDIClock() {
    midi_clock_tracker_.OnClock(sample_counter_);
    // Update clock Hz for modulators
    if (midi_clock_tracker_.IsActive(sample_counter_, sample_rate_)) {
        midi_clock_hz_ = midi_clock_tracker_.GetClockHz(sample_rate_);
    } else {
        midi_clock_hz_ = 0.0f;
    }
    
    // Gate Output clock modes (MIDI clock has priority over Gate 2)
    int gate_out_mode = gate_out_params_[GATEOUT_MODE].GetIndex();
    if (gate_out_mode == 3) {  // ClkDiv mode
        clock_div_counter_++;
        int divider = clk_div_values[gate_out_params_[GATEOUT_CLK_DIV].GetIndex()];
        if (clock_div_counter_ >= divider) {
            clock_div_counter_ = 0;
            gate_out_state_ = true;
            gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);  // 10ms trigger
        }
    } else if (gate_out_mode == 4) {  // ClkProb mode - trigger on each MIDI clock tick with probability
        float prob = gate_out_params_[GATEOUT_PROB].value;
        float rand_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        if (rand_val < prob) {
            gate_out_state_ = true;
            gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
        }
    }
}

void PlaitsPort::UpdateSampleCounter(size_t samples) {
    sample_counter_ += samples;
}

void PlaitsPort::NoteOn(uint8_t note, uint8_t velocity) {
    float vel_normalized = static_cast<float>(velocity) / 127.0f;
    
    // Pass lambda to query envelope values on-demand (only when stealing)
    voice_manager_.NoteOn(note, vel_normalized, [this](int voice_idx) -> float {
        if (voice_idx == 0) {
            return voice_->GetDecayEnvelope();
        } else {
            return poly_voices_[voice_idx - 1]->GetDecayEnvelope();
        }
    });
    
    // V/Oct and MIDI pitch are mutually exclusive
    // If V/Oct is mapped to a CV input, ignore MIDI note pitch
    // (MIDI clock and other messages still work, and all messages pass to MIDI out)
    if (!params_[7].mapping.IsCVSource()) {
        midi_note_ = static_cast<float>(note);
    }
    midi_velocity_ = vel_normalized;
    midi_gate_ = true;
    
    // Trigger CV modulators (for AD envelopes and LFO retrig)
    cv_modulator_1_.Trigger();
    cv_modulator_2_.Trigger();
    
    // Gate Output: Trigger and TrigProb modes respond to MIDI note on
    int gate_out_mode = gate_out_params_[GATEOUT_MODE].GetIndex();
    if (gate_out_mode == 0) {  // Trigger mode
        gate_out_state_ = true;
        gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);  // 10ms trigger
    } else if (gate_out_mode == 2) {  // TrigProb mode
        float prob = gate_out_params_[GATEOUT_PROB].value;
        float rand_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        if (rand_val < prob) {
            gate_out_state_ = true;
            gate_out_trigger_counter_ = static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
        }
    }
}

void PlaitsPort::NoteOff(uint8_t note, uint8_t velocity) {
    (void)velocity;
    
    // Route through voice manager for polyphony
    voice_manager_.NoteOff(note);
    
    // Legacy monophonic handling for backward compatibility
    // (This ensures midi_gate_ tracks the last released note)
    if (!params_[7].mapping.IsCVSource()) {
        if (static_cast<uint8_t>(midi_note_) == note) {
            midi_gate_ = false;
        }
    } else {
        // V/Oct mode: always release gate on any note off
        midi_gate_ = false;
    }
}

void PlaitsPort::AllNotesOff() {
    voice_manager_.AllNotesOff();
    midi_gate_ = false;
}

void PlaitsPort::Panic() {
    voice_manager_.Panic();
    midi_gate_ = false;
}

bool PlaitsPort::GetGateOutput() const {
    int mode = gate_out_params_[GATEOUT_MODE].GetIndex();
    
    switch(mode) {
        case 0:  // Trigger - Gate 1 rise OR MIDI note on
            return gate_out_trigger_counter_ > 0;
            
        case 1:  // EndEnv - Trigger at end of envelope
            return gate_out_state_;
            
        case 2:  // TrigProb - Trigger with probability
            return gate_out_trigger_counter_ > 0;
            
        case 3:  // ClkDiv - Clock divider
            return gate_out_state_;
            
        case 4:  // ClkProb - Clock tick with probability
            return gate_out_trigger_counter_ > 0;
            
        default:
            return false;
    }
}

} // namespace mutables_plaits
