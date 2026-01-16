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
    
    // Simple one-pole lowpass filter for CV input (raw, no pot scaling)
    // Only removes high-frequency noise, preserves CV precision
    float Filter(float input, float coefficient = 0.1f) {
        // No pot scaling - use raw ADC value directly
        // This preserves precision for V/Oct and other CV applications
        filtered_value_ += coefficient * (input - filtered_value_);
        return filtered_value_;
    }
    
    // Get the raw unfiltered value
    float GetRaw() const { return raw_value_; }
    
    void SetRaw(float value) { raw_value_ = value; }
    
    void Reset() {
        filtered_value_ = 0.0f;
        raw_value_ = 0.0f;
    }
    
private:
    float filtered_value_;
    float raw_value_ = 0.0f;
};

// Helper to manage all 4 CV inputs
class CVInputBank {
public:
    CVInputBank() {}
    
    // Update with raw ADC values (0.0-1.0 from ADC)
    void UpdateRawValues(float cv1, float cv2, float cv3, float cv4) {
        raw_values_[0] = cv1;
        raw_values_[1] = cv2;
        raw_values_[2] = cv3;
        raw_values_[3] = cv4;
        
        // Store raw and apply light filtering
        for (int i = 0; i < 4; i++) {
            filters_[i].SetRaw(raw_values_[i]);
            filtered_values_[i] = filters_[i].Filter(raw_values_[i]);
        }
    }
    
    // Get filtered value (light lowpass only, no pot scaling)
    float GetFiltered(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return filtered_values_[index];
    }
    
    // Get raw unfiltered ADC value (for maximum precision, e.g., V/Oct)
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
