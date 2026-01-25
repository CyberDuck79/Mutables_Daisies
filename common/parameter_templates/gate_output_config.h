#pragma once

#include "../parameter.h"
#include <array>
#include <cstdio>

namespace mutables_ui {
namespace templates {
namespace gate_out {

static const char *kModeNames[] = {"Trig", "EndEnv", "TrigPrb", "ClkDiv",
                                   "ClkPrb"};
static constexpr int kNumModes = 5;

static const char *kClkDivNames[] = {"/1", "/2",  "/3",  "/4",  "/6",
                                     "/8", "/12", "/16", "/24", "/32"};
static constexpr int kNumClkDivs = 10;
static const int kClkDivValues[] = {24,  48,  72,  96,  144,
                                    192, 288, 384, 576, 768};

enum ParamIndex { MODE = 0, CLK_DIV = 1, PROB = 2 };

static constexpr int kNumParams = 3;

static bool VisibilityCallback(const Parameter *siblings, uint8_t sibling_count,
                               uint8_t param_index) {
  if (sibling_count < 1)
    return true;
  int mode = siblings[MODE].GetIndex();

  switch (param_index) {
  case MODE:
    return true;
  case CLK_DIV:
    return (mode == 3);
  case PROB:
    return (mode == 2 || mode == 4);
  default:
    return true;
  }
}

static void FormatCallback(const Parameter *param, const Parameter *, uint8_t,
                           uint8_t, char *buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "%d%%",
           static_cast<int>(param->value * 100.0f));
}

template <typename ParamArray> void Setup(ParamArray &params) {
  static_assert(std::tuple_size<ParamArray>::value >= kNumParams,
                "Parameter array too small");

  params[MODE] = Parameter::Enum("Mode", kModeNames, kNumModes);
  params[CLK_DIV] = Parameter::Enum("ClkDiv", kClkDivNames, kNumClkDivs);
  params[PROB] = Parameter::Knob("Prob", 0.0f, 1.0f, 0.5f);

  params[PROB].format_callback = FormatCallback;

  for (int i = 0; i < kNumParams; i++) {
    params[i].visibility_callback = VisibilityCallback;
  }
}

} // namespace gate_out
} // namespace templates
} // namespace mutables_ui
