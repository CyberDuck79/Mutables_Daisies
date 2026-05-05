#include "clouds_port.h"
#include "../common/constants.h"
#include "../eurorack/clouds/dsp/granular_processor.h"
#include "../eurorack/clouds/dsp/parameters.h"
#include <cstring>

namespace mutables_clouds {

using namespace mutables;

const char *CloudsPort::playback_mode_names_[] = {"Granular", "Stretch", "Delay", "Spectral"};
const char *CloudsPort::quality_names_[] = {"Stereo 16b", "Mono 16b", "Stereo 8b", "Mono 8b"};
const char *CloudsPort::midi_channel_names_[] = {
    "Omni", "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",
    "9",    "10", "11", "12", "13", "14", "15", "16"};

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
  // Playback Mode
  params_[0] = mutables_ui::Parameter::Enum(
      "Mode", playback_mode_names_, kNumPlaybackModes);

  // Core parameters
  params_[1] = mutables_ui::Parameter::Knob("Position", 0.0f, 1.0f, 0.5f);
  params_[2] = mutables_ui::Parameter::Knob("Size", 0.0f, 1.0f, 0.5f);
  params_[3] = mutables_ui::Parameter::Knob("Pitch", 0.0f, 1.0f, 0.5f);
  params_[4] = mutables_ui::Parameter::Knob("Density", 0.0f, 1.0f, 0.5f);
  params_[5] = mutables_ui::Parameter::Knob("Texture", 0.0f, 1.0f, 0.5f);

  // Mix submenu
  mix_params_[0] = mutables_ui::Parameter::Knob("Dry/Wet", 0.0f, 1.0f, 0.5f);
  mix_params_[1] = mutables_ui::Parameter::Knob("Stereo Spread", 0.0f, 1.0f, 0.5f);
  mix_params_[2] = mutables_ui::Parameter::Knob("Feedback", 0.0f, 1.0f, 0.5f);
  mix_params_[3] = mutables_ui::Parameter::Knob("Reverb", 0.0f, 1.0f, 0.5f);
  params_[6] = mutables_ui::Parameter::Sub("Mix", mix_params_.data(), kNumMixParams);

  // Freeze
  params_[7] = mutables_ui::Parameter::Enum("Freeze", {"OFF", "ON"}, 2);

  // Settings submenu
  settings_params_[0] = mutables_ui::Parameter::Enum(
      "Quality", quality_names_, kNumQualityModes);
  settings_params_[1] = mutables_ui::Parameter::Enum(
      "MIDI Ch", midi_channel_names_, kNumMidiChannels);
  settings_params_[1].SetIndex(0); // Omni
  settings_params_[2] = mutables_ui::Parameter::Calibration();
  params_[8] = mutables_ui::Parameter::Sub(
      "Settings", settings_params_.data(), kNumSettingsParams);

  // Audio In L/R (CV input config)
  audio_in_params_[0][0] = mutables_ui::Parameter::CV("Audio In L");
  params_[9] = mutables_ui::Parameter::Sub(
      "Audio In L", audio_in_params_[0].data(), kNumAudioInParams);
  audio_in_params_[1][0] = mutables_ui::Parameter::CV("Audio In R");
  params_[10] = mutables_ui::Parameter::Sub(
      "Audio In R", audio_in_params_[1].data(), kNumAudioInParams);

  // CV Out 1/2 (optional modulation output)
  // Placeholder - would use cv_out template like Plaits
  params_[11] = mutables_ui::Parameter::Sub("CV Out 1", cv_out_params_[0].data(), kNumCVOutParams);
  params_[12] = mutables_ui::Parameter::Sub("CV Out 2", cv_out_params_[1].data(), kNumCVOutParams);

  // Gate Out (optional)
  params_[13] = mutables_ui::Parameter::Sub(
      "Gate Out", gate_out_params_.data(), kNumGateOutParams);

  // Save/Load
  params_[14] = mutables_ui::Parameter::Save();
  params_[15] = mutables_ui::Parameter::Load();
}

void CloudsPort::UpdateParametersToDSP() {
  if (!processor_) return;

  clouds::Parameters *p = processor_->mutable_parameters();

  p->position = params_[1].value;
  p->size = params_[2].value;
  p->pitch = (params_[3].value - 0.5f) * 96.0f;  // ±48 semitones
  p->density = params_[4].value;
  p->texture = params_[5].value;
  p->dry_wet = mix_params_[0].value;
  p->stereo_spread = mix_params_[1].value;
  p->feedback = mix_params_[2].value;
  p->reverb = mix_params_[3].value;
  p->freeze = params_[7].GetIndex() == 1 || freeze_state_;
  p->trigger = trigger_state_;
}

void CloudsPort::Process(float **in, float **out, size_t size) {
  if (!processor_) return;

  UpdateParametersToDSP();

  // Process in kBlockSize (32) chunks
  for (size_t i = 0; i < size; i += kBlockSize) {
    size_t block = (i + kBlockSize <= size) ? kBlockSize : (size - i);

    // Float → ShortFrame conversion (input)
    // Use 32767.0f to avoid overflow at exactly 1.0 (32768 would overflow int16)
    for (size_t j = 0; j < block; j++) {
      input_frames_[j].l = static_cast<int16_t>(
          std::clamp(in[0][i + j], -1.0f, 1.0f) * 32767.0f);
      input_frames_[j].r = static_cast<int16_t>(
          std::clamp(in[1][i + j], -1.0f, 1.0f) * 32767.0f);
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
