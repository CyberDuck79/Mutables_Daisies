#pragma once

#include "../parameter.h"
#include <array>
#include <cmath>
#include <cstdio>

namespace mutables_ui {
namespace templates {
namespace filter {

static const char *kModeNames[] = {"OFF",  "LP12", "LP24", "BP12",
                                   "BP24", "HP12", "HP24"};
static constexpr int kNumModes = 7;

enum ParamIndex { MODE = 0, FREQ = 1, RESO = 2, DRIVE = 3, TRACK = 4 };

static constexpr int kNumParams = 5;

static bool VisibilityCallback(const Parameter *siblings, uint8_t sibling_count,
                               uint8_t param_index) {
  if (sibling_count < 1)
    return true;
  int mode = siblings[MODE].GetIndex();

  switch (param_index) {
  case MODE:
    return true;
  default:
    return (mode != 0);
  }
}

static void FormatCallback(const Parameter *param, const Parameter *, uint8_t,
                           uint8_t param_index, char *buffer,
                           size_t buffer_size) {
  float value = param->value;

  switch (param_index) {
  case FREQ: {
    float freq = 20.0f * powf(1000.0f, value);
    if (freq < 100.0f) {
      snprintf(buffer, buffer_size, "%dHz", static_cast<int>(freq + 0.5f));
    } else if (freq < 1000.0f) {
      snprintf(buffer, buffer_size, "%dHz", static_cast<int>(freq + 0.5f));
    } else {
      int khz_int = static_cast<int>(freq / 100.0f + 0.5f);
      snprintf(buffer, buffer_size, "%d.%dkHz", khz_int / 10, khz_int % 10);
    }
  } break;
  default:
    snprintf(buffer, buffer_size, "%d%%",
             static_cast<int>(value * 100.0f + 0.5f));
    break;
  }
}

template <typename ParamArray> void Setup(ParamArray &params) {
  static_assert(std::tuple_size<ParamArray>::value >= kNumParams,
                "Parameter array too small");

  params[MODE] = Parameter::Enum("Mode", kModeNames, kNumModes);
  params[FREQ] = Parameter::Knob("Freq", 0.0f, 1.0f, 0.7f);
  params[RESO] = Parameter::Knob("Reso", 0.0f, 1.0f, 0.0f);
  params[DRIVE] = Parameter::Knob("Drive", 0.0f, 1.0f, 0.25f);
  params[TRACK] = Parameter::Knob("Track", 0.0f, 1.0f, 0.0f);

  for (int i = 0; i < kNumParams; i++) {
    params[i].visibility_callback = VisibilityCallback;
    if (i > MODE)
      params[i].format_callback = FormatCallback;
  }
}

} // namespace filter
} // namespace templates
} // namespace mutables_ui
