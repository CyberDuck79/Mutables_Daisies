#pragma once

#include "../parameter.h"
#include "../utils/format_utils.h"
#include <array>
#include <cstdio>

namespace mutables_ui {
namespace templates {
namespace audio_in {

// Audio Input modes
static const char *kModeNames[] = {"OFF", "ENV", "TRIG"};
static constexpr int kNumModes = 3;

// Parameter indices
enum ParamIndex {
  MODE = 0,
  GAIN = 1,
  TIMBRE_AMT = 2,
  MORPH_AMT = 3,
  ATTACK = 4,
  RELEASE = 5,
  THRESHOLD = 6,
  HOLDOFF = 7
};

static constexpr int kNumParams = 8;

// Visibility callback
static bool VisibilityCallback(const Parameter *siblings, uint8_t sibling_count,
                               uint8_t param_index) {
  if (sibling_count < 1)
    return true;

  int mode = siblings[MODE].GetIndex();

  switch (param_index) {
  case MODE:
    return true;
  case GAIN:
    return (mode != 0);
  case TIMBRE_AMT:
  case MORPH_AMT:
  case ATTACK:
  case RELEASE:
    return (mode == 1); // ENV
  case THRESHOLD:
  case HOLDOFF:
    return (mode == 2); // TRIG
  default:
    return true;
  }
}

// Helpers for formatted output
static void FormatBipolarPercent(char *buffer, size_t size, float value) {
  format::FormatBipolarPercent(buffer, size, value);
}

// Format callback
static void FormatCallback(const Parameter *param, const Parameter *siblings,
                           uint8_t sibling_count, uint8_t param_index,
                           char *buffer, size_t buffer_size) {
  (void)siblings;
  (void)sibling_count;

  float value = param->value;

  switch (param_index) {
  case GAIN:
    format::FormatGainDB(buffer, buffer_size, value);
    break;
  case ATTACK:
    format::FormatAttackTime(buffer, buffer_size, value);
    break;
  case RELEASE:
    format::FormatReleaseTime(buffer, buffer_size, value);
    break;
  case THRESHOLD:
    format::FormatPercent(buffer, buffer_size, value);
    break;
  case HOLDOFF: {
    float ms = 20.0f + value * 180.0f;
    snprintf(buffer, buffer_size, "%dms", static_cast<int>(ms + 0.5f));
  } break;
  default:
    snprintf(buffer, buffer_size, "%d%%",
             static_cast<int>(value * 100.0f + 0.5f));
    break;
  }
}

// Bipolar format callback wrapper
static void BipolarFormatCallback(const Parameter *param, const Parameter *,
                                  uint8_t, uint8_t, char *buffer, size_t size) {
  FormatBipolarPercent(buffer, size, param->value);
}

// Setup function
template <typename ParamArray> void Setup(ParamArray &params) {
  static_assert(std::tuple_size<ParamArray>::value >= kNumParams,
                "Parameter array too small");

  params[MODE] = Parameter::Enum("Mode", kModeNames, kNumModes);
  params[GAIN] = Parameter::Knob("Gain", 0.0f, 1.0f, 0.0f);
  params[TIMBRE_AMT] = Parameter::Knob("Tim. Mod", -1.0f, 1.0f, 0.0f);
  params[MORPH_AMT] = Parameter::Knob("Mrph Mod", -1.0f, 1.0f, 0.0f);
  params[ATTACK] = Parameter::Knob("Attack", 0.0f, 1.0f, 0.1f);
  params[RELEASE] = Parameter::Knob("Release", 0.0f, 1.0f, 0.3f);
  params[THRESHOLD] = Parameter::Knob("Thresh", 0.0f, 1.0f, 0.3f);
  params[HOLDOFF] = Parameter::Knob("Holdoff", 0.0f, 1.0f, 0.2f);

  params[GAIN].format_callback = FormatCallback;
  params[TIMBRE_AMT].format_callback = BipolarFormatCallback;
  params[MORPH_AMT].format_callback = BipolarFormatCallback;
  params[ATTACK].format_callback = FormatCallback;
  params[RELEASE].format_callback = FormatCallback;
  params[THRESHOLD].format_callback = FormatCallback;
  params[HOLDOFF].format_callback = FormatCallback;

  for (int i = 0; i < kNumParams; i++) {
    params[i].visibility_callback = VisibilityCallback;
  }
}

} // namespace audio_in
} // namespace templates
} // namespace mutables_ui
