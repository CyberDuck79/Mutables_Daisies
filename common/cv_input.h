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
    float Filter(float input, float coefficient = 0.01f) {
        filtered_value_ += coefficient * (input - filtered_value_);
        return filtered_value_;
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
