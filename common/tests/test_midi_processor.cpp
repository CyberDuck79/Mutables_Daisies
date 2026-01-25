#include "../midi_processor.h"
#include "test_framework.h"

using namespace mutables_ui;

// Fixture for MIDIProcessor tests
class MIDIProcessorTest {
public:
  MIDIProcessor processor;

  MIDIProcessorTest() {
    processor.Init(0); // Default to Omni
  }
};

TEST(MIDIProcessor, InitDefaultsToOmni) {
  MIDIProcessor p;
  p.Init();
  EXPECT_EQ(p.GetChannel(), 0);
}

TEST(MIDIProcessor, InitClearsCCValues) {
  MIDIProcessor p;
  p.SetCC(1, 127);
  p.Init();
  EXPECT_FLOAT_EQ(p.GetCC(1), 0.0f);
}

TEST(MIDIProcessor, SetGetChannel) {
  MIDIProcessor p;
  p.SetChannel(5);
  EXPECT_EQ(p.GetChannel(), 5);
}

TEST(MIDIProcessor, ShouldProcessOmni) {
  MIDIProcessor p;
  p.Init(0);                        // Omni
  EXPECT_TRUE(p.ShouldProcess(0));  // Ch 1
  EXPECT_TRUE(p.ShouldProcess(15)); // Ch 16
}

TEST(MIDIProcessor, ShouldProcessSpecificChannel) {
  MIDIProcessor p;
  p.Init(1);                        // Channel 1 (index 0)
  EXPECT_TRUE(p.ShouldProcess(0));  // Match
  EXPECT_FALSE(p.ShouldProcess(1)); // Mismatch

  p.SetChannel(16);                 // Channel 16 (index 15)
  EXPECT_TRUE(p.ShouldProcess(15)); // Match
  EXPECT_FALSE(p.ShouldProcess(0)); // Mismatch
}

TEST(MIDIProcessor, SetCCNormalizesValue) {
  MIDIProcessor p;
  p.SetCC(10, 0);
  EXPECT_FLOAT_EQ(p.GetCC(10), 0.0f);

  p.SetCC(10, 127);
  EXPECT_FLOAT_EQ(p.GetCC(10), 1.0f);

  p.SetCC(10, 64);
  // 64/127 approx 0.5039
  float val = p.GetCC(10);
  EXPECT_TRUE(val > 0.5f && val < 0.51f);
}

TEST(MIDIProcessor, GetCCOutOfBoundsReturnsZero) {
  MIDIProcessor p;
  EXPECT_FLOAT_EQ(p.GetCC(-1), 0.0f);
  EXPECT_FLOAT_EQ(p.GetCC(128), 0.0f);
}

TEST(MIDIProcessor, SetCCOutOfBoundsDoesNothing) {
  MIDIProcessor p;
  p.SetCC(128, 127); // Should be ignored
                     // No crash is good also
}

TEST(MIDIProcessor, GetCCValuesReturnsPointer) {
  MIDIProcessor p;
  p.SetCC(7, 127);
  const float *values = p.GetCCValues();
  EXPECT_FLOAT_EQ(values[7], 1.0f);
}

TEST(MIDIProcessor, BuildThruMessageNoteOn) {
  uint8_t buffer[3];
  size_t size = MIDIProcessor::BuildThruMessage(0x90, 5, 60, 100, buffer);

  EXPECT_EQ(size, 3);
  EXPECT_EQ(buffer[0], 0x95); // 0x90 | 0x05
  EXPECT_EQ(buffer[1], 60);
  EXPECT_EQ(buffer[2], 100);
}

TEST(MIDIProcessor, BuildThruMessageNoteOff) {
  uint8_t buffer[3];
  size_t size = MIDIProcessor::BuildThruMessage(0x80, 0, 64, 0, buffer);

  EXPECT_EQ(size, 3);
  EXPECT_EQ(buffer[0], 0x80); // 0x80 | 0x00
  EXPECT_EQ(buffer[1], 64);
  EXPECT_EQ(buffer[2], 0);
}

TEST(MIDIProcessor, BuildThruMessageControlChange) {
  uint8_t buffer[3];
  size_t size = MIDIProcessor::BuildThruMessage(0xB0, 15, 7, 127, buffer);

  EXPECT_EQ(size, 3);
  EXPECT_EQ(buffer[0], 0xBF); // 0xB0 | 0x0F
  EXPECT_EQ(buffer[1], 7);
  EXPECT_EQ(buffer[2], 127);
}

TEST(MIDIProcessor, BuildThruMessageIgnoredTypes) {
  uint8_t buffer[3];
  // Program Change (0xC0) not currently handled by Thru helper
  size_t size = MIDIProcessor::BuildThruMessage(0xC0, 0, 0, 0, buffer);
  EXPECT_EQ(size, 0);
}
