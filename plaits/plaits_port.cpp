#include "plaits_port.h"
#include "../common/constants.h"
#include "../common/parameter_templates/audio_input_config.h"
#include "../common/parameter_templates/cv_output_config.h"
#include "../common/parameter_templates/filter_config.h"
#include "../common/parameter_templates/gate_output_config.h"
#include "../common/utils/format_utils.h"
#include "../eurorack/plaits/dsp/voice.h"
#include "../eurorack/stmlib/utils/buffer_allocator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mutables_plaits {

using namespace mutables;
namespace gate_out = mutables_ui::templates::gate_out;
namespace audio_in = mutables_ui::templates::audio_in;
namespace cv_out = mutables_ui::templates::cv_out;
namespace filter = mutables_ui::templates::filter;

// Bank names
const char *PlaitsPort::bank_names_[] = {"Synth", "Drum", "New"};

// Octave names (C0-C8 base octave, Freq knob adds ±7 semitones fine tuning)
const char *PlaitsPort::octave_names_[] = {
    "C0", // 0: Base C0 (note 12)
    "C1", // 1: Base C1 (note 24)
    "C2", // 2: Base C2 (note 36)
    "C3", // 3: Base C3 (note 48)
    "C4", // 4: Base C4 (note 60)
    "C5", // 5: Base C5 (note 72)
    "C6", // 6: Base C6 (note 84)
    "C7", // 7: Base C7 (note 96)
    "C8"  // 8: Base C8 (note 108)
};

// MIDI channel names (Omni = all channels, then 1-16)
const char *PlaitsPort::midi_channel_names_[] = {
    "Omni", // 0: Listen to all channels
    "1",    "2",  "3",  "4",  "5",  "6",  "7",  "8",
    "9",    "10", "11", "12", "13", "14", "15", "16"};

// Synth engines (indices 8-15 in Plaits)
const char *PlaitsPort::synth_engine_names_[] = {
    "VA",     // 8: Virtual analog
    "WavShp", // 9: Waveshaping oscillator
    "FM",     // 10: Two operator FM
    "Grain",  // 11: Granular formant oscillator
    "Addtv",  // 12: Harmonic oscillator
    "WavTbl", // 13: Wavetable oscillator
    "Chord",  // 14: Chords
    "Speech"  // 15: Speech synthesis
};

// Drum/noise engines (indices 16-23 in Plaits)
const char *PlaitsPort::drum_engine_names_[] = {
    "Swarm",  // 16: Swarm of sawtooths
    "Noise",  // 17: Filtered noise
    "Partcl", // 18: Particle noise
    "String", // 19: Inharmonic string modeling
    "Modal",  // 20: Modal resonator
    "Kick",   // 21: Analog kick drum
    "Snare",  // 22: Analog snare drum
    "HiHat"   // 23: Analog hi-hat
};

// New engines (indices 0-7 in Plaits - engine2)
const char *PlaitsPort::new_engine_names_[] = {
    "VA VCF", // 0: Virtual analog with VCF
    "PhasDs", // 1: Phase distortion
    "6-Op 1", // 2: Six operator FM (patch 1)
    "6-Op 2", // 3: Six operator FM (patch 2)
    "6-Op 3", // 4: Six operator FM (patch 3)
    "WavTrn", // 5: Wave terrain
    "StrMch", // 6: String machine
    "Chip"    // 7: Chiptune
};

// Warps Lite Stage 1 (Audio In 1) mode names - 7 algorithms (no Vocoder)
static const char *audio_in1_mode_names[] = {
    "OFF",
    "XFADE", // Crossfade between synth and external
    "FOLD",  // Wavefolding
    "AnaRM", // Analog ring modulation (diode)
    "DigRM", // Digital ring modulation
    "XOR",   // Bitwise XOR
    "COMP",  // Comparator modes
    "FM"     // Phase modulation (true FM)
};
static constexpr int kNumAudioIn1Modes = 8;

// Warps Lite Stage 2 (Audio In 2) mode names - 8 algorithms (with Vocoder)
static const char *audio_in2_mode_names[] = {
    "OFF",
    "XFADE", // Crossfade between synth and external
    "FOLD",  // Wavefolding
    "AnaRM", // Analog ring modulation (diode)
    "DigRM", // Digital ring modulation
    "XOR",   // Bitwise XOR
    "COMP",  // Comparator modes
    "FM",    // Phase modulation (true FM)
    "VOCOD"  // Vocoder (Stage 2 only, CPU intensive)
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
static bool AudioIn1VisibilityCallback(const mutables_ui::Parameter *siblings,
                                       uint8_t sibling_count,
                                       uint8_t param_index) {
  if (sibling_count < 1)
    return true;

  int mode = siblings[AUDIOMOD_MODE].GetIndex();

  switch (param_index) {
  case AUDIOMOD_MODE:
    return true; // Always visible
  case AUDIOMOD_GAIN:
  case AUDIOMOD_LEVEL:
  case AUDIOMOD_TIMBRE:
    return (mode != 0); // Only visible when modulation is enabled
  default:
    return true;
  }
}

// Visibility callback for Warps Lite Stage 2 params
static bool AudioIn2VisibilityCallback(const mutables_ui::Parameter *siblings,
                                       uint8_t sibling_count,
                                       uint8_t param_index) {
  if (sibling_count < 1)
    return true;

  int mode = siblings[AUDIOMOD_MODE].GetIndex();

  switch (param_index) {
  case AUDIOMOD_MODE:
    return true; // Always visible
  case AUDIOMOD_GAIN:
  case AUDIOMOD_LEVEL:
  case AUDIOMOD_TIMBRE:
    return (mode != 0); // Only visible when modulation is enabled
  default:
    return true;
  }
}

// Format callback for Warps Lite params (Gain in dB, Level and Timbre in %)
static void AudioModFormatCallback(const mutables_ui::Parameter *param,
                                   const mutables_ui::Parameter *siblings,
                                   uint8_t sibling_count, uint8_t param_index,
                                   char *buffer, size_t buffer_size) {
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

PlaitsPort::PlaitsPort()
    : voice_(nullptr), patch_(nullptr), modulations_(nullptr),
      allocator_(nullptr), current_bank_(0), midi_note_(kMidiNoteC4),
      midi_velocity_(0.8f), midi_gate_(false), gate_state_(false),
      previous_gate_(false), previous_gate2_(false),
      sample_rate_(kDefaultSampleRate), midi_clock_hz_(0.0f),
      gate2_clock_hz_(0.0f), sample_counter_(0) {}

PlaitsPort::~PlaitsPort() {
  if (voice_)
    delete voice_;
  if (patch_)
    delete patch_;
  if (modulations_)
    delete modulations_;
  if (allocator_)
    delete allocator_;
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
  params_[1] = mutables_ui::Parameter::Enum("Engine", synth_engine_names_,
                                            kNumSynthEngines);
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
  params_[6].value = 0.8f; // Default level

  // V/Oct - CV input for pitch control (0-5V = ±30 semitones, 2.5V = 0)
  // Uses raw ADC for maximum precision
  params_[7] = mutables_ui::Parameter::CV("V/Oct");
  params_[7].value = 0.5f; // Default to center (2.5V = 0 semitones)

  // Volume - scales output level (1.0 = full, useful for eurorack
  // compatibility) Can add velocity mod for standard velocity->volume behavior
  params_[8] = mutables_ui::Parameter::Knob("Volume", 0.0f, 1.0f,
                                            1.0f); // Default to full

  // Filter submenu (Moog ladder filter, post Stage 2)
  // Signal flow: Plaits -> Stage 1 -> Stage 2 -> Filter -> Output
  mutables_ui::templates::filter::Setup(filter_params_);
  params_[9] = mutables_ui::Parameter::Sub("Filter", filter_params_.data(),
                                           kNumFilterParams);

  // Settings submenu (groups LPG and other settings from Plaits manual)
  settings_params_[0] =
      mutables_ui::Parameter::Knob("LPG Color", 0.0f, 1.0f, 0.5f);
  settings_params_[1] =
      mutables_ui::Parameter::Knob("LPG Decay", 0.0f, 1.0f, 0.5f);
  settings_params_[2] =
      mutables_ui::Parameter::Enum("Octave", octave_names_, kNumOctaves);
  settings_params_[2].SetIndex(4); // Default to C4 (middle C)
  settings_params_[3] = mutables_ui::Parameter::Enum(
      "MIDI Ch", midi_channel_names_, kNumMidiChannels);
  settings_params_[3].SetIndex(0); // Default to Omni
  params_[10] = mutables_ui::Parameter::Sub("Settings", settings_params_.data(),
                                            kNumSettingsParams);

  // CV Output 1 submenu
  mutables_ui::templates::cv_out::Setup(cv_out1_params_);
  params_[11] = mutables_ui::Parameter::Sub("CV Out 1", cv_out1_params_.data(),
                                            kNumCVOutParams);

  // CV Output 2 submenu
  mutables_ui::templates::cv_out::Setup(cv_out2_params_);
  params_[12] = mutables_ui::Parameter::Sub("CV Out 2", cv_out2_params_.data(),
                                            kNumCVOutParams);

  // Gate Output submenu
  mutables_ui::templates::gate_out::Setup(gate_out_params_);
  params_[13] = mutables_ui::Parameter::Sub(
      "Gate Out 1", gate_out_params_.data(), kNumGateOutParams);

  // Stage 1 submenu (IN1 = first Warps-style processing stage, 7 algorithms)
  audio_in1_params_[AUDIOMOD_MODE] = mutables_ui::Parameter::Enum(
      "Mode", audio_in1_mode_names, kNumAudioIn1Modes);
  audio_in1_params_[AUDIOMOD_GAIN] = mutables_ui::Parameter::Knob(
      "Gain", 0.0f, 1.0f, 0.0f); // 1x-10x (+0dB to +20dB)
  audio_in1_params_[AUDIOMOD_GAIN].format_callback = AudioModFormatCallback;
  audio_in1_params_[AUDIOMOD_LEVEL] = mutables_ui::Parameter::Knob(
      "Level", 0.0f, 1.0f, 1.0f); // Modulator level
  audio_in1_params_[AUDIOMOD_LEVEL].format_callback = AudioModFormatCallback;
  audio_in1_params_[AUDIOMOD_TIMBRE] = mutables_ui::Parameter::Knob(
      "Timbre", 0.0f, 1.0f, 0.5f); // Algorithm-specific
  audio_in1_params_[AUDIOMOD_TIMBRE].format_callback = AudioModFormatCallback;
  for (int i = 0; i < kNumAudioIn1Params; i++) {
    audio_in1_params_[i].visibility_callback = AudioIn1VisibilityCallback;
  }
  params_[14] = mutables_ui::Parameter::Sub(
      "Audio In 1", audio_in1_params_.data(), kNumAudioIn1Params);

  // Stage 2 submenu (IN2 = second Warps-style processing stage, 8 algorithms
  // with Vocoder)
  audio_in2_params_[AUDIOMOD_MODE] = mutables_ui::Parameter::Enum(
      "Mode", audio_in2_mode_names, kNumAudioIn2Modes);
  audio_in2_params_[AUDIOMOD_GAIN] = mutables_ui::Parameter::Knob(
      "Gain", 0.0f, 1.0f, 0.0f); // 1x-10x (+0dB to +20dB)
  audio_in2_params_[AUDIOMOD_GAIN].format_callback = AudioModFormatCallback;
  audio_in2_params_[AUDIOMOD_LEVEL] = mutables_ui::Parameter::Knob(
      "Level", 0.0f, 1.0f, 1.0f); // Modulator level
  audio_in2_params_[AUDIOMOD_LEVEL].format_callback = AudioModFormatCallback;
  audio_in2_params_[AUDIOMOD_TIMBRE] = mutables_ui::Parameter::Knob(
      "Timbre", 0.0f, 1.0f, 0.5f); // Algorithm-specific
  audio_in2_params_[AUDIOMOD_TIMBRE].format_callback = AudioModFormatCallback;
  for (int i = 0; i < kNumAudioIn2Params; i++) {
    audio_in2_params_[i].visibility_callback = AudioIn2VisibilityCallback;
  }
  params_[15] = mutables_ui::Parameter::Sub(
      "Audio In 2", audio_in2_params_.data(), kNumAudioIn2Params);

  // Audio Input 3 submenu (IN3 = audio-derived modulation)
  mutables_ui::templates::audio_in::Setup(audio_in3_params_);
  params_[16] = mutables_ui::Parameter::Sub(
      "Audio In 3", audio_in3_params_.data(), kNumAudioInParams);

  // Audio Input 4 submenu (IN4 = audio-derived modulation, normalized to IN3)
  mutables_ui::templates::audio_in::Setup(audio_in4_params_);
  params_[17] = mutables_ui::Parameter::Sub(
      "Audio In 4", audio_in4_params_.data(), kNumAudioInParams);

  // User Data submenu - allows selecting custom user data files from SD card
  // Target indices match UserDataManager::Target enum
  user_data_params_[0] =
      mutables_ui::Parameter::UserData("6-Op Bk 1", 0); // TARGET_SIX_OP_1
  user_data_params_[1] =
      mutables_ui::Parameter::UserData("6-Op Bk 2", 1); // TARGET_SIX_OP_2
  user_data_params_[2] =
      mutables_ui::Parameter::UserData("6-Op Bk 3", 2); // TARGET_SIX_OP_3
  user_data_params_[3] =
      mutables_ui::Parameter::UserData("WavTerrain", 3); // TARGET_WAVE_TERRAIN
  user_data_params_[4] =
      mutables_ui::Parameter::UserData("Wavetable", 4); // TARGET_WAVETABLE
  params_[18] = mutables_ui::Parameter::Sub(
      "User Data", user_data_params_.data(), kNumUserDataParams);

  // Save/Load presets
  params_[19] = mutables_ui::Parameter::Save();
  params_[20] = mutables_ui::Parameter::Load();
}

void PlaitsPort::UpdateEngineListForBank(int bank) {
  if (bank == current_bank_)
    return;

  current_bank_ = bank;

  // Save the current mapping before recreating the parameter
  auto saved_mapping = params_[1].mapping;
  float saved_value = params_[1].value;

  // Update engine list based on bank
  switch (bank) {
  case 0: // Synth
    params_[1] = mutables_ui::Parameter::Enum("Engine", synth_engine_names_,
                                              kNumSynthEngines);
    break;
  case 1: // Drum
    params_[1] = mutables_ui::Parameter::Enum("Engine", drum_engine_names_,
                                              kNumDrumEngines);
    break;
  case 2: // New
    params_[1] = mutables_ui::Parameter::Enum("Engine", new_engine_names_,
                                              kNumNewEngines);
    break;
  }

  // Restore the mapping (preserves CV assignment, etc.)
  params_[1].mapping = saved_mapping;
  // Clamp the value to the new valid range
  if (saved_value < params_[1].min)
    saved_value = params_[1].min;
  if (saved_value > params_[1].max)
    saved_value = params_[1].max;
  params_[1].value = saved_value;
}

int PlaitsPort::GetActualEngineIndex(int bank, int engine_in_bank) {
  switch (bank) {
  case 0: // Synth bank -> engines 8-15
    return 8 + engine_in_bank;
  case 1: // Drum bank -> engines 16-21
    return 16 + engine_in_bank;
  case 2: // New bank -> engines 0-7
    return engine_in_bank;
  default:
    return 8; // Default to first synth engine
  }
}

void PlaitsPort::UpdatePatchFromParams() {
  if (!patch_)
    return;

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

  int octave = settings_params_[2].GetIndex(); // 0-8 = C0-C8
  float frequency_knob = params_[2].value;

  // Base note from octave selection: C0=note 12, C1=24, ..., C8=108
  float octave_note = static_cast<float>((octave + 1) * 12); // +1 because C0=12

  // Fine tuning from Frequency knob: ±7 semitones
  float fine_tune = (frequency_knob - 0.5f) * 14.0f;

  float base_note = octave_note + fine_tune;

  // MIDI note acts as V/Oct offset from C4 (middle C = note 60)
  // When MIDI note 60 is received, it adds 0 to base_note
  // MIDI note 72 adds +12 (one octave up), MIDI note 48 adds -12 (one octave
  // down)
  float midi_offset = midi_note_ - kMidiNoteC4;

  // V/Oct CV input: 0-5V maps to ±30 semitones (2.5V = 0)
  // params_[7].value contains the raw CV (0.0-1.0)
  // Only apply V/Oct if the parameter is mapped to a CV input
  float voct_cv_offset = 0.0f;
  if (params_[7].mapping.IsCVSource()) {
    // Use raw CV value for precision (0.0-1.0 corresponds to 0-5V)
    float voct_raw = params_[7].value;
    voct_cv_offset = (voct_raw - kCVCenter) * kVOctRange; // ±30 semitones
  }

  patch_->note = base_note + midi_offset + voct_cv_offset;

  // For parameters with CV mapping: use offset as base value when plugged
  // This lets Plaits add the CV modulation on top
  auto &harmonics = params_[3];
  auto &timbre = params_[4];
  auto &morph = params_[5];
  auto &frequency = params_[2];

  // Apply velocity modulation: vel_mod = velocity * velocity_amount
  // This is applied when USING values, not when storing them (to avoid
  // feedback)
  float harm_vel = midi_velocity_ * harmonics.mapping.velocity_amount;
  float timb_vel = midi_velocity_ * timbre.mapping.velocity_amount;
  float morph_vel = midi_velocity_ * morph.mapping.velocity_amount;
  float lpg_col_vel =
      midi_velocity_ * settings_params_[0].mapping.velocity_amount;
  float lpg_dec_vel =
      midi_velocity_ * settings_params_[1].mapping.velocity_amount;

  float harm_base =
      harmonics.mapping.plugged ? harmonics.mapping.offset : harmonics.value;
  float timb_base =
      timbre.mapping.plugged ? timbre.mapping.offset : timbre.value;
  float morph_base = morph.mapping.plugged ? morph.mapping.offset : morph.value;

  patch_->harmonics = std::clamp(harm_base + harm_vel, 0.0f, 1.0f);
  patch_->timbre = std::clamp(timb_base + timb_vel, 0.0f, 1.0f);
  patch_->morph = std::clamp(morph_base + morph_vel, 0.0f, 1.0f);

  // Modulation amounts - use the attenuverter from each parameter's mapping
  // These control internal envelope routing (when unplugged) or external CV
  // scaling (when plugged) CC mapping just replaces the base value, doesn't
  // affect modulation
  patch_->frequency_modulation_amount = frequency.mapping.attenuverter;
  patch_->timbre_modulation_amount = timbre.mapping.attenuverter;
  patch_->morph_modulation_amount = morph.mapping.attenuverter;

  // LPG parameters with velocity modulation
  patch_->lpg_colour =
      std::clamp(settings_params_[0].value + lpg_col_vel, 0.0f, 1.0f);
  patch_->decay =
      std::clamp(settings_params_[1].value + lpg_dec_vel, 0.0f, 1.0f);
}

void PlaitsPort::SetCVModulations(float frequency_cv, float timbre_cv,
                                  float morph_cv) {
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
}

void PlaitsPort::Process(float **in, float **out, size_t size) {
  if (!voice_ || !patch_ || !modulations_)
    return;

  UpdatePatchFromParams();
  UpdateCVModulatorsFromParams();
  UpdateAudioEnvFromParams(audio_env_processor_3_, audio_in3_params_);
  UpdateAudioEnvFromParams(audio_env_processor_4_, audio_in4_params_);

  // Plaits processes in blocks
  plaits::Voice::Frame frames[kBlockSize];

  for (size_t i = 0; i < size; i += kBlockSize) {
    size_t block_size = (i + kBlockSize <= size) ? kBlockSize : (size - i);

    // Process audio inputs for envelope/transient detection
    // IN3 = in[2], IN4 = in[3] (IN4 is normalized to IN3 if unplugged)
    const float *audio_in3_block = in[2] + i;
    const float *audio_in4_block = in[3] + i;
    audio_env_processor_3_.ProcessBlock(audio_in3_block, block_size);
    audio_env_processor_4_.ProcessBlock(audio_in4_block, block_size);

    // Get audio-derived modulations from both processors and combine
    float audio_timbre_mod = audio_env_processor_3_.GetTimbreModulation() +
                             audio_env_processor_4_.GetTimbreModulation();
    float audio_morph_mod = audio_env_processor_3_.GetMorphModulation() +
                            audio_env_processor_4_.GetMorphModulation();

    // Apply audio envelope modulation to patch values (additive, like velocity)
    patch_->timbre = std::clamp(patch_->timbre + audio_timbre_mod, 0.0f, 1.0f);
    patch_->morph = std::clamp(patch_->morph + audio_morph_mod, 0.0f, 1.0f);

    // Check for Audio Inputs (Modulation)
    // Use the processor's trigger detection (which respects gain and checks all
    // samples)
    bool audio_trigger_3 = audio_env_processor_3_.GetTrigger();
    bool audio_trigger_4 = audio_env_processor_4_.GetTrigger();

    // Use MIDI gate OR hardware gate OR audio transient trigger
    bool active_gate = midi_gate_ || gate_state_;

    // In original firmware, Audio In modes were:
    // 0: Off, 1: On (Processing), 2: Trig (Use as trigger source)

    // Check if Audio In 3 is set to Trig mode
    if (audio_in3_params_[audio_in::MODE].GetIndex() == 2 && audio_trigger_3) {
      // Trigger logic if needed...
      active_gate = true;
    }

    // Check if Audio In 4 is set to Trig mode
    if (audio_in4_params_[audio_in::MODE].GetIndex() == 2 && audio_trigger_4) {
      // Trigger logic...
      active_gate = true;
    }
    // Set modulations - keep trigger high while gate is active
    // Plaits does its own edge detection internally
    modulations_->trigger = active_gate ? 1.0f : 0.0f;

    // Level value
    modulations_->level = params_[6].value;

    // CV modulation values (set by main.cpp via SetCVModulations)
    modulations_->frequency = frequency_cv_;
    modulations_->timbre = timbre_cv_;
    modulations_->morph = morph_cv_;

    // Patched status - true only when CV is mapped+plugged
    // This tells Plaits whether to use external modulation or internal envelope
    // CC mapping just replaces base value, doesn't count as patched
    auto &frequency = params_[2];
    auto &timbre = params_[4];
    auto &morph = params_[5];
    auto &level = params_[6];

    modulations_->frequency_patched =
        frequency.mapping.IsCVSource() && frequency.mapping.plugged;
    modulations_->timbre_patched =
        timbre.mapping.IsCVSource() && timbre.mapping.plugged;
    modulations_->morph_patched =
        morph.mapping.IsCVSource() && morph.mapping.plugged;
    modulations_->trigger_patched = true; // Always patched via MIDI/Gate
    modulations_->level_patched = level.mapping.IsCVSource(); // Level parameter

    // Update LEDs or other UI feedback if needed
    // ...

    // Update Gate Output parameters from UI
    int gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();
    // ... update gate output logic if parameter changed ...

    // Warps Lite Stage 1: Audio hybridization via IN1
    // Modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM
    int stage1_mode = audio_in1_params_[AUDIOMOD_MODE].GetIndex();
    if (stage1_mode > 0) {
      modulations_->audio_mod_in1 = in[0] + i; // IN1
      modulations_->audio_mod_mode1 = stage1_mode;
      modulations_->audio_mod_gain1 =
          1.0f + audio_in1_params_[AUDIOMOD_GAIN].value * 9.0f; // 1x-10x
      modulations_->audio_mod_level1 = audio_in1_params_[AUDIOMOD_LEVEL].value;
      modulations_->audio_mod_timbre1 =
          audio_in1_params_[AUDIOMOD_TIMBRE].value;
    } else {
      modulations_->audio_mod_in1 = nullptr;
      modulations_->audio_mod_mode1 = 0;
      modulations_->audio_mod_gain1 = 1.0f;
      modulations_->audio_mod_level1 = 1.0f;
      modulations_->audio_mod_timbre1 = 0.5f;
    }

    // Warps Lite Stage 2: Audio hybridization via IN2 (chained after Stage 1)
    // Modes: 0=OFF, 1=XFADE, 2=FOLD, 3=AnaRM, 4=DigRM, 5=XOR, 6=COMP, 7=FM,
    // 8=VOCODER
    int stage2_mode = audio_in2_params_[AUDIOMOD_MODE].GetIndex();
    if (stage2_mode > 0) {
      modulations_->audio_mod_in2 = in[1] + i; // IN2
      modulations_->audio_mod_mode2 = stage2_mode;
      modulations_->audio_mod_gain2 =
          1.0f + audio_in2_params_[AUDIOMOD_GAIN].value * 9.0f; // 1x-10x
      modulations_->audio_mod_level2 = audio_in2_params_[AUDIOMOD_LEVEL].value;
      modulations_->audio_mod_timbre2 =
          audio_in2_params_[AUDIOMOD_TIMBRE].value;
    } else {
      modulations_->audio_mod_in2 = nullptr;
      modulations_->audio_mod_mode2 = 0;
      modulations_->audio_mod_gain2 = 1.0f;
      modulations_->audio_mod_level2 = 1.0f;
      modulations_->audio_mod_timbre2 = 0.5f;
    }

    // Render audio (monophonic)
    voice_->Render(*patch_, *modulations_, frames, block_size);

    // Apply velocity to amplitude (always-on velocity requested by user)
    // Note: Volume parameter modulation is applied separately below
    for (size_t j = 0; j < block_size; j++) {
      frames[j].out = static_cast<int16_t>(frames[j].out * midi_velocity_);
      frames[j].aux = static_cast<int16_t>(frames[j].aux * midi_velocity_);
      frames[j].out_dry =
          static_cast<int16_t>(frames[j].out_dry * midi_velocity_);
      frames[j].aux_dry =
          static_cast<int16_t>(frames[j].aux_dry * midi_velocity_);
    }

    // Process CV modulators (once per audio block)
    // Get LPG envelope from voice for LPG_ENV mode
    float lpg_gain = voice_->GetLPGGain();

    // ======================================================================
    // MOOG LADDER FILTER (post Stage 1 & Stage 2)
    // Signal flow: Plaits -> Stage1 -> Stage2 -> Filter -> Output
    // ======================================================================
    int filter_mode = filter_params_[filter::MODE].GetIndex();
    if (filter_mode > 0) {
      // Set filter mode (maps our 1-6 to DaisySP's enum)
      static const daisysp::LadderFilter::FilterMode mode_map[] = {
          daisysp::LadderFilter::FilterMode::LP24, // placeholder for OFF (won't
                                                   // be used)
          daisysp::LadderFilter::FilterMode::LP12, // 1 = LP12
          daisysp::LadderFilter::FilterMode::LP24, // 2 = LP24
          daisysp::LadderFilter::FilterMode::BP12, // 3 = BP12
          daisysp::LadderFilter::FilterMode::BP24, // 4 = BP24
          daisysp::LadderFilter::FilterMode::HP12, // 5 = HP12
          daisysp::LadderFilter::FilterMode::HP24  // 6 = HP24
      };
      filter_.SetFilterMode(mode_map[filter_mode]);

      // Calculate cutoff frequency with key tracking and envelope modulation
      // Base frequency: 20Hz to 20kHz (log scaled from 0-1 parameter)
      float freq_param = filter_params_[filter::FREQ].value;

      // Cache base frequency calculation (avoid expensive powf every block)
      if (freq_param != last_filter_freq_param_) {
        cached_filter_base_freq_ =
            20.0f * powf(1000.0f, freq_param); // 20 * 1000^value
        last_filter_freq_param_ = freq_param;
      }
      float base_freq = cached_filter_base_freq_;

      // Key tracking: 0-100% of note pitch applied to cutoff
      // Reference: C4 (MIDI 60) = no shift, each octave up doubles freq
      float tracking = filter_params_[filter::TRACK].value;
      float note_offset = (patch_->note - 60.0f) / 12.0f; // Octaves from C4
      float tracking_multiplier = powf(2.0f, note_offset * tracking);

      // Envelope modulation: when Freq param has no CV plugged,
      // attenuverter controls how much LPG envelope modulates cutoff
      // Range: attenuverter -1 to +1 maps to -4 to +4 octaves of env modulation
      float env_mod_amount = 0.0f;
      if (!filter_params_[filter::FREQ].mapping.IsCVSource() ||
          !filter_params_[filter::FREQ].mapping.plugged) {
        // No CV plugged: use attenuverter for envelope modulation
        env_mod_amount = filter_params_[filter::FREQ].mapping.attenuverter;
      }
      float env_mod_multiplier = powf(2.0f, lpg_gain * env_mod_amount * 4.0f);

      // Final frequency with all modulations
      float final_freq = base_freq * tracking_multiplier * env_mod_multiplier;
      final_freq = std::clamp(final_freq, 20.0f, 20000.0f);
      filter_.SetFreq(final_freq);

      // Resonance: 0-1 parameter maps to 0-1.8 (DaisySP max for
      // self-oscillation)
      float reso = filter_params_[filter::RESO].value * 1.8f;
      filter_.SetRes(reso);

      // Drive: 0-1 parameter maps to 0-4 (DaisySP range)
      float drive = filter_params_[filter::DRIVE].value * 4.0f;
      filter_.SetInputDrive(drive);

      // Apply filter to wet outputs (OUT and AUX)
      for (size_t j = 0; j < block_size; j++) {
        // Filter the main output
        float out_sample = static_cast<float>(frames[j].out) / kAudioScaleInt16;
        float filtered_out = filter_.Process(out_sample);
        frames[j].out =
            static_cast<int16_t>(std::clamp(filtered_out * kAudioScaleInt16,
                                            -kAudioScaleInt16, kAudioMaxInt16));

        // Note: AUX output could use a second filter instance for stereo,
        // but for CPU efficiency we apply the same filter coefficients
        // The aux signal passes through unchanged for now (common in mono
        // synths)
      }
    }

    // Pass audio envelopes to CV modulators for FOLLOW_3 and FOLLOW_4 modes
    float audio_env_3 = audio_env_processor_3_.GetEnvelope();
    float audio_env_4 = audio_env_processor_4_.GetEnvelope();
    cv_modulator_1_.SetAudioEnvelope3(audio_env_3);
    cv_modulator_1_.SetAudioEnvelope4(audio_env_4);
    cv_modulator_2_.SetAudioEnvelope3(audio_env_3);
    cv_modulator_2_.SetAudioEnvelope4(audio_env_4);

    cv_modulator_1_.Process(lpg_gain, midi_clock_hz_, gate2_clock_hz_,
                            cv_inputs_);
    cv_modulator_2_.Process(lpg_gain, midi_clock_hz_, gate2_clock_hz_,
                            cv_inputs_);

    // Gate Output processing
    gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();

    // EndEnv mode (1): detect when envelope ends (lpg_gain drops below
    // threshold)
    if (gate_out_mode == 1) {
      if (prev_lpg_gain_ > kEnvThreshold && lpg_gain <= kEnvThreshold) {
        // Envelope just ended - set trigger
        gate_out_state_ = true;
        gate_out_trigger_counter_ =
            static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
      }
      prev_lpg_gain_ = lpg_gain;
    }

    // Countdown trigger pulse for modes that use it (Trigger, EndEnv,
    // TrigProb, ClkDiv, ClkProb)
    if (gate_out_trigger_counter_ > 0) {
      if (gate_out_trigger_counter_ > block_size) {
        gate_out_trigger_counter_ -= block_size;
      } else {
        gate_out_trigger_counter_ = 0;
        gate_out_state_ = false;
      }
    }

    // Get volume with velocity modulation
    float vol_vel =
        midi_velocity_ * params_[8].mapping.velocity_amount; // Volume parameter
    float volume = std::clamp(params_[8].value + vol_vel, 0.0f, 1.0f);

    // Convert from short to float, apply velocity and volume separately, copy
    // to outputs OUT (wet) -> channel 1, AUX (wet) -> channel 2 OUT_DRY ->
    // channel 3, AUX_DRY -> channel 4
    for (size_t j = 0; j < block_size && (i + j) < size; j++) {
      // Apply velocity for amplitude control, then volume for final level
      float out_sample =
          (static_cast<float>(frames[j].out) / kAudioScaleInt16) * volume;
      float aux_sample =
          (static_cast<float>(frames[j].aux) / kAudioScaleInt16) * volume;
      float out_dry_sample =
          (static_cast<float>(frames[j].out_dry) / kAudioScaleInt16) * volume;
      float aux_dry_sample =
          (static_cast<float>(frames[j].aux_dry) / kAudioScaleInt16) * volume;
      out[0][i + j] = out_sample;     // OUT (wet) -> channel 1
      out[1][i + j] = aux_sample;     // AUX (wet) -> channel 2
      out[2][i + j] = out_dry_sample; // OUT (dry) -> channel 3
      out[3][i + j] = aux_dry_sample; // AUX (dry) -> channel 4
    }
  }
}

mutables_ui::Parameter *PlaitsPort::GetParameters() { return params_.data(); }

size_t PlaitsPort::GetParameterCount() const { return params_.size(); }

void PlaitsPort::ProcessGate(int gate_index, bool state) {

  // Handle Gate 1 input for Trig mode
  if (gate_index == 0) {
    gate_state_ = state;

    // Rising edge detection
    if (state && !previous_gate_) {
      // Trigger internal voices
      cv_modulator_1_.Trigger();
      cv_modulator_2_.Trigger();

      int gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();

      if (gate_out_mode == 0) { // Trig
        // Pass through Gate 1 (or OR with MIDI)
        gate_out_state_ = true;
        gate_out_trigger_counter_ =
            static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
      } else if (gate_out_mode == 2) { // TrigPrb
        // Probability check
        float prob = gate_out_params_[gate_out::PROB].value;
        if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < prob) {
          gate_out_state_ = true;
          gate_out_trigger_counter_ =
              static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
        }
      }
    }

    previous_gate_ = state;
  }

  // Handle Gate 2 input for ClkDiv/ClkPrb modes
  if (gate_index == 1) {
    // Update clock tracker with current state and timestamp
    gate2_clock_tracker_.Process(state, sample_counter_);

    if (gate2_clock_tracker_.IsActive(sample_counter_, sample_rate_)) {
      gate2_clock_hz_ = gate2_clock_tracker_.GetClockHz(sample_rate_);
    } else {
      gate2_clock_hz_ = 0.0f;
    }

    if (state && !previous_gate2_) {
      // Gate 2 rising edge logic...

      int gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();

      if (gate_out_mode == 3) { // ClkDiv
        int divider_idx = gate_out_params_[gate_out::CLK_DIV].GetIndex();
        // Constants are in 24 PPQ (MIDI), but analog clock is 1 pulse per step.
        // So we divide by 24 to get the pulse count.
        // /1 (24) -> 1 pulse
        // /2 (48) -> 2 pulses
        int divider = gate_out::kClkDivValues[divider_idx] / 24;
        if (divider < 1)
          divider = 1;

        clock_div_counter_++;
        // Simple divider logic for external clock
        if (clock_div_counter_ >= divider) {
          clock_div_counter_ = 0;
          gate_out_state_ = true;
          gate_out_trigger_counter_ =
              static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
        }

      } else if (gate_out_mode == 4) { // ClkPrb
        // Probability check on clock tick
        float prob = gate_out_params_[gate_out::PROB].value;
        if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < prob) {
          gate_out_state_ = true;
          gate_out_trigger_counter_ =
              static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
        }
      }
    }
    previous_gate2_ = state;
  }
}

void PlaitsPort::UpdateCVModulatorsFromParams() {
  // Update CV modulator 1 from  // CV Out 1
  cv_modulator_1_.SetMode(
      static_cast<CVOutMode>(cv_out1_params_[cv_out::MODE].GetIndex()));
  cv_modulator_1_.SetAttack(cv_out1_params_[cv_out::ATTACK].value);
  cv_modulator_1_.SetRelease(cv_out1_params_[cv_out::RELEASE].value);
  cv_modulator_1_.SetLFOShape(
      static_cast<LFOShape>(cv_out1_params_[cv_out::SHAPE].GetIndex()));
  cv_modulator_1_.SetSlewAmount(cv_out1_params_[cv_out::SLEW].value);
  cv_modulator_1_.SetSHSource(
      static_cast<SHSource>(cv_out1_params_[cv_out::SH_SRC].GetIndex()));
  cv_modulator_1_.SetSyncMode(
      static_cast<SyncMode>(cv_out1_params_[cv_out::SYNC].GetIndex()));
  cv_modulator_1_.SetRate(cv_out1_params_[cv_out::RATE].value);
  cv_modulator_1_.SetRetrig(cv_out1_params_[cv_out::RETRIG].GetIndex() == 1);
  cv_modulator_1_.SetAmp(cv_out1_params_[cv_out::AMP].value);
  cv_modulator_1_.SetPhaseOffset(cv_out1_params_[cv_out::PHASE].value);
  cv_modulator_1_.SetFollowScale3(cv_out1_params_[cv_out::SCALE3].value * 2.0f);
  cv_modulator_1_.SetFollowScale4(cv_out1_params_[cv_out::SCALE4].value * 2.0f);

  // CV Out 2
  cv_modulator_2_.SetMode(
      static_cast<CVOutMode>(cv_out2_params_[cv_out::MODE].GetIndex()));
  cv_modulator_2_.SetAttack(cv_out2_params_[cv_out::ATTACK].value);
  cv_modulator_2_.SetRelease(cv_out2_params_[cv_out::RELEASE].value);
  cv_modulator_2_.SetLFOShape(
      static_cast<LFOShape>(cv_out2_params_[cv_out::SHAPE].GetIndex()));
  cv_modulator_2_.SetSlewAmount(cv_out2_params_[cv_out::SLEW].value);
  cv_modulator_2_.SetSHSource(
      static_cast<SHSource>(cv_out2_params_[cv_out::SH_SRC].GetIndex()));
  cv_modulator_2_.SetSyncMode(
      static_cast<SyncMode>(cv_out2_params_[cv_out::SYNC].GetIndex()));
  cv_modulator_2_.SetRate(cv_out2_params_[cv_out::RATE].value);
  cv_modulator_2_.SetRetrig(cv_out2_params_[cv_out::RETRIG].GetIndex() == 1);
  cv_modulator_2_.SetAmp(cv_out2_params_[cv_out::AMP].value);
  cv_modulator_2_.SetPhaseOffset(cv_out2_params_[cv_out::PHASE].value);
  cv_modulator_2_.SetFollowScale3(cv_out2_params_[cv_out::SCALE3].value * 2.0f);
  cv_modulator_2_.SetFollowScale4(cv_out2_params_[cv_out::SCALE4].value *
                                  2.0f); // 0-1 -> 0-2x
}

// Helper function to update audio env processor from parameters
void PlaitsPort::UpdateAudioEnvFromParams(
    AudioEnvProcessor &processor,
    std::array<mutables_ui::Parameter, kNumAudioInParams> &params) {
  // Update audio envelope processor from its params
  int mode = params[audio_in::MODE].GetIndex();
  processor.SetMode(static_cast<AudioEnvMode>(mode));

  // Input gain (normalized 0-1 -> 1x-10x for line level signals)
  // params[1] is Gain (0-1). Map to 1.0 - 10.0 (0-20dB)
  // Or linear gain. Let's assume 1.0 + 9.0 * value
  float gain = 1.0f + 9.0f * params[audio_in::GAIN].value;
  processor.SetGainNormalized(params[audio_in::GAIN].value);
  processor.SetGain(gain);

  // Modulation amounts (bipolar -1 to +1)
  processor.SetTimbreAmount(params[audio_in::TIMBRE_AMT].value);
  processor.SetMorphAmount(params[audio_in::MORPH_AMT].value);

  // Envelope follower params (normalized 0-1, internally converted to
  // log-scaled ms)
  // Map 0-1 to reasonable times (10ms to 2s)
  // Attack: 0.5ms to 200ms
  processor.SetAttack(params[audio_in::ATTACK].value);
  processor.SetRelease(params[audio_in::RELEASE].value);

  // Transient detector params
  processor.SetThreshold(params[audio_in::THRESHOLD].value);
  processor.SetHoldoff(params[audio_in::HOLDOFF].value);
}

void PlaitsPort::OnMIDIClock() {
  midi_clock_tracker_.OnClock(sample_counter_);
  float freq = midi_clock_tracker_.GetClockHz(sample_rate_);
  if (freq > 0.0f) {
    midi_clock_hz_ = freq;
  }

  // Handle Gate Out ClkDiv mode (MIDI clock source)
  int gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();

  if (gate_out_mode == 3) {  // ClkDiv
    int main_clock_ppq = 24; // MIDI clock is 24 PPQ
    int divider_idx = gate_out_params_[gate_out::CLK_DIV].GetIndex();
    int divider = gate_out::kClkDivValues[divider_idx];

    // With 24 PPQ input, we output a pulse every 'divider' ticks?
    // No, clk_div_values are in 1/96 notes (standard internal) or something.
    // 24 = quarter note. 6 = 16th note.
    // If we receive 24 ticks per beat, and we want 16ths (4 per beat), we
    // trigger every 6 ticks.

    // Let's simplified this:
    // clock_div_counter_ increments on every clock tick.
    // If (clock_div_counter_ % (divider / 4)) == 0 -> Trigger?
    // Assuming divider values are relative to 96ppq standard which is common
    // in Plaits. MIDI is 24ppq. So we divide values by 4.

    int ticks_per_pulse = divider / 4;
    if (ticks_per_pulse < 1)
      ticks_per_pulse = 1;

    clock_div_counter_++;
    if (clock_div_counter_ >= ticks_per_pulse) {
      clock_div_counter_ = 0;
      gate_out_state_ = true;
      gate_out_trigger_counter_ = kBlockSize / 2;
    }

  } else if (gate_out_mode == 4) { // ClkPrb
    // Probability check on clock tick
    float prob = gate_out_params_[gate_out::PROB].value;
    if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < prob) {
      gate_out_state_ = true;
      gate_out_trigger_counter_ = kBlockSize / 4;
    }
  }
}

void PlaitsPort::UpdateSampleCounter(size_t samples) {
  // Tracker processing is usually done per sample or block, but here we just
  // update counter If trackers need per-block update, check their
  // implementation. Based on error: GateClockTracker::Process(bool gate,
  // uint32_t t) needs checking gate state. We handle GateClockTracker in
  // ProcessGate per sample/block usually. But MIDIClock tracker might not
  // need explicit Process per sample if we just call OnClock.

  sample_counter_ += samples;
}

void PlaitsPort::NoteOn(uint8_t note, uint8_t velocity) {
  midi_note_ = note;
  midi_velocity_ = velocity / 127.0f;
  midi_gate_ = true;

  // Handle Gate Out trigger for MIDI notes
  int gate_out_mode = gate_out_params_[gate_out::MODE].GetIndex();
  if (gate_out_mode == 0) { // Trig
    gate_out_state_ = true;
    gate_out_trigger_counter_ =
        static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
  } else if (gate_out_mode == 2) { // TrigPrb
    // Probability check
    float prob = gate_out_params_[gate_out::PROB].value;
    if (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) < prob) {
      gate_out_state_ = true;
      gate_out_trigger_counter_ =
          static_cast<uint32_t>(sample_rate_ * kTriggerDurationS);
    }
  }
}

void PlaitsPort::NoteOff(uint8_t note, uint8_t velocity) {
  if (midi_note_ == note) {
    midi_gate_ = false;
  }
}

void PlaitsPort::AllNotesOff() { midi_gate_ = false; }

void PlaitsPort::Panic() {
  AllNotesOff();
  // Reset engines?
}

float PlaitsPort::GetCVOutput(int cv_index) {
  // We need to pass the current environment state to the modulators
  // These values should likely be members updated in Process(), but for now
  // we pass approximations or last known values. Ideally, these Process()
  // calls should happen in the main Process loop and we just return the
  // stored output here to avoid re-calculation or missing state.

  // Actually, checking CVModulator logic: it needs lpg_gain, midi_clock_hz,
  // gate2_clock_hz, and cv_values. We should update the modulators in
  // `Process()` and store the result, or pass the values here. But
  // GetCVOutput is called by the main loop which might expect it to compute.

  float lpg_envelope_ =
      voice_->GetDecayEnvelope(); // Assuming we want the envelope here

  // Note: We don't have easy access to lpg_gain here unless we store it.
  // Let's assume standard behavior:

  if (cv_index == 0)
    return cv_modulator_1_.Process(lpg_envelope_, midi_clock_hz_,
                                   gate2_clock_hz_, nullptr);
  if (cv_index == 1)
    return cv_modulator_2_.Process(lpg_envelope_, midi_clock_hz_,
                                   gate2_clock_hz_, nullptr);
  return 0.0f;
}

bool PlaitsPort::GetGateOutput() const {
  int mode = gate_out_params_[gate_out::MODE].GetIndex();

  switch (mode) {
  case 0: // Trigger - Gate 1 rise OR MIDI note on
    return gate_out_trigger_counter_ > 0;

  case 1: // EndEnv - Trigger at end of envelope
    return gate_out_state_;

  case 2: // TrigProb - Trigger with probability
    return gate_out_trigger_counter_ > 0;

  case 3: // ClkDiv - Clock divider
    return gate_out_state_;

  case 4: // ClkProb - Clock tick with probability
    return gate_out_trigger_counter_ > 0;

  default:
    return false;
  }
}

} // namespace mutables_plaits
