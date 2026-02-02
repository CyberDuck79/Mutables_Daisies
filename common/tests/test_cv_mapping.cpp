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

// ============================================================================
// Processing Tests
// ============================================================================

TEST(CVMappingProcessor, RebuildCacheFindsMappedParams) {
  Parameter p1 = Parameter::Knob("P1");
  p1.mapping.source = MappingSource::CV1;

  Parameter p2 = Parameter::Knob("P2");
  p2.mapping.source = MappingSource::CC;
  p2.mapping.cc_number = 10;

  Parameter params[] = {p1, p2};

  CVMappingProcessor processor;
  processor.RebuildCache(params, 2);
  EXPECT_FALSE(processor.IsDirty());

  // Can't inspect private state easily, but ProcessCVMappings will prove it
  // worked
}

// Create a test calibration that does identity mapping (no scaling)
// Accounts for kCalibrationMargin by setting min/max to cancel it out
static SystemCalibration CreateTestCalibration() {
  SystemCalibration cal;
  // With margin = 0.002, effective_min = min - 0.002, effective_max = max + 0.002
  // For identity: we want effective range to be exactly 0.0 to 1.0
  // So min = 0.002, max = 0.998 -> effective: 0.0 to 1.0
  for (int i = 0; i < 4; i++) {
    cal.cv_inputs[i].min = kCalibrationMargin;
    cal.cv_inputs[i].max = 1.0f - kCalibrationMargin;
  }
  cal.UpdateChecksum();
  return cal;
}

// Helper to settle filter state by repeatedly calling Update
void SettleCV(CVInputBank &cv, float v0, float v1, float v2, float v3) {
  // CVInputBank doesn't have Process(); UpdateRawValues updates filter
  // immediately. To settle the 1-pole filter, we call UpdateRawValues many
  // times.
  for (int i = 0; i < 100; i++) {
    cv.UpdateRawValues(v0, v1, v2, v3);
  }
}

TEST(CVMappingProcessor, ProcessCVMappingsUpdatesValue) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = false;
  p.value = 0.0f;

  // CV Input with identity calibration for testing
  CVInputBank cv_inputs;
  static SystemCalibration test_cal = CreateTestCalibration();
  cv_inputs.SetCalibration(&test_cal);
  SettleCV(cv_inputs, 0.75f, 0, 0, 0); // CV1 = 0.75

  CVMappingProcessor processor;
  processor.RebuildCache(&p, 1);

  // Process
  processor.ProcessCVMappings(cv_inputs, 0.001f);

  // Param should be updated to 0.75
  EXPECT_FLOAT_EQ(p.value, 0.75f);
}

TEST(CVMappingProcessor, ProcessCVMappingsSubmenu) {
  Parameter child = Parameter::Knob("Child");
  child.mapping.source = MappingSource::CV2;
  child.mapping.plugged = false;

  Parameter sub = Parameter::Sub("Sub", &child, 1);

  // CV Input with identity calibration for testing
  CVInputBank cv_inputs;
  static SystemCalibration test_cal = CreateTestCalibration();
  cv_inputs.SetCalibration(&test_cal);
  SettleCV(cv_inputs, 0, 0.33f, 0, 0); // CV2 = 0.33

  CVMappingProcessor processor;
  processor.RebuildCache(&sub, 1);

  processor.ProcessCVMappings(cv_inputs, 0.001f);

  EXPECT_FLOAT_EQ(child.value, 0.33f);
}

TEST(CVMappingProcessor, ProcessCCMappingsUpdatesValue) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CC;
  p.mapping.cc_number = 7;
  p.value = 0.0f;

  CVMappingProcessor processor;
  processor.RebuildCache(&p, 1);

  float cc_values[128] = {0};
  cc_values[7] = 0.88f;

  processor.ProcessCCMappings(cc_values, 0.001f);

  EXPECT_FLOAT_EQ(p.value, 0.88f);
}
// ============================================================================
// GetModulationSignal Tests
// ============================================================================

TEST(CVMappingProcessor, GetModulationSignalPluggedReturnsSignal) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = true;
  p.mapping.offset = 0.5f;

  // CV Input with identity calibration for testing
  CVInputBank cv_inputs;
  static SystemCalibration test_cal = CreateTestCalibration();
  cv_inputs.SetCalibration(&test_cal);
  SettleCV(cv_inputs, 0.75f, 0, 0, 0); // CV1 = 0.75

  CVMappingProcessor processor;
  processor.RebuildCache(&p, 1);
  processor.ProcessCVMappings(cv_inputs, 0.001f);

  // Modulation signal = CV (0.75) - Offset (0.5) = 0.25
  float signal = processor.GetModulationSignal(p);
  EXPECT_FLOAT_EQ(signal, 0.25f);
}

TEST(CVMappingProcessor, GetModulationSignalUnpluggedReturnsZero) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV1;
  p.mapping.plugged = false;  // Not plugged
  p.mapping.offset = 0.5f;

  CVMappingProcessor processor;
  
  // Even without processing, unplugged should return 0
  float signal = processor.GetModulationSignal(p);
  EXPECT_FLOAT_EQ(signal, 0.0f);
}

TEST(CVMappingProcessor, GetModulationSignalNoSourceReturnsZero) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::NONE;
  p.mapping.plugged = true;

  CVMappingProcessor processor;
  
  float signal = processor.GetModulationSignal(p);
  EXPECT_FLOAT_EQ(signal, 0.0f);
}

TEST(CVMappingProcessor, GetModulationSignalNegative) {
  Parameter p = Parameter::Knob("Test");
  p.mapping.source = MappingSource::CV2;
  p.mapping.plugged = true;
  p.mapping.offset = 0.7f;

  CVInputBank cv_inputs;
  static SystemCalibration test_cal = CreateTestCalibration();
  cv_inputs.SetCalibration(&test_cal);
  SettleCV(cv_inputs, 0, 0.3f, 0, 0); // CV2 = 0.3

  CVMappingProcessor processor;
  processor.RebuildCache(&p, 1);
  processor.ProcessCVMappings(cv_inputs, 0.001f);

  // Modulation signal = CV (0.3) - Offset (0.7) = -0.4
  float signal = processor.GetModulationSignal(p);
  EXPECT_FLOAT_EQ(signal, -0.4f);
}

// ============================================================================
// ZeroUnmappedCVParams Tests
// ============================================================================

TEST(CVMappingProcessor, ZeroUnmappedCVParamsZeroesUnmapped) {
  Parameter params[3] = {
    Parameter::CV("Mapped"),
    Parameter::CV("Unmapped"),
    Parameter::Knob("NotCV")
  };
  
  // First CV param is mapped
  params[0].mapping.source = MappingSource::CV1;
  params[0].value = 0.5f;
  
  // Second CV param is NOT mapped
  params[1].mapping.source = MappingSource::NONE;
  params[1].value = 0.75f;  // Should be zeroed
  
  // Third param is a KNOB, should not be touched
  params[2].mapping.source = MappingSource::NONE;
  params[2].value = 0.8f;
  
  CVMappingProcessor::ZeroUnmappedCVParams(params, 3, 0.001f);
  
  // Mapped CV should keep its value
  EXPECT_FLOAT_EQ(params[0].value, 0.5f);
  
  // Unmapped CV should be zeroed
  EXPECT_FLOAT_EQ(params[1].value, 0.0f);
  
  // KNOB should be untouched
  EXPECT_FLOAT_EQ(params[2].value, 0.8f);
}

TEST(CVMappingProcessor, ZeroUnmappedCVParamsHandlesSubmenus) {
  Parameter children[2] = {
    Parameter::CV("ChildMapped"),
    Parameter::CV("ChildUnmapped")
  };
  
  children[0].mapping.source = MappingSource::CV3;
  children[0].value = 0.6f;
  
  children[1].mapping.source = MappingSource::NONE;
  children[1].value = 0.9f;  // Should be zeroed
  
  Parameter parent = Parameter::Sub("Parent", children, 2);
  
  CVMappingProcessor::ZeroUnmappedCVParams(&parent, 1, 0.001f);
  
  // Mapped child should keep value
  EXPECT_FLOAT_EQ(children[0].value, 0.6f);
  
  // Unmapped child should be zeroed
  EXPECT_FLOAT_EQ(children[1].value, 0.0f);
}