#pragma once

#include "../parameter.h"
#include "../utils/format_utils.h"
#include <array>
#include <cstdio>

namespace mutables_ui {
namespace templates {
namespace cv_out {

// CV Output mode names
static const char *kModeNames[] = {
    "LPG",    // Follows internal LPG envelope
    "AD",     // AD envelope triggered on gate
    "LFO",    // Low frequency oscillator
    "Foll.3", // Follow audio input 3 envelope
    "Foll.4"  // Follow audio input 4 envelope
};
static constexpr int kNumModes = 5;

// LFO shape names
static const char *kLFOShapeNames[] = {"Sine",   "Tri", "Saw",
                                       "Square", "S&H", "Smooth"};
static constexpr int kNumLFOShapes = 6;

// Clock sync modes
static const char *kSyncNames[] = {"Free", "MIDI", "Gate2"};
static constexpr int kNumSyncModes = 3;

// Retrig (on/off toggle)
static const char *kRetrigNames[] = {"OFF", "ON"};
static constexpr int kNumRetrigOptions = 2;

// S&H source names
static const char *kSHSourceNames[] = {"Random", "CV1", "CV2", "CV3", "CV4"};
static constexpr int kNumSHSources = 5;

// Clock ratio names
static const char *kClockRatioNames[] = {
    "1/16", "1/16T", "1/16D", "1/8",  "1/8T", "1/8D", "1/4", "1/4T",
    "1/4D", "1/2",   "1/2T",  "1/2D", "1",    "2",    "4",   "8"};
static constexpr int kNumClockRatios = 16;

// Parameter indices
enum ParamIndex {
  MODE = 0,
  ATTACK = 1,
  ENV_RELEASE = 2,
  SHAPE = 3,
  SLEW = 4,
  SH_SRC = 5,
  SYNC = 6,
  RATE = 7,
  RETRIG = 8,
  AMP = 9,
  PHASE = 10,
  SCALE3 = 11,
  SCALE4 = 12
};

static constexpr int kNumParams = 13;

// Visibility callback
static bool VisibilityCallback(const Parameter *siblings, uint8_t sibling_count,
                               uint8_t param_index) {
  if (sibling_count < 1)
    return true;

  int mode = siblings[MODE].GetIndex();
  int shape = (sibling_count > SHAPE) ? siblings[SHAPE].GetIndex() : 0;

  switch (param_index) {
  case MODE:
    return true;
  case ATTACK:
  case ENV_RELEASE:
    return (mode == 1); // AD
  case SHAPE:
    return (mode == 2); // LFO
  case SLEW:
    return (mode == 2) && (shape == 5); // LFO + Smooth
  case SH_SRC:
    return (mode == 2) && (shape == 4); // LFO + S&H
  case SYNC:
  case RATE:
  case RETRIG:
  case PHASE:
    return (mode == 2); // LFO
  case AMP:
    return true;
  case SCALE3:
    return (mode == 3); // Foll.3
  case SCALE4:
    return (mode == 4); // Foll.4
  default:
    return true;
  }
}

// Format callback
static void FormatCallback(const Parameter *param, const Parameter *siblings,
                           uint8_t sibling_count, uint8_t param_index,
                           char *buffer, size_t buffer_size) {
  if (param->type == ParamType::ENUM) {
    snprintf(buffer, buffer_size, "%.6s", param->GetEnumLabel());
    return;
  }

  float value = param->value;

  switch (param_index) {
  case ATTACK:
    format::FormatAttackTime(buffer, buffer_size, value);
    break;
  case ENV_RELEASE:
    format::FormatReleaseTime(buffer, buffer_size, value);
    break;
  case RATE:
    if (sibling_count > SYNC && siblings[SYNC].GetIndex() >= 1) {
      int ratio = static_cast<int>(value * (kNumClockRatios - 1) + 0.5f);
      if (ratio < 0)
        ratio = 0;
      if (ratio >= kNumClockRatios)
        ratio = kNumClockRatios - 1;
      snprintf(buffer, buffer_size, "%s", kClockRatioNames[ratio]);
    } else {
      format::FormatLFORate(buffer, buffer_size, value);
    }
    break;
  case AMP:
  case SLEW:
    format::FormatPercent(buffer, buffer_size, value);
    break;
  case PHASE:
    format::FormatDegrees(buffer, buffer_size, value);
    break;
  case SCALE3:
  case SCALE4:
    format::FormatMultiplier(buffer, buffer_size, value);
    break;
  default:
    format::FormatDecimal(buffer, buffer_size, value);
    break;
  }
}

// Setup function
template <typename ParamArray> void Setup(ParamArray &params) {
  static_assert(std::tuple_size<ParamArray>::value >= kNumParams,
                "Parameter array too small");

  params[MODE] = Parameter::Enum("Mode", kModeNames, kNumModes);
  params[ATTACK] = Parameter::Knob("Attack", 0.0f, 1.0f, 0.01f);
  params[ENV_RELEASE] = Parameter::Knob("Release", 0.0f, 1.0f, 0.3f);
  params[SHAPE] = Parameter::Enum("Shape", kLFOShapeNames, kNumLFOShapes);
  params[SLEW] = Parameter::Knob("Slew", 0.0f, 1.0f, 0.5f);
  params[SH_SRC] = Parameter::Enum("SH Src", kSHSourceNames, kNumSHSources);
  params[SYNC] = Parameter::Enum("Sync", kSyncNames, kNumSyncModes);
  params[RATE] = Parameter::Knob("Rate", 0.0f, 1.0f, 0.3f);
  params[RETRIG] = Parameter::Enum("Retrig", kRetrigNames, kNumRetrigOptions);
  params[AMP] = Parameter::Knob("Amp", 0.0f, 1.0f, 1.0f);
  params[PHASE] = Parameter::Knob("Phase", 0.0f, 1.0f, 0.0f);
  params[SCALE3] = Parameter::Knob("Scale3", 0.0f, 1.0f, 0.5f);
  params[SCALE4] = Parameter::Knob("Scale4", 0.0f, 1.0f, 0.5f);

  for (int i = 0; i < kNumParams; i++) {
    params[i].visibility_callback = VisibilityCallback;
    params[i].format_callback = FormatCallback;
  }
}

} // namespace cv_out
} // namespace templates
} // namespace mutables_ui
