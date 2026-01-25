#include "../cv_mapping_processor.h"
#include "test_framework.h"

using namespace mutables_ui;

// Helper to print MappingSource
std::ostream &operator<<(std::ostream &os, const MappingSource &s) {
  os << static_cast<int>(s);
  return os;
}

// ============================================================================
// CalculateMappedValue Tests
// ============================================================================

TEST(CVMappingProcessor, CalculateMappedValueUnplugged) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = false;

  // Direct CV pass-through (ignores base value)
  float result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.75f);
  EXPECT_FLOAT_EQ(result, 0.75f);
}

TEST(CVMappingProcessor, CalculateMappedValueNoSource) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::NONE;

  // Should return base value
  float result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.75f);
  EXPECT_FLOAT_EQ(result, 0.5f);
}

TEST(CVMappingProcessor, CalculateMappedValuePluggedPositive) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = true;
  p.mapping.offset = 0.5f;
  p.mapping.attenuverter = 0.5f; // +50%

  // Signal = CV (0.9) - Offset (0.5) = 0.4
  // Result = Offset (0.5) + Signal (0.4) * Atten (0.5) = 0.5 + 0.2 = 0.7
  float result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.9f);
  EXPECT_FLOAT_EQ(result, 0.7f);
}

TEST(CVMappingProcessor, CalculateMappedValuePluggedNegative) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = true;
  p.mapping.offset = 0.5f;
  p.mapping.attenuverter = -0.5f; // -50%

  // Signal = CV (0.9) - Offset (0.5) = 0.4
  // Result = Offset (0.5) + Signal (0.4) * Atten (-0.5) = 0.5 - 0.2 = 0.3
  float result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.9f);
  EXPECT_FLOAT_EQ(result, 0.3f);
}

TEST(CVMappingProcessor, CalculateMappedValueClamps) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = true;
  p.mapping.offset = 0.5f;
  p.mapping.attenuverter = 2.0f; // Extreme gain

  // Signal = 0.8 - 0.5 = 0.3
  // Result = 0.5 + 0.3 * 2.0 = 1.1 -> Clamped to 1.0
  float result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.8f);
  EXPECT_FLOAT_EQ(result, 1.0f);

  // Signal = 0.2 - 0.5 = -0.3
  // Result = 0.5 + (-0.3) * 2.0 = -0.1 -> Clamped to 0.0
  result = CVMappingProcessor::CalculateMappedValue(p, 0.5f, 0.2f);
  EXPECT_FLOAT_EQ(result, 0.0f);
}

// ============================================================================
// CalculateEnumFromCV Tests
// ============================================================================

TEST(CVMappingProcessor, CalculateEnumIndexQuantizes) {
  const char *labels[] = {"A", "B", "C", "D"};
  Parameter p = Parameter::Enum("Test", labels, 4);
  p.mapping.source = MappingSource::CV1;
  p.mapping.attenuverter = 1.0f;
  p.mapping.plugged = false; // Center based

  // Center based on 0.5
  // Scale = 0.5 + (CV - 0.5) * 1.0 = CV

  // 0.0 -> Index 0
  EXPECT_EQ(CVMappingProcessor::CalculateEnumFromCV(p, 0.0f), 0);

  // 0.25 -> Index 1 (Range 0.25-0.5)
  EXPECT_EQ(CVMappingProcessor::CalculateEnumFromCV(p, 0.26f), 1);

  // 0.99 -> Index 3
  EXPECT_EQ(CVMappingProcessor::CalculateEnumFromCV(p, 0.99f), 3);
}

TEST(CVMappingProcessor, CalculateEnumIndexWithPlugged) {
  const char *labels[] = {"A", "B", "C", "D"};
  Parameter p = Parameter::Enum("Test", labels, 4);
  p.mapping.source = MappingSource::CV1;
  p.mapping.attenuverter = 1.0f;
  p.mapping.plugged = true;
  p.mapping.offset = 0.2f;

  // Scale = 0.5 + (CV - Offset) * Atten

  // input = 0.2 (offset) -> 0.5 -> Index 2 (Start of upper half)
  // 0.5 * 4 = 2.0 -> Index 2
  EXPECT_EQ(CVMappingProcessor::CalculateEnumFromCV(p, 0.2f), 2);

  // input = 0.45
  // Signal = 0.25. Scaled = 0.5 + 0.25 = 0.75 -> Index 3
  EXPECT_EQ(CVMappingProcessor::CalculateEnumFromCV(p, 0.45f), 3);
}

// ============================================================================
// CycleMappingSource Tests
// ============================================================================

TEST(CVMappingProcessor, CycleMappingSourceForKnob) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::NONE;

  // None -> CV1
  CVMappingProcessor::CycleMappingSource(p, 1);
  EXPECT_EQ(p.mapping.source, MappingSource::CV1);

  // Cycle up to CV4
  p.mapping.source = MappingSource::CV4;
  CVMappingProcessor::CycleMappingSource(p, 1);
  // CV4 -> GATE1 (Skip) -> GATE2 (Skip) -> CC (Maybe?)
  // Logic: do { current++ } while (current == GATE1 || current == GATE2)
  // So CV4 -> GATE1 ... skips ... -> CC?
  // Let's verify enums: CV4=4, GATE1=5, GATE2=6, CC=7

  EXPECT_EQ(p.mapping.source, MappingSource::CC);

  // CC -> None
  CVMappingProcessor::CycleMappingSource(p, 1);
  EXPECT_EQ(p.mapping.source, MappingSource::NONE);
}

TEST(CVMappingProcessor, CycleMappingSourceForEnum) {
  const char *labels[] = {"A", "B"};
  Parameter p = Parameter::Enum("Test", labels, 2);
  p.mapping.source = MappingSource::CV4;

  // Should behave like KNOB and skip Gates (based on current implementation)
  CVMappingProcessor::CycleMappingSource(p, 1);
  EXPECT_EQ(p.mapping.source, MappingSource::CC);
}

TEST(CVMappingProcessor, CycleMappingSourceReverse) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::NONE;

  // None -> CC (Reverse)
  CVMappingProcessor::CycleMappingSource(p, -1);
  EXPECT_EQ(p.mapping.source, MappingSource::CC);

  // CC -> CV4 (Reverse, skipping Gates)
  CVMappingProcessor::CycleMappingSource(p, -1);
  // CC(7) -> GATE2(6) -> Skips -> GATE1(5) -> Skips -> CV4(4)
  EXPECT_EQ(p.mapping.source, MappingSource::CV4);
}
