#pragma once

#include "../common/module_base.h"
#include "../common/parameter.h"
#include "../eurorack/clouds/dsp/frame.h"
#include <array>
#include <cstddef>

// Forward declarations for Clouds DSP classes
namespace clouds {
class GranularProcessor;
}

namespace mutables_clouds {

//===========================================================================
// CloudsPort - Daisy Patch port of Mutable Instruments Clouds
//===========================================================================
// Audio effect port (processes input → output), unlike Plaits which generates.
// Runs at 32 kHz global sample rate to match original firmware.
//===========================================================================

class CloudsPort : public mutables_ui::ModuleBase {
public:
  CloudsPort();
  ~CloudsPort() override;

  //===========================================================================
  // Parameter indices — public for main loop access
  //===========================================================================
  enum ParamIndex : uint8_t {
    MODE = 0,
    POSITION,
    SIZE,
    PITCH,
    DENSITY,
    TEXTURE,
    MIX,
    FREEZE,
    SETTINGS,
    AUDIO_IN_L,
    AUDIO_IN_R,
    CV_OUT_1,
    CV_OUT_2,
    GATE_OUT,
    SAVE,
    LOAD,
    PARAM_COUNT
  };

  static constexpr size_t kNumParams = PARAM_COUNT;

  //===========================================================================
  // ModuleBase Interface
  //===========================================================================
  const char *GetName() const override { return "Clouds"; }
  const char *GetShortName() const override { return "clouds"; }

  void Init(float sample_rate) override;
  void Process(float **in, float **out, size_t size) override;
  mutables_ui::Parameter *GetParameters() override;
  size_t GetParameterCount() const override;

  void ProcessGate(int gate_index, bool state) override;

  //===========================================================================
  // Clouds-specific: Prepare() called from main loop
  //===========================================================================
  // Matches original firmware pattern: Prepare() runs every main loop iteration.
  // Process() outputs silence while reset_buffers_ is true, ensuring safety.
  // __DSB() memory barrier after reset_buffers_ = false prevents compiler
  // reordering on single-core ARM.
  //===========================================================================
  void Prepare();

  //===========================================================================
  // Playback mode access for main loop
  //===========================================================================
  void SetPlaybackMode(int mode);
  int GetPlaybackMode() const;

  //===========================================================================
  // Freeze control (Gate 1)
  //===========================================================================
  void SetFreeze(bool freeze);

private:
  // Buffer size constants
  static constexpr size_t kBlockSize = 32;  // Matches Clouds kMaxBlockSize
  static constexpr size_t kLargeBufferSize = 118784;  // ~116 KB main buffer
  static constexpr size_t kSmallBufferSize = 65408;   // ~64 KB CCM buffer

  // DSP engine
  clouds::GranularProcessor *processor_;

  // Audio buffers (placed in AXI SRAM)
  alignas(4) uint8_t large_buffer_[kLargeBufferSize];
  alignas(4) uint8_t small_buffer_[kSmallBufferSize];

  // Audio I/O conversion buffers
  clouds::ShortFrame input_frames_[kBlockSize];
  clouds::ShortFrame output_frames_[kBlockSize];

  // Gate state
  bool freeze_state_;
  bool trigger_state_;
  bool previous_gate1_;
  bool previous_gate2_;

  // Sample rate
  float sample_rate_;

  //===========================================================================
  // Parameter sub-arrays
  //===========================================================================
  static constexpr int kNumMixParams = 4;
  static constexpr int kNumSettingsParams = 3;
  static constexpr int kNumAudioInParams = 8;  // Matches audio_in template
  static constexpr int kNumCVOutParams = 13;   // Matches cv_out template
  static constexpr int kNumGateOutParams = 3;  // Matches gate_out template

  std::array<mutables_ui::Parameter, kNumParams> params_;
  std::array<mutables_ui::Parameter, kNumMixParams> mix_params_;
  std::array<mutables_ui::Parameter, kNumSettingsParams> settings_params_;
  std::array<mutables_ui::Parameter, kNumAudioInParams> audio_in_params_[2];
  std::array<mutables_ui::Parameter, kNumCVOutParams> cv_out_params_[2];
  std::array<mutables_ui::Parameter, kNumGateOutParams> gate_out_params_;

  // Playback mode names
  static const char *playback_mode_names_[];
  static constexpr int kNumPlaybackModes = 4;

  // Quality mode names
  static const char *quality_names_[];
  static constexpr int kNumQualityModes = 4;

  // MIDI channel names
  static const char *midi_channel_names_[];
  static constexpr int kNumMidiChannels = 17;

  void SetupParameters();
  void UpdateParametersToDSP();
};

} // namespace mutables_clouds
