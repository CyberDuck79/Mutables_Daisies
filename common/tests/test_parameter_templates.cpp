#include "../parameter_templates/audio_input_config.h"
#include "../parameter_templates/cv_output_config.h"
#include "../parameter_templates/filter_config.h"
#include "../parameter_templates/gate_output_config.h"
#include "test_framework.h"
#include <array>
#include <cstring>

using namespace mutables_ui;

// CV Output Tests
TEST(TemplateCVOut, SetupInitializesParams) {
  std::array<Parameter, templates::cv_out::kNumParams> params;
  templates::cv_out::Setup(params);

  EXPECT_STREQ(params[templates::cv_out::MODE].name, "Mode");
  EXPECT_STREQ(params[templates::cv_out::ATTACK].name, "Attack");
  EXPECT_FLOAT_EQ(params[templates::cv_out::AMP].value, 1.0f);
}

TEST(TemplateCVOut, VisibilityBasedOnMode) {
  std::array<Parameter, templates::cv_out::kNumParams> params;
  templates::cv_out::Setup(params);

  // Set Mode to AD (index 1)
  params[templates::cv_out::MODE].SetIndex(1);

  // Check callback directly
  EXPECT_TRUE(params[0].visibility_callback(params.data(), params.size(),
                                            templates::cv_out::ATTACK));
  EXPECT_FALSE(params[0].visibility_callback(params.data(), params.size(),
                                             templates::cv_out::SHAPE));
}

TEST(TemplateCVOut, Formatting) {
  std::array<Parameter, templates::cv_out::kNumParams> params;
  templates::cv_out::Setup(params);

  char buffer[32];

  // Test Rate formatting (free)
  params[templates::cv_out::RATE].value = 0.5f;
  // Need to reset sync to 0 for free Hz check, but default IS free
  params[templates::cv_out::RATE].format_callback(
      &params[templates::cv_out::RATE], params.data(), params.size(),
      templates::cv_out::RATE, buffer, 32);
  // Should be valid Hz string
  EXPECT_TRUE(strlen(buffer) > 0);

  // Test Rate formatting (synced)
  params[templates::cv_out::SYNC].SetIndex(1); // MIDI
  params[templates::cv_out::RATE].format_callback(
      &params[templates::cv_out::RATE], params.data(), params.size(),
      templates::cv_out::RATE, buffer, 32);
  // Should be ratio string e.g. "1/4"
  EXPECT_TRUE(strchr(buffer, '/') != nullptr || strcmp(buffer, "1") == 0 ||
              strcmp(buffer, "2") == 0 || strcmp(buffer, "4") == 0 ||
              strcmp(buffer, "8") == 0);
}

// Audio Input Tests
TEST(TemplateAudioIn, SetupHelper) {
  std::array<Parameter, templates::audio_in::kNumParams> params;
  templates::audio_in::Setup(params);

  EXPECT_STREQ(params[templates::audio_in::GAIN].name, "Gain");
  EXPECT_FLOAT_EQ(params[templates::audio_in::GAIN].min, 0.0f);
}

TEST(TemplateAudioIn, Visibility) {
  std::array<Parameter, templates::audio_in::kNumParams> params;
  templates::audio_in::Setup(params);

  // Mode OFF
  params[templates::audio_in::MODE].SetIndex(0);
  EXPECT_FALSE(params[0].visibility_callback(params.data(), params.size(),
                                             templates::audio_in::GAIN));

  // Mode ENV
  params[templates::audio_in::MODE].SetIndex(1);
  EXPECT_TRUE(params[0].visibility_callback(params.data(), params.size(),
                                            templates::audio_in::ATTACK));
  EXPECT_FALSE(params[0].visibility_callback(params.data(), params.size(),
                                             templates::audio_in::THRESHOLD));
}

// Filter Tests
TEST(TemplateFilter, FormattingFreq) {
  std::array<Parameter, templates::filter::kNumParams> params;
  templates::filter::Setup(params);

  char buffer[32];
  // Min freq (20Hz)
  params[templates::filter::FREQ].value = 0.0f;
  params[templates::filter::FREQ].format_callback(
      &params[templates::filter::FREQ], nullptr, 0, templates::filter::FREQ,
      buffer, 32);
  EXPECT_STREQ(buffer, "20Hz");

  // Max freq
  params[templates::filter::FREQ].value = 1.0f;
  params[templates::filter::FREQ].format_callback(
      &params[templates::filter::FREQ], nullptr, 0, templates::filter::FREQ,
      buffer, 32);
  EXPECT_STREQ(buffer, "20.0kHz");
}

// Gate Out Tests
TEST(TemplateGateOut, Visibility) {
  std::array<Parameter, templates::gate_out::kNumParams> params;
  templates::gate_out::Setup(params);

  // Mode Trig (0)
  params[templates::gate_out::MODE].SetIndex(0);
  EXPECT_FALSE(params[0].visibility_callback(params.data(), params.size(),
                                             templates::gate_out::PROB));

  // Mode TrigPrb (2)
  params[templates::gate_out::MODE].SetIndex(2);
  EXPECT_TRUE(params[0].visibility_callback(params.data(), params.size(),
                                            templates::gate_out::PROB));
}
