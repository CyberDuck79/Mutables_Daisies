#include "clouds_port.h"
#include "../common/constants.h"
#include "../common/parameter_templates/audio_input_config.h"
#include "../common/parameter_templates/cv_output_config.h"
#include "../common/parameter_templates/gate_output_config.h"
#include "../eurorack/clouds/dsp/granular_processor.h"
#include "../eurorack/clouds/dsp/parameters.h"
#include <cstring>

namespace mutables_clouds {

using namespace mutables;

CloudsPort::CloudsPort()
    : processor_(nullptr),
      freeze_state_(false),
      trigger_state_(false),
      previous_gate1_(false),
      previous_gate2_(false),
      sample_rate_(kDefaultSampleRate) {
  std::memset(input_frames_, 0, sizeof(input_frames_));
  std::memset(output_frames_, 0, sizeof(output_frames_));
}

CloudsPort::~CloudsPort() {
  if (processor_) {
    delete processor_;
  }
}

void CloudsPort::Init(float sample_rate) {
  sample_rate_ = sample_rate;

  // Allocate and initialize the GranularProcessor
  processor_ = new clouds::GranularProcessor;
  processor_->Init(
      large_buffer_, kLargeBufferSize,
      small_buffer_, kSmallBufferSize);

  // Set default playback mode
  processor_->set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);
  processor_->Prepare();

  SetupParameters();
}

void CloudsPort::SetupParameters() {
  // Playback Mode — count deduced from array size at compile time
  params_[MODE] = mutables_ui::Parameter::Enum("Mode", playback_mode_names_);

  // Core parameters
  params_[POSITION] = mutables_ui::Parameter::Knob("Position", 0.0f, 1.0f, 0.5f);
  params_[SIZE] = mutables_ui::Parameter::Knob("Size", 0.0f, 1.0f, 0.5f);
  params_[PITCH] = mutables_ui::Parameter::Knob("Pitch", 0.0f, 1.0f, 0.5f);
  params_[DENSITY] = mutables_ui::Parameter::Knob("Density", 0.0f, 1.0f, 0.5f);
  params_[TEXTURE] = mutables_ui::Parameter::Knob("Texture", 0.0f, 1.0f, 0.5f);

  // Mix submenu
  mix_params_[0] = mutables_ui::Parameter::Knob("Dry/Wet", 0.0f, 1.0f, 0.5f);
  mix_params_[1] = mutables_ui::Parameter::Knob("Stereo Spread", 0.0f, 1.0f, 0.5f);
  mix_params_[2] = mutables_ui::Parameter::Knob("Feedback", 0.0f, 1.0f, 0.5f);
  mix_params_[3] = mutables_ui::Parameter::Knob("Reverb", 0.0f, 1.0f, 0.5f);
  params_[MIX] = mutables_ui::Parameter::Sub("Mix", mix_params_.data(), kNumMixParams);

  // Freeze — count deduced from array size
  static const char *freeze_labels_[] = {"OFF", "ON"};
  params_[FREEZE] = mutables_ui::Parameter::Enum("Freeze", freeze_labels_);

  // Settings submenu — counts deduced from array sizes
  params_[SETTINGS] = mutables_ui::Parameter::Sub(
      "Settings", settings_params_.data(), kNumSettingsParams);
  settings_params_[0] = mutables_ui::Parameter::Enum("Quality", quality_names_);
  settings_params_[1] = mutables_ui::Parameter::Enum("MIDI Ch", midi_channel_names_);
  settings_params_[1].SetIndex(0); // Omni
  settings_params_[2] = mutables_ui::Parameter::Calibration();

  // Audio In L/R (CV input config) - use template
  mutables_ui::templates::audio_in::Setup(audio_in_params_[0]);
  params_[AUDIO_IN_L] = mutables_ui::Parameter::Sub(
      "Audio In L", audio_in_params_[0].data(), kNumAudioInParams);
  mutables_ui::templates::audio_in::Setup(audio_in_params_[1]);
  params_[AUDIO_IN_R] = mutables_ui::Parameter::Sub(
      "Audio In R", audio_in_params_[1].data(), kNumAudioInParams);

  // CV Out 1/2 (optional modulation output) - use template
  mutables_ui::templates::cv_out::Setup(cv_out_params_[0]);
  params_[CV_OUT_1] = mutables_ui::Parameter::Sub("CV Out 1", cv_out_params_[0].data(), kNumCVOutParams);
  mutables_ui::templates::cv_out::Setup(cv_out_params_[1]);
  params_[CV_OUT_2] = mutables_ui::Parameter::Sub("CV Out 2", cv_out_params_[1].data(), kNumCVOutParams);

  // Gate Out (optional) - use template
  mutables_ui::templates::gate_out::Setup(gate_out_params_);
  params_[GATE_OUT] = mutables_ui::Parameter::Sub(
      "Gate Out", gate_out_params_.data(), kNumGateOutParams);

  // Save/Load
  params_[SAVE] = mutables_ui::Parameter::Save();
  params_[LOAD] = mutables_ui::Parameter::Load();

  // Compile-time validation: ensure all enum indices are within array bounds
  static_assert(PARAM_COUNT <= params_.size(), "PARAM_COUNT exceeds array size");
}

void CloudsPort::UpdateParametersToDSP() {
  if (!processor_) return;

  clouds::Parameters *p = processor_->mutable_parameters();

  p->position = params_[POSITION].value;
  p->size = params_[SIZE].value;
  p->pitch = (params_[PITCH].value - 0.5f) * 96.0f;  // ±48 semitones
  p->density = params_[DENSITY].value;
  p->texture = params_[TEXTURE].value;
  p->dry_wet = mix_params_[0].value;
  p->stereo_spread = mix_params_[1].value;
  p->feedback = mix_params_[2].value;
  p->reverb = mix_params_[3].value;
  p->freeze = params_[FREEZE].GetIndex() == 1 || freeze_state_;
  p->trigger = trigger_state_;
}

void CloudsPort::Process(float **in, float **out, size_t size) {
  if (!processor_) return;

  UpdateParametersToDSP();

  // Process in kBlockSize (32) chunks
  for (size_t i = 0; i < size; i += kBlockSize) {
    size_t block = (i + kBlockSize <= size) ? kBlockSize : (size - i);

    // Float → ShortFrame conversion (input)
    // Matches original: int16 = float * 32768.0f (no extra clamping)
    // SoftConvert is applied inside GranularProcessor::Process()
    for (size_t j = 0; j < block; j++) {
      input_frames_[j].l = static_cast<int16_t>(in[0][i + j] * 32768.0f);
      input_frames_[j].r = static_cast<int16_t>(in[1][i + j] * 32768.0f);
    }

    // Pad remaining samples if block < 32
    if (block < kBlockSize) {
      for (size_t j = block; j < kBlockSize; j++) {
        input_frames_[j].l = 0;
        input_frames_[j].r = 0;
      }
    }

    // Process through Clouds DSP
    processor_->Process(input_frames_, output_frames_, kBlockSize);

    // ShortFrame → Float conversion (output)
    for (size_t j = 0; j < block; j++) {
      out[0][i + j] = static_cast<float>(output_frames_[j].l) / 32768.0f;
      out[1][i + j] = static_cast<float>(output_frames_[j].r) / 32768.0f;
    }

    // Clear remaining output samples
    for (size_t j = block; j < size; j++) {
      out[0][i + j] = 0.0f;
      out[1][i + j] = 0.0f;
    }
  }
}

void CloudsPort::Prepare() {
  processor_->Prepare();
  // Memory barrier: ensures reset_buffers_ = false in Prepare()
  // is visible to the audio callback before it resumes processing.
  // Single-core ARM is safe from hardware races, but this prevents
  // compiler reordering of the store.
  __DSB();
}

void CloudsPort::SetPlaybackMode(int mode) {
  if (!processor_) return;
  // Bounds check: prevent invalid enum values that cause DSP silence
  if (mode < 0 || mode >= static_cast<int>(kNumPlaybackModes)) return;
  processor_->set_playback_mode(
      static_cast<clouds::PlaybackMode>(mode));
}

int CloudsPort::GetPlaybackMode() const {
  if (!processor_) return 0;
  return static_cast<int>(processor_->playback_mode());
}

void CloudsPort::SetFreeze(bool freeze) {
  freeze_state_ = freeze;
}

void CloudsPort::ProcessGate(int gate_index, bool state) {
  if (gate_index == 0) {
    // Gate 1: Freeze (level-triggered)
    freeze_state_ = state;
    previous_gate1_ = state;
  } else if (gate_index == 1) {
    // Gate 2: Trigger (rising edge)
    if (state && !previous_gate2_) {
      trigger_state_ = true;
    } else if (!state) {
      trigger_state_ = false;
    }
    previous_gate2_ = state;
  }
}

mutables_ui::Parameter *CloudsPort::GetParameters() {
  return params_.data();
}

size_t CloudsPort::GetParameterCount() const {
  return params_.size();
}

} // namespace mutables_clouds
