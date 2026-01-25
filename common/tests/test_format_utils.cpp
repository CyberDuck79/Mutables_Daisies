#include "../utils/format_utils.h"
#include "test_framework.h"

using namespace mutables_ui::format;

// ============================================================================
// Time Formatting Tests
// ============================================================================

TEST(FormatUtils, FormatAttackTimeMin) {
  char buffer[16];
  FormatAttackTime(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "0.5ms");
}

TEST(FormatUtils, FormatAttackTimeMid) {
  char buffer[16];
  // 0.5 * 400^0.5 = 0.5 * 20 = 10ms
  FormatAttackTime(buffer, sizeof(buffer), 0.5f);
  EXPECT_STREQ(buffer, "10ms");
}

TEST(FormatUtils, FormatAttackTimeMax) {
  char buffer[16];
  // 0.5 * 400^1.0 = 200ms
  FormatAttackTime(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "200ms");
}

TEST(FormatUtils, FormatReleaseTimeMin) {
  char buffer[16];
  FormatReleaseTime(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "5.0ms");
}

TEST(FormatUtils, FormatReleaseTimeMax) {
  char buffer[16];
  // 5 * 400 = 2000ms = 2.0s
  FormatReleaseTime(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "2.0s");
}

// ============================================================================
// Gain/Level Formatting Tests
// ============================================================================

TEST(FormatUtils, FormatGainDBZero) {
  char buffer[16];
  FormatGainDB(buffer, sizeof(buffer), 0.0f);
  // 1x = 0dB
  EXPECT_STREQ(buffer, "+0dB");
}

TEST(FormatUtils, FormatGainDBMax) {
  char buffer[16];
  FormatGainDB(buffer, sizeof(buffer), 1.0f);
  // 10x = 20dB
  EXPECT_STREQ(buffer, "+20dB");
}

// ============================================================================
// Percentage Formatting Tests
// ============================================================================

TEST(FormatUtils, FormatPercentZero) {
  char buffer[16];
  FormatPercent(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "0%");
}

TEST(FormatUtils, FormatPercentHalf) {
  char buffer[16];
  FormatPercent(buffer, sizeof(buffer), 0.5f);
  EXPECT_STREQ(buffer, "50%");
}

TEST(FormatUtils, FormatPercentFull) {
  char buffer[16];
  FormatPercent(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "100%");
}

TEST(FormatUtils, FormatBipolarPercentNegative) {
  char buffer[16];
  FormatBipolarPercent(buffer, sizeof(buffer), -1.0f);
  EXPECT_STREQ(buffer, "-100%");
}

TEST(FormatUtils, FormatBipolarPercentPositive) {
  char buffer[16];
  FormatBipolarPercent(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "+100%");
}

TEST(FormatUtils, FormatBipolarPercentZero) {
  char buffer[16];
  FormatBipolarPercent(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "+0%");
}

// ============================================================================
// Frequency Formatting Tests
// ============================================================================

TEST(FormatUtils, FormatLFORateMin) {
  char buffer[16];
  FormatLFORate(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "0.10Hz");
}

TEST(FormatUtils, FormatLFORateMax) {
  char buffer[16];
  // 0.1 * 200 = 20Hz
  FormatLFORate(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "20Hz");
}

// ============================================================================
// Miscellaneous Formatting Tests
// ============================================================================

TEST(FormatUtils, FormatDegreesZero) {
  char buffer[16];
  FormatDegrees(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "0deg");
}

TEST(FormatUtils, FormatDegreesFull) {
  char buffer[16];
  FormatDegrees(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "360deg");
}

TEST(FormatUtils, FormatMultiplierZero) {
  char buffer[16];
  FormatMultiplier(buffer, sizeof(buffer), 0.0f);
  EXPECT_STREQ(buffer, "0.0x");
}

TEST(FormatUtils, FormatMultiplierMax) {
  char buffer[16];
  FormatMultiplier(buffer, sizeof(buffer), 1.0f);
  EXPECT_STREQ(buffer, "2.0x");
}
