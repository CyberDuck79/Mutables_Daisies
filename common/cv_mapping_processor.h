#pragma once

#include "parameter.h"
#include <algorithm>
#include <cmath>

namespace mutables_ui {

class CVMappingProcessor {
public:
  // Calculate parameter value with mapping applied
  // param: The parameter to calculate value for
  // base_value: The parameter's current manual setting
  // cv_value: The filtered CV input value (0.0 to 1.0) for the mapped source
  static float CalculateMappedValue(const Parameter &param, float base_value,
                                    float cv_value) {
    const MappingConfig &m = param.mapping;

    if (m.source == MappingSource::NONE) {
      return base_value;
    }

    // This function expects valid CV value.
    // Logic handles whether it's actually a CV source calculation.
    // If it's a CC source, this function treats it as "not CV source" unless we
    // change logic, but typically CalculateMappedValue is used for CV. Let's
    // stick to the main.cpp logic but adapted.

    if (m.IsCVSource()) {
      if (m.plugged) {
        // Attenuverter emulation: cv_signal = current - offset
        float cv_signal = cv_value - m.offset;
        return std::clamp(m.offset + (cv_signal * m.attenuverter), 0.0f, 1.0f);
      } else {
        // Direct CV: just use the value
        return cv_value;
      }
    }

    return base_value;
  }

  // Calculate ENUM index from CV value with attenuverter
  // param: The parameter
  // cv_value: The filtered CV input value (0.0 to 1.0)
  static int CalculateEnumFromCV(const Parameter &param, float cv_value) {
    const MappingConfig &m = param.mapping;

    if (!m.IsCVSource())
      return param.GetIndex();

    // If plugged, use offset-based attenuverter (like KNOB)
    float scaled;
    if (m.plugged) {
      float cv_signal = cv_value - m.offset;
      scaled = 0.5f + cv_signal * m.attenuverter;
    } else {
      // Without plugged, simple centered scaling
      scaled = 0.5f + (cv_value - 0.5f) * m.attenuverter;
    }
    scaled = std::clamp(scaled, 0.0f, 1.0f);

    // Quantize to enum count
    int index = static_cast<int>(scaled * param.enum_count);
    return std::clamp(index, 0, static_cast<int>(param.enum_count) - 1);
  }

  // Cycle through mapping sources
  // param: The parameter to modify
  // direction: +1 or -1
  static void CycleMappingSource(Parameter &param, int direction) {
    int current = static_cast<int>(param.mapping.source);

    if (param.type == ParamType::KNOB) {
      // KNOB: None, CV1-4, CC (skip Gate1, Gate2)
      do {
        current += direction;
        // Wrap around at CC/None
        if (current < 0)
          current = static_cast<int>(MappingSource::CC);
        if (current > static_cast<int>(MappingSource::CC))
          current = 0; // NONE is 0
      } while (current == static_cast<int>(MappingSource::GATE1) ||
               current == static_cast<int>(MappingSource::GATE2));
    } else if (param.type == ParamType::CV) {
      // CV: None, CV1-4 only
      // CV params usually just map to their own CV input or can be reassigned?
      // In encoder_handlers.h: "CV: None, CV1-4 only"
      current += direction;
      if (current < 0)
        current = static_cast<int>(MappingSource::CV4);
      if (current > static_cast<int>(MappingSource::CV4))
        current = 0;
    } else if (param.type == ParamType::ENUM) {
      // ENUM: None, CV1-4, CC (skip Gate1, Gate2)
      // Wait, in encoder_handlers.h, ENUM loop logic seemed identical to KNOB.
      // But ENUMs can also have triggers (Gate)...
      // Let's re-read encoder_handlers.h logic.
      // "skip Gate1, Gate2" was in the loop for ENUM too.
      do {
        current += direction;
        if (current < 0)
          current = static_cast<int>(MappingSource::CC);
        if (current > static_cast<int>(MappingSource::CC))
          current = 0;
      } while (current == static_cast<int>(MappingSource::GATE1) ||
               current == static_cast<int>(MappingSource::GATE2));

      // Note: Gate mapping for ENUM (Triggers) seems to be configured
      // separately or maybe the encoder handler logic excluded it for general
      // mapping source cycling? "Trigger (if Gate mapped)" is handled in
      // submenu item 4. But how do you select Gate source if CycleMappingSource
      // skips it?

      // Re-reading encoder_handlers.h:
      // The mapping submenu has item 0: Mapping.
      // CycleMappingSource is called for item 0.
      // The logic shown explicitly skips GATE1/GATE2 for ENUM.
      // This is strange if ENUMs support Gate Triggers.
      // Ah, maybe Gate mapping is meant to be set differently?
      // Or maybe there is a bug in the original code?
      // Or maybe `MappingSource::GATE1` and `GATE2` are not valid for the
      // "source" field? Parameter.h defines: NONE, CV1-4, GATE1, GATE2, CC.

      // Let's assume the original code is correct for now and copy it exactly.
      // If ENUM MappingSource can be GATE1/2, then CycleMappingSource SHOULD
      // allow it. But line 48 in encoder_handlers.h: `while (current == GATE1
      // || current == GATE2);` This definitely skips Gates. Wait, maybe I
      // misread. ParamType::ENUM handler in main.cpp handles "Trigger (if Gate
      // mapped)". If we can't select Gate source, how is `IsGateSource()` ever
      // true?

      // Let's check `Parameter.h`.
      // `IsGateSource()` checks `source == MappingSource::GATE1`.

      // This suggests that maybe ENUMs *should* function with Gates, but the
      // original `CycleMappingSource` implementation extracted from
      // `encoder_handlers.h` explicitly SKIPS them. This might be a bug I
      // stumbled upon, or intent I don't grasp.

      // Wait, checking `encoder_handlers.h` again.
      // Lines 43-50:
      // do { ... } while (current == GATE1 || current == GATE2);

      // Could it be that Gate mapping is only for... specific enums?
      // Or maybe I should check if I missed something.

      // I'll replicate the exact logic for now to ensure behavior parity
      // ("Extraction"). If I fix a bug, it should be a separate step or noted.
      // BUT, if I'm "Extracting Pure Functions", I should extract what is
      // there.
    }

    param.mapping.source = static_cast<MappingSource>(current);
  }
};

} // namespace mutables_ui
