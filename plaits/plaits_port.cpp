#include "plaits_port.h"
#include "../eurorack/plaits/dsp/voice.h"
#include "../eurorack/stmlib/utils/buffer_allocator.h"

namespace mutables_plaits {

// Engine names from Plaits
const char* PlaitsPort::engine_names_[] = {
    "Pair",      // 0: Pair of classic waveforms
    "WavTrn",    // 1: Waveshaping oscillator
    "FM",        // 2: Two operator FM
    "Grain",     // 3: Granular formant oscillator
    "Addtv",     // 4: Harmonic oscillator
    "WavTbl",    // 5: Wavetable oscillator
    "Chord",     // 6: Chords
    "Speech",    // 7: Speech synthesis
    "Swarm",     // 8: Swarm of sawtooths
    "Noise",     // 9: Filtered noise
    "Partcl",    // 10: Particle noise
    "String",    // 11: Inharmonic string modeling
    "Modal",     // 12: Modal resonator
    "Kick",      // 13: Analog kick drum
    "Snare",     // 14: Analog snare drum
    "HiHat"      // 15: Analog hi-hat
};

PlaitsPort::PlaitsPort() 
    : voice_(nullptr)
    , patch_(nullptr)
    , modulations_(nullptr)
    , allocator_(nullptr)
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
    params_[0] = mutables_ui::Parameter("Engine", engine_names_, kNumEngines);
    
    params_[1] = mutables_ui::Parameter("Harmonics", 0.0f, 1.0f);
    params_[1].cv_mapping.cv_input = 1;  // CV 2
    params_[1].cv_mapping.active = true;
    params_[1].cv_mapping.attenuverter = 1.0f;
    
    params_[2] = mutables_ui::Parameter("Timbre", 0.0f, 1.0f);
    params_[2].cv_mapping.cv_input = 2;  // CV 3
    params_[2].cv_mapping.active = true;
    params_[2].cv_mapping.attenuverter = 1.0f;
    
    params_[3] = mutables_ui::Parameter("Morph", 0.0f, 1.0f);
    params_[3].cv_mapping.cv_input = 3;  // CV 4
    params_[3].cv_mapping.active = true;
    params_[3].cv_mapping.attenuverter = 1.0f;
    
    params_[4] = mutables_ui::Parameter("Frequency", 0.0f, 1.0f);
    params_[4].value = 0.5f;  // Middle C
    params_[4].cv_mapping.cv_input = 0;  // CV 1
    params_[4].cv_mapping.active = true;
    params_[4].cv_mapping.attenuverter = 1.0f;
    
    params_[5] = mutables_ui::Parameter("LPG Colour", 0.0f, 1.0f);
    params_[6] = mutables_ui::Parameter("LPG Decay", 0.0f, 1.0f);
    params_[6].value = 0.5f;
    
    params_[7] = mutables_ui::Parameter("Level", 0.0f, 1.0f);
    params_[7].value = 0.8f;
}

void PlaitsPort::UpdatePatchFromParams() {
    if (!patch_) return;
    
    // Engine selection
    patch_->engine = params_[0].GetIndex();
    
    // Main parameters (normalized 0-1 to 0-65535)
    patch_->note = params_[4].value * 128.0f;  // MIDI note range
    patch_->harmonics = params_[1].value;
    patch_->timbre = params_[2].value;
    patch_->morph = params_[3].value;
    
    // LPG parameters
    patch_->lpg_colour = params_[5].value;
    patch_->decay = params_[6].value;
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
        modulations_->level = params_[7].value;
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
