#include "../parameter.h"
#include "test_framework.h"

using namespace mutables_ui;

// ============================================================================
// Parameter Normalization Tests
// ============================================================================

TEST(Parameter, GetNormalizedReturnsCorrectValue) {
  auto p = Parameter::Knob("Test", 0.0f, 100.0f, 50.0f);
  EXPECT_FLOAT_EQ(p.GetNormalized(), 0.5f);

  p.value = 0.0f;
  EXPECT_FLOAT_EQ(p.GetNormalized(), 0.0f);

  p.value = 100.0f;
  EXPECT_FLOAT_EQ(p.GetNormalized(), 1.0f);
}

TEST(Parameter, SetNormalizedClamps) {
  auto p = Parameter::Knob("Test", 0.0f, 100.0f);

  p.SetNormalized(1.5f);
  EXPECT_FLOAT_EQ(p.value, 100.0f);

  p.SetNormalized(-0.5f);
  EXPECT_FLOAT_EQ(p.value, 0.0f);
}

TEST(Parameter, SetNormalizedWithHysteresisFilters) {
  auto p = Parameter::Knob("Test", 0.0f, 100.0f, 50.0f); // Range = 100

  // Change < tolerance (0.5% default = 0.5 units) should be ignored
  // 0.501 * 100 = 50.1. Diff = 0.1. Range * 0.005 = 0.5. Diff < Tolerance -> No
  // change.
  bool changed = p.SetNormalizedWithHysteresis(0.501f);
  EXPECT_FALSE(changed);
  EXPECT_FLOAT_EQ(p.value, 50.0f);

  // Change > tolerance should process
  // 0.6 * 100 = 60. Diff = 10. > 0.5 -> Change.
  changed = p.SetNormalizedWithHysteresis(0.6f);
  EXPECT_TRUE(changed);
  EXPECT_FLOAT_EQ(p.value, 60.0f);
}

// ============================================================================
// Parameter Enum Tests
// ============================================================================

TEST(Parameter, EnumGetIndexRounds) {
  const char *labels[] = {"A", "B", "C"};
  auto p = Parameter::Enum("Test", labels, 3);

  // 0.0 -> 0
  p.value = 0.0f;
  EXPECT_EQ(p.GetIndex(), 0);

  // 0.9 -> 1 (rounds to nearest integer)
  p.value = 0.9f;
  EXPECT_EQ(p.GetIndex(), 1);

  // 1.1 -> 1
  p.value = 1.1f;
  EXPECT_EQ(p.GetIndex(), 1);
}

TEST(Parameter, EnumSetIndexClamps) {
  const char *labels[] = {"A", "B", "C"};
  auto p = Parameter::Enum("Test", labels, 3);

  p.SetIndex(-1);
  EXPECT_FLOAT_EQ(p.value, 0.0f);

  p.SetIndex(5);
  EXPECT_FLOAT_EQ(p.value, 2.0f);
}

TEST(Parameter, EnumGetLabelReturnsCorrect) {
  const char *labels[] = {"OptionA", "OptionB"};
  auto p = Parameter::Enum("Test", labels, 2, 1);

  EXPECT_STREQ(p.GetEnumLabel(), "OptionB");

  p.SetIndex(0);
  EXPECT_STREQ(p.GetEnumLabel(), "OptionA");
}

// ============================================================================
// Parameter Type Helper Tests
// ============================================================================

TEST(Parameter, HasSubmenuForKnob) {
  auto p = Parameter::Knob("Test");
  EXPECT_TRUE(p.HasSubmenu());
}

TEST(Parameter, HasSubmenuForSub) {
  auto p = Parameter::Sub("Test", nullptr, 0);
  EXPECT_TRUE(p.HasSubmenu());
}

TEST(Parameter, IsEditableForKnob) {
  auto p = Parameter::Knob("Test");
  EXPECT_TRUE(p.IsEditable());
}

TEST(Parameter, IsEditableForSave) {
  auto p = Parameter::Save();
  EXPECT_FALSE(p.IsEditable());
}

// ============================================================================
// Parameter Visibility Tests
// ============================================================================

TEST(Parameter, IsVisibleWithoutCallback) {
  auto p = Parameter::Knob("Test");
  EXPECT_TRUE(p.IsVisible(nullptr, 0, 0));
}

bool TestVisibility(const Parameter *siblings, uint8_t count, uint8_t index) {
  return false; // Always hide
}

TEST(Parameter, IsVisibleWithCallback) {
  auto p = Parameter::Knob("Test");
  p.visibility_callback = TestVisibility;
  EXPECT_FALSE(p.IsVisible(nullptr, 0, 0));
}
