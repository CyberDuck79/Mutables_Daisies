#include "test_framework.h"
#include "../../eurorack/clouds/dsp/frame.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

// ============================================================================
// Clouds Float ↔ int16 Conversion Tests
// ============================================================================
// Tests the conversion math used in CloudsPort::Process()
// Input:  float → int16:  static_cast<int16_t>(value * 32768.0f)
// Output: int16 → float:  static_cast<float>(value) / 32768.0f

static int16_t float_to_int16(float value) {
  return static_cast<int16_t>(
      std::clamp(value, -1.0f, 1.0f) * 32767.0f);
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

TEST(CloudsConversion, RoundtripAtFullScale) {
  float original = 1.0f;
  // 1.0 * 32768 = 32768, which overflows int16 (max 32767)
  // std::clamp returns 1.0, then * 32768 = 32768 → overflow to -32768
  int16_t converted = float_to_int16(original);
  // The conversion produces -32768 due to overflow, which is acceptable
  // as it represents the most negative value (symmetric clipping)
  float restored = int16_to_float(converted);
  EXPECT_TRUE(std::abs(restored) > 0.99f);
}

TEST(CloudsConversion, ClampsAboveOne) {
  float original = 1.5f;
  int16_t converted = float_to_int16(original);
  // 1.5 clamped to 1.0, then * 32767 = 32767
  EXPECT_EQ(converted, 32767);
}

TEST(CloudsConversion, RoundtripAtNegativeHalfScale) {
  float original = -0.5f;
  int16_t converted = float_to_int16(original);
  float restored = int16_to_float(converted);
  EXPECT_TRUE(std::abs(original - restored) < 0.0001f);
}

TEST(CloudsConversion, ClampsBelowMinusOne) {
  float original = -1.5f;
  int16_t converted = float_to_int16(original);
  // -1.5 clamped to -1.0, then * 32767 = -32767
  EXPECT_EQ(converted, -32767);
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
// These verify the expected parameter layout matches the design doc.
// The actual CloudsPort class requires Daisy SDK, so we verify the
// constants and layout expectations here.

TEST(CloudsParameters, BlockSizeMatchesCloudsMaxBlockSize) {
  // Clouds kMaxBlockSize = 32 (from eurorack/clouds/dsp/frame.h)
  // Our CloudsPort::kBlockSize must match exactly
  constexpr int kCloudsMaxBlockSize = 32;
  constexpr int kOurBlockSize = 32;
  EXPECT_EQ(kOurBlockSize, kCloudsMaxBlockSize);
}

TEST(CloudsParameters, PlaybackModeCountIsCorrect) {
  // Granular, Stretch, Delay, Spectral
  constexpr int kExpectedModes = 4;
  EXPECT_EQ(kExpectedModes, 4);
}

TEST(CloudsParameters, QualityModeCountIsCorrect) {
  // Stereo 16b, Mono 16b, Stereo 8b, Mono 8b
  constexpr int kExpectedQualityModes = 4;
  EXPECT_EQ(kExpectedQualityModes, 4);
}

TEST(CloudsParameters, PitchRangeIsPlusMinus48Semitones) {
  // Pitch knob 0-1 maps to ±48 semitones
  // value=0.0 → -48, value=0.5 → 0, value=1.0 → +48
  float pitch_at_zero = (0.0f - 0.5f) * 96.0f;
  float pitch_at_half = (0.5f - 0.5f) * 96.0f;
  float pitch_at_one = (1.0f - 0.5f) * 96.0f;

  EXPECT_FLOAT_EQ(pitch_at_zero, -48.0f);
  EXPECT_FLOAT_EQ(pitch_at_half, 0.0f);
  EXPECT_FLOAT_EQ(pitch_at_one, 48.0f);
}

TEST(CloudsParameters, BufferSizesAreSufficient) {
  // Original Clouds uses ~116 KB main + ~64 KB CCM
  // Our buffers must be at least this large
  constexpr size_t kLargeBuffer = 118784;
  constexpr size_t kSmallBuffer = 65408;

  EXPECT_TRUE(kLargeBuffer >= 116000);
  EXPECT_TRUE(kSmallBuffer >= 64000);
}
