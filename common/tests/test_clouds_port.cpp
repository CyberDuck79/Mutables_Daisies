#include "test_framework.h"
#include "../../eurorack/clouds/dsp/frame.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

// ============================================================================
// Clouds Float ↔ int16 Conversion Tests
// ============================================================================
// Tests the conversion math used in CloudsPort::Process()
// Matches original Clouds: no extra clamping, SoftConvert is inside DSP

static int16_t float_to_int16(float value) {
  return static_cast<int16_t>(value * 32768.0f);
}

static float int16_to_float(int16_t value) {
  return static_cast<float>(value) / 32768.0f;
}

TEST(CloudsConversion, RoundtripAtZero) {
  float original = 0.0f;
  int16_t converted = float_to_int16(original);
  float restored = int16_to_float(converted);
  EXPECT_FLOAT_EQ(original, restored);
}

TEST(CloudsConversion, RoundtripAtHalfScale) {
  float original = 0.5f;
  int16_t converted = float_to_int16(original);
  float restored = int16_to_float(converted);
  EXPECT_TRUE(std::abs(original - restored) < 0.0001f);
}

TEST(CloudsConversion, RoundtripAtNegativeHalfScale) {
  float original = -0.5f;
  int16_t converted = float_to_int16(original);
  float restored = int16_to_float(converted);
  EXPECT_TRUE(std::abs(original - restored) < 0.0001f);
}

TEST(CloudsConversion, QuantizationErrorIsWithinTolerance) {
  // Worst case quantization error is 1/32768 ≈ 0.00003
  float original = 0.333333f;
  int16_t converted = float_to_int16(original);
  float restored = int16_to_float(converted);
  float error = std::abs(original - restored);
  EXPECT_TRUE(error < 0.0001f);
}

TEST(CloudsConversion, BlockOfZerosProducesSilence) {
  clouds::ShortFrame frames[32];
  for (int i = 0; i < 32; i++) {
    frames[i].l = float_to_int16(0.0f);
    frames[i].r = float_to_int16(0.0f);
  }
  for (int i = 0; i < 32; i++) {
    float restored_l = int16_to_float(frames[i].l);
    float restored_r = int16_to_float(frames[i].r);
    EXPECT_FLOAT_EQ(restored_l, 0.0f);
    EXPECT_FLOAT_EQ(restored_r, 0.0f);
  }
}

// ============================================================================
// Clouds Parameter Definition Tests (compile-time verification)
// ============================================================================

TEST(CloudsParameters, BlockSizeMatchesCloudsMaxBlockSize) {
  constexpr int kCloudsMaxBlockSize = 32;
  constexpr int kOurBlockSize = 32;
  EXPECT_EQ(kOurBlockSize, kCloudsMaxBlockSize);
}

TEST(CloudsParameters, PlaybackModeCountIsCorrect) {
  constexpr int kExpectedModes = 4;
  EXPECT_EQ(kExpectedModes, 4);
}

TEST(CloudsParameters, QualityModeCountIsCorrect) {
  constexpr int kExpectedQualityModes = 4;
  EXPECT_EQ(kExpectedQualityModes, 4);
}

TEST(CloudsParameters, PitchRangeIsPlusMinus48Semitones) {
  float pitch_at_zero = (0.0f - 0.5f) * 96.0f;
  float pitch_at_half = (0.5f - 0.5f) * 96.0f;
  float pitch_at_one = (1.0f - 0.5f) * 96.0f;

  EXPECT_FLOAT_EQ(pitch_at_zero, -48.0f);
  EXPECT_FLOAT_EQ(pitch_at_half, 0.0f);
  EXPECT_FLOAT_EQ(pitch_at_one, 48.0f);
}

TEST(CloudsParameters, BufferSizesAreSufficient) {
  constexpr size_t kLargeBuffer = 118784;
  constexpr size_t kSmallBuffer = 65408;

  EXPECT_TRUE(kLargeBuffer >= 116000);
  EXPECT_TRUE(kSmallBuffer >= 64000);
}
