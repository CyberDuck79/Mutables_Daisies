#pragma once

#include "parameter.h"
#include <algorithm>

namespace mutables_ui {

class CVInput {
public:
    CVInput() : filtered_value_(0.0f) {}
    
    // Process CV input with attenuverter simulation
    // knob_value: current knob position (0.0 to 1.0)
    // cv_value: CV input value (0.0 to 1.0, hardware already normalized)
    // Returns: final parameter value with CV applied
    static float ProcessWithMapping(const Parameter& param, 
                                   float knob_value, 
                                   float cv_value) {
        if (param.cv_mapping.cv_input < 0 || !param.cv_mapping.active) {
            return knob_value;
        }
        
        // CV contribution scaled by attenuverter
        float cv_contribution = (cv_value - 0.5f) * 2.0f * param.cv_mapping.attenuverter;
        
        // Apply relative to origin offset
        float result = param.cv_mapping.origin_offset + cv_contribution;
        
        // Clamp to normalized range
        return std::clamp(result, 0.0f, 1.0f);
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
