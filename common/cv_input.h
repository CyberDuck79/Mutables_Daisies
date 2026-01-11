#pragma once

#include "parameter.h"
#include <algorithm>

namespace mutables_ui {

class CVInput {
public:
    CVInput() : filtered_value_(0.0f) {}
    
    // Process CV input with attenuverter emulation
    // cv_value: Current CV value from CVInputBank
    // mapping: Parameter's mapping configuration
    // Returns: Final parameter value with attenuverter applied
    static float ProcessWithMapping(float cv_value, const MappingConfig& mapping) {
        if (!mapping.IsCVSource()) {
            return cv_value;
        }
        
        if (mapping.plugged) {
            // Attenuverter emulation: cv_signal = current - offset
            float cv_signal = cv_value - mapping.offset;
            float result = mapping.offset + (cv_signal * mapping.attenuverter);
            return std::clamp(result, 0.0f, 1.0f);
        } else {
            // No plugged: direct value
            return cv_value;
        }
    }
    
    // Simple one-pole lowpass filter for CV input
    // Helps reduce noise and jitter from CV inputs
    float Filter(float input, float coefficient = 0.02f) {
        // Scale input from actual ADC range to full 0.0-1.0
        // Pots physically don't reach exact 0.0/1.0, typically ~0.03 to ~0.96
        const float adc_min = 0.025f;
        const float adc_max = 0.97f;
        input = (input - adc_min) / (adc_max - adc_min);
        input = std::clamp(input, 0.0f, 1.0f);
        
        filtered_value_ += coefficient * (input - filtered_value_);
        
        // Snap to edges for display (0.99 rounds to 1.00, 0.00x rounds to 0.00)
        float output = filtered_value_;
        if (output < 0.01f) output = 0.0f;
        if (output > 0.99f) output = 1.0f;
        
        return output;
    }
    
    void Reset() {
        filtered_value_ = 0.0f;
    }
    
private:
    float filtered_value_;
};

// Helper to manage all 4 CV inputs
class CVInputBank {
public:
    CVInputBank() {}
    
    void UpdateRawValues(float cv1, float cv2, float cv3, float cv4) {
        raw_values_[0] = cv1;
        raw_values_[1] = cv2;
        raw_values_[2] = cv3;
        raw_values_[3] = cv4;
        
        // Apply filtering
        for (int i = 0; i < 4; i++) {
            filtered_values_[i] = filters_[i].Filter(raw_values_[i]);
        }
    }
    
    float GetFiltered(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return filtered_values_[index];
    }
    
    float GetRaw(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return raw_values_[index];
    }
    
private:
    float raw_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float filtered_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    CVInput filters_[4];
};

} // namespace mutables_ui
