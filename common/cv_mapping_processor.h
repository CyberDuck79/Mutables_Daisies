#pragma once

#include "cv_input.h"
#include "parameter.h"
#include <algorithm>
#include <cmath>

namespace mutables_ui {

class CVMappingProcessor {
public:
  CVMappingProcessor() = default;

  // Mark cache as needing rebuild (call when mappings change)
  void MarkDirty() { cache_dirty_ = true; }
  bool IsDirty() const { return cache_dirty_; }

  // Rebuild mapping cache from parameters
  void RebuildCache(Parameter *params, size_t param_count) {
    // Clear caches
    for (int i = 0; i < 4; i++)
      cv_mappings_[i].count = 0;
    for (int i = 0; i < 128; i++)
      cc_mappings_[i].count = 0;

    auto ProcessParam = [&](Parameter &param) {
      if (param.mapping.IsCVSource()) {
        int cv_idx = param.mapping.GetCVIndex();
        if (cv_idx >= 0 && cv_idx < 4) {
          auto &cache = cv_mappings_[cv_idx];
          if (cache.count < 8)
            cache.mapped_params[cache.count++] = &param;
        }
      } else if (param.mapping.source == MappingSource::CC) {
        int cc_num = param.mapping.cc_number;
        if (cc_num >= 0 && cc_num < 128) {
          auto &cache = cc_mappings_[cc_num];
          if (cache.count < 8)
            cache.mapped_params[cache.count++] = &param;
        }
      }
    };

    for (size_t i = 0; i < param_count; i++) {
      ProcessParam(params[i]);
      // Handle SUB children (1 level deep)
      if (params[i].type == ParamType::SUB && params[i].children) {
        for (int j = 0; j < params[i].child_count; j++) {
          ProcessParam(params[i].children[j]);
        }
      }
    }
    cache_dirty_ = false;
  }

  // Process all CV-mapped parameters
  void ProcessCVMappings(const CVInputBank &cv_inputs, float hysteresis,
                         bool sample_hold_trigger = false) {
    if (cache_dirty_)
      return; // Should call RebuildCache first

    // Cache filtered CVs to avoid re-filtering
    float filtered_cvs[4];
    for (int i = 0; i < 4; i++)
      filtered_cvs[i] = cv_inputs.GetFiltered(i);

    for (int cv = 0; cv < 4; cv++) {
      const auto &cache = cv_mappings_[cv];
      if (cache.count == 0)
        continue;

      float cv_value = filtered_cvs[cv];

      for (uint8_t i = 0; i < cache.count; i++) {
        Parameter *param = cache.mapped_params[i];

        if (param->type == ParamType::KNOB) {
          float mapped = CalculateMappedValue(*param, param->value, cv_value);
          param->SetNormalizedWithHysteresis(mapped, hysteresis);
        } else if (param->type == ParamType::CV) {
          param->SetNormalizedWithHysteresis(cv_value, hysteresis);
        } else if (param->type == ParamType::ENUM) {
          if (param->mapping.plugged) {
            if (sample_hold_trigger) {
              int idx = CalculateEnumFromCV(*param, cv_value);
              param->SetIndex(idx);
            }
          } else {
            int idx = CalculateEnumFromCV(*param, cv_value);
            param->SetIndex(idx);
          }
        }
      }
    }
  }

  // Process all CC-mapped parameters
  void ProcessCCMappings(const float *cc_values, float hysteresis) {
    if (cache_dirty_)
      return;

    for (int cc = 0; cc < 128; cc++) {
      const auto &cache = cc_mappings_[cc];
      if (cache.count == 0)
        continue;

      float val = cc_values[cc];
      for (uint8_t i = 0; i < cache.count; i++) {
        Parameter *param = cache.mapped_params[i];
        if (param->type == ParamType::KNOB) {
          param->SetNormalizedWithHysteresis(val, hysteresis);
        } else if (param->type == ParamType::ENUM) {
          int index = static_cast<int>(val * param->enum_count);
          index = std::clamp(index, 0, static_cast<int>(param->enum_count) - 1);
          param->SetIndex(index);
        }
      }
    }
  }

  // Static Helpers
  static float CalculateMappedValue(const Parameter &param, float base_value,
                                    float cv_value) {
    const MappingConfig &m = param.mapping;

    if (m.source == MappingSource::NONE) {
      return base_value;
    }

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

  static int CalculateEnumFromCV(const Parameter &param, float cv_value) {
    const MappingConfig &m = param.mapping;

    if (!m.IsCVSource())
      return param.GetIndex();

    float scaled;
    if (m.plugged) {
      float cv_signal = cv_value - m.offset;
      scaled = 0.5f + cv_signal * m.attenuverter;
    } else {
      scaled = 0.5f + (cv_value - 0.5f) * m.attenuverter;
    }
    scaled = std::clamp(scaled, 0.0f, 1.0f);

    int index = static_cast<int>(scaled * param.enum_count);
    return std::clamp(index, 0, static_cast<int>(param.enum_count) - 1);
  }

  static void CycleMappingSource(Parameter &param, int direction) {
    int current = static_cast<int>(param.mapping.source);

    if (param.type == ParamType::KNOB || param.type == ParamType::ENUM) {
      do {
        current += direction;
        if (current < 0)
          current = static_cast<int>(MappingSource::CC);
        if (current > static_cast<int>(MappingSource::CC))
          current = 0;
      } while (current == static_cast<int>(MappingSource::GATE1) ||
               current == static_cast<int>(MappingSource::GATE2));
    } else if (param.type == ParamType::CV) {
      current += direction;
      if (current < 0)
        current = static_cast<int>(MappingSource::CV4);
      if (current > static_cast<int>(MappingSource::CV4))
        current = 0;
    }

    param.mapping.source = static_cast<MappingSource>(current);
  }

private:
  struct CVMappingCache {
    Parameter *mapped_params[8];
    uint8_t count = 0;
  };

  CVMappingCache cv_mappings_[4];
  CVMappingCache cc_mappings_[128];
  bool cache_dirty_ = true;
};

} // namespace mutables_ui
