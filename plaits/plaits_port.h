#pragma once

#include "../common/module_base.h"
#include "../common/parameter.h"
#include "cv_modulator.h"
#include "audio_processors.h"
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
    
    // Set raw CV input values for S&H source (0-1 each)
    void SetRawCVInputs(float cv1, float cv2, float cv3, float cv4);
    
    // Reload user data after file change (call voice_->ReloadUserData())
    void ReloadUserData();
    
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
    
    // Raw CV input values for S&H source
    float cv_inputs_[4];
    
    // Buffers
    static constexpr size_t kBlockSize = 24;
    static constexpr size_t kBufferSize = 32768;  // Buffer for Plaits engines
    uint8_t buffer_[kBufferSize];
    
    // Parameters
    static constexpr int kNumParams = 22;  // Bank, Engine, Frequency, Harmonics, Timbre, Morph, Level, V/Oct, LPG Color, LPG Decay, Octave, Volume, MIDI Ch, CV Out 1 (SUB), CV Out 2 (SUB), Gate Out (SUB), Audio In 2 (SUB), Audio In 3 (SUB), Audio In 4 (SUB), User Data (SUB), Save, Load
    static constexpr int kNumUserDataParams = 5;  // 3 FM banks + wavetable + wave terrain
    static constexpr int kNumCVOutParams = 12;  // Mode, Attack, Release, Shape, Slew (RndSmth), SH_Src (S&H), Sync, Rate, Amp, Phase, Scale3 (Foll.3), Scale4 (Foll.4)
    static constexpr int kNumGateOutParams = 2;  // Mode, Clk Div
    static constexpr int kNumAudioInParams = 8;  // Mode, Gain, Timbre±, Morph±, Attack, Release, Threshold, Holdoff
    static constexpr int kNumAudioIn2Params = 3;  // Mode, Gain, Amount (audio-rate modulation)
    std::array<mutables_ui::Parameter, kNumParams> params_;
    std::array<mutables_ui::Parameter, kNumUserDataParams> user_data_params_;  // Children of User Data submenu
    std::array<mutables_ui::Parameter, kNumCVOutParams> cv_out1_params_;  // Children of CV Out 1 submenu
    std::array<mutables_ui::Parameter, kNumCVOutParams> cv_out2_params_;  // Children of CV Out 2 submenu
    std::array<mutables_ui::Parameter, kNumGateOutParams> gate_out_params_;  // Children of Gate Out submenu
    std::array<mutables_ui::Parameter, kNumAudioIn2Params> audio_in2_params_;  // Children of Audio In 2 submenu (AM/RM/XFADE)
    std::array<mutables_ui::Parameter, kNumAudioInParams> audio_in3_params_;  // Children of Audio In 3 submenu
    std::array<mutables_ui::Parameter, kNumAudioInParams> audio_in4_params_;  // Children of Audio In 4 submenu
    
    // Bank and engine system
    static const char* bank_names_[];
    static const char* synth_engine_names_[];
    static const char* drum_engine_names_[];
    static const char* new_engine_names_[];
    static const char* octave_names_[];
    static const char* midi_channel_names_[];
    static constexpr int kNumBanks = 3;
    static constexpr int kNumSynthEngines = 8;
    static constexpr int kNumDrumEngines = 8;
    static constexpr int kNumNewEngines = 8;
    static constexpr int kNumOctaves = 9;  // C0-C8
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
    
    // CV Output modulators
    CVModulator cv_modulator_1_;
    CVModulator cv_modulator_2_;
    MIDIClockTracker midi_clock_tracker_;
    GateClockTracker gate2_clock_tracker_;
    float midi_clock_hz_;
    float gate2_clock_hz_;
    uint32_t sample_counter_;
    
    // Gate output state
    float prev_lpg_gain_;           // For end-of-envelope detection
    uint32_t gate_out_trigger_counter_;  // For trigger pulse
    int clock_div_counter_;         // For clock divider
    bool gate_out_state_;           // Current gate output state
    
    // Audio input processing (IN3 and IN4 = audio-derived modulation)
    AudioEnvProcessor audio_env_processor_3_;  // IN3
    AudioEnvProcessor audio_env_processor_4_;  // IN4
    
    void UpdatePatchFromParams();
    void SetupParameters();
    void UpdateEngineListForBank(int bank);
    int GetActualEngineIndex(int bank, int engine_in_bank);
    void UpdateCVModulatorsFromParams();
    void UpdateAudioEnvFromParams(AudioEnvProcessor& processor, std::array<mutables_ui::Parameter, kNumAudioInParams>& params);
    void SetupCVOutParams(std::array<mutables_ui::Parameter, kNumCVOutParams>& params, const char* name_prefix);
    void SetupAudioInParams(std::array<mutables_ui::Parameter, kNumAudioInParams>& params);
    
public:
    // MIDI interface
    void NoteOn(uint8_t note, uint8_t velocity);
    void NoteOff(uint8_t note, uint8_t velocity);
    float GetVelocity() const { return midi_velocity_; }
    
    // MIDI clock handling
    void OnMIDIClock();
    void UpdateSampleCounter(size_t samples);
    
    // Get MIDI channel setting: 0 = Omni (all), 1-16 = specific channel
    int GetMidiChannel() const { return params_[12].GetIndex(); }
    
    // Gate output
    bool GetGateOutput() const;
};

} // namespace mutables_plaits
