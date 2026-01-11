#pragma once

#include "../common/module_base.h"
#include "../common/parameter.h"
#include <array>

// Forward declarations for Plaits classes
namespace plaits {
class Voice;
struct Modulations;
struct Patch;
}

namespace stmlib {
class BufferAllocator;
}

namespace mutables_plaits {

class PlaitsPort : public mutables_ui::ModuleBase {
public:
    PlaitsPort();
    ~PlaitsPort() override;
    
    // ModuleBase interface
    const char* GetName() const override { return "Plaits"; }
    const char* GetShortName() const override { return "plaits"; }
    
    void Init(float sample_rate) override;
    void Process(float** in, float** out, size_t size) override;
    mutables_ui::Parameter* GetParameters() override;
    size_t GetParameterCount() const override;
    
    void ProcessGate(int gate_index, bool state) override;
    float GetCVOutput(int cv_index) override;
    
    // Set CV modulation values (called from main before Process)
    void SetCVModulations(float frequency_cv, float timbre_cv, float morph_cv);
    
private:
    // Plaits engine
    plaits::Voice* voice_;
    plaits::Patch* patch_;
    plaits::Modulations* modulations_;
    stmlib::BufferAllocator* allocator_;
    
    // CV modulation values from mapped inputs
    float frequency_cv_;
    float timbre_cv_;
    float morph_cv_;
    
    // Buffers
    static constexpr size_t kBlockSize = 24;
    static constexpr size_t kBufferSize = 32768;  // Buffer for Plaits engines
    uint8_t buffer_[kBufferSize];
    
    // Parameters
    static constexpr int kNumParams = 12;  // Bank, Engine, Freq.Rng, Frequency, Harmonics, Timbre, Morph, Level, LPG Color, LPG Decay, Volume, MIDI Ch
    std::array<mutables_ui::Parameter, kNumParams> params_;
    
    // Bank and engine system
    static const char* bank_names_[];
    static const char* synth_engine_names_[];
    static const char* drum_engine_names_[];
    static const char* new_engine_names_[];
    static const char* freq_range_names_[];
    static const char* midi_channel_names_[];
    static constexpr int kNumBanks = 3;
    static constexpr int kNumSynthEngines = 8;
    static constexpr int kNumDrumEngines = 8;
    static constexpr int kNumNewEngines = 8;
    static constexpr int kNumFreqRanges = 10;  // C0-C8 + full range
    static constexpr int kNumMidiChannels = 17;  // Omni + 1-16
    
    int current_bank_;
    
    // MIDI state
    float midi_note_;      // Current MIDI note (0-127)
    float midi_velocity_;  // Current MIDI velocity (0.0-1.0)
    bool midi_gate_;       // Gate from MIDI note on/off
    
    // State
    bool gate_state_;
    bool previous_gate_;   // For trigger detection
    float sample_rate_;
    
    void UpdatePatchFromParams();
    void SetupParameters();
    void UpdateEngineListForBank(int bank);
    int GetActualEngineIndex(int bank, int engine_in_bank);
    
public:
    // MIDI interface
    void NoteOn(uint8_t note, uint8_t velocity);
    void NoteOff(uint8_t note, uint8_t velocity);
    float GetVelocity() const { return midi_velocity_; }
    
    // Get MIDI channel setting: 0 = Omni (all), 1-16 = specific channel
    int GetMidiChannel() const { return params_[11].GetIndex(); }
};

} // namespace mutables_plaits
