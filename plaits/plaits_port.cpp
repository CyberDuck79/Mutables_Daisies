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
    , gate_state_(false)
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
    
    // Initialize voice with buffer allocator
    voice_->Init(allocator_);
    
    // Setup parameters
    SetupParameters();
    
    // Initialize patch with default values
    UpdatePatchFromParams();
}

void PlaitsPort::SetupParameters() {
    params_[0] = mutables_ui::Parameter("Bank", bank_names_, kNumBanks);
    params_[1] = mutables_ui::Parameter("Engine", synth_engine_names_, kNumSynthEngines);
    current_bank_ = 0;
    
    params_[2] = mutables_ui::Parameter("Harmonics", 0.0f, 1.0f);
    params_[2].cv_mapping.cv_input = 1;  // CV 2
    params_[2].cv_mapping.active = true;
    params_[2].cv_mapping.attenuverter = 1.0f;
    
    params_[3] = mutables_ui::Parameter("Timbre", 0.0f, 1.0f);
    params_[3].cv_mapping.cv_input = 2;  // CV 3
    params_[3].cv_mapping.active = true;
    params_[3].cv_mapping.attenuverter = 1.0f;
    
    params_[4] = mutables_ui::Parameter("Morph", 0.0f, 1.0f);
    params_[4].cv_mapping.cv_input = 3;  // CV 4
    params_[4].cv_mapping.active = true;
    params_[4].cv_mapping.attenuverter = 1.0f;
    
    params_[5] = mutables_ui::Parameter("Frequency", 0.0f, 1.0f);
    params_[5].value = 0.5f;  // Middle C
    params_[5].cv_mapping.cv_input = 0;  // CV 1
    params_[5].cv_mapping.active = true;
    params_[5].cv_mapping.attenuverter = 1.0f;
    
    params_[6] = mutables_ui::Parameter("LPG Colour", 0.0f, 1.0f);
    params_[7] = mutables_ui::Parameter("LPG Decay", 0.0f, 1.0f);
    params_[7].value = 0.5f;
    
    params_[8] = mutables_ui::Parameter("Level", 0.0f, 1.0f);
    params_[8].value = 0.8f;
}

void PlaitsPort::UpdateEngineListForBank(int bank) {
    if (bank == current_bank_) return;
    
    current_bank_ = bank;
    
    // Reset engine selection to 0 when changing banks
    switch (bank) {
        case 0:  // Synth
            params_[1] = mutables_ui::Parameter("Engine", synth_engine_names_, kNumSynthEngines);
            break;
        case 1:  // Drum
            params_[1] = mutables_ui::Parameter("Engine", drum_engine_names_, kNumDrumEngines);
            break;
        case 2:  // New
            params_[1] = mutables_ui::Parameter("Engine", new_engine_names_, kNumNewEngines);
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
    
    // Main parameters (normalized 0-1 to 0-65535)
    patch_->note = params_[5].value * 128.0f;  // MIDI note range
    patch_->harmonics = params_[2].value;
    patch_->timbre = params_[3].value;
    patch_->morph = params_[4].value;
    
    // LPG parameters
    patch_->lpg_colour = params_[6].value;
    patch_->decay = params_[7].value;
}

void PlaitsPort::Process(float** in, float** out, size_t size) {
    if (!voice_ || !patch_ || !modulations_) return;
    
    UpdatePatchFromParams();
    
    // Plaits processes in blocks
    plaits::Voice::Frame frames[kBlockSize];
    
    for (size_t i = 0; i < size; i += kBlockSize) {
        size_t block_size = (i + kBlockSize <= size) ? kBlockSize : (size - i);
        
        // Set modulations (from CV inputs - will be connected later)
        modulations_->trigger = gate_state_ ? 0.8f : 0.0f;
        modulations_->level = params_[8].value;
        modulations_->frequency_patched = false;
        modulations_->timbre_patched = false;
        modulations_->morph_patched = false;
        modulations_->trigger_patched = false;
        modulations_->level_patched = false;
        
        // Render audio
        voice_->Render(*patch_, *modulations_, frames, block_size);
        
        // Convert from short to float and copy to output
        for (size_t j = 0; j < block_size && (i + j) < size; j++) {
            out[0][i + j] = static_cast<float>(frames[j].out) / 32768.0f;
            out[1][i + j] = static_cast<float>(frames[j].aux) / 32768.0f;
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

} // namespace mutables_plaits
