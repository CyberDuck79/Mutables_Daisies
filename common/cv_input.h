#pragma once

#include "calibration.h"
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

// Helper to manage all 4 CV inputs with calibration support
class CVInputBank {
public:
    CVInputBank() : calibration_(nullptr) {}
    
    // Set calibration data (optional, uses defaults if not set)
    void SetCalibration(const SystemCalibration* calibration) {
        calibration_ = calibration;
    }
    
    // Update with raw ADC values (0.0-1.0 from ADC, before any scaling)
    // Note: Caller should NOT invert or scale - pass raw ADC values
    void UpdateRawValues(float cv1, float cv2, float cv3, float cv4) {
        // Store the truly raw ADC values (before any calibration)
        raw_adc_values_[0] = cv1;
        raw_adc_values_[1] = cv2;
        raw_adc_values_[2] = cv3;
        raw_adc_values_[3] = cv4;
        
        // Apply calibration scaling
        for (int i = 0; i < 4; i++) {
            if (calibration_) {
                raw_values_[i] = calibration_->cv_inputs[i].Scale(raw_adc_values_[i]);
            } else {
                // Use default calibration
                CVCalibration default_cal;
                raw_values_[i] = default_cal.Scale(raw_adc_values_[i]);
            }
            
            // Store scaled raw and apply light filtering
            filters_[i].SetRaw(raw_values_[i]);
            filtered_values_[i] = filters_[i].Filter(raw_values_[i]);
        }
    }
    
    // Get filtered value (light lowpass, after calibration scaling)
    float GetFiltered(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return filtered_values_[index];
    }
    
    // Get calibrated raw value (after calibration, before filtering)
    float GetRaw(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return raw_values_[index];
    }
    
    // Get truly raw ADC value (before any calibration or filtering)
    // Use this for calibration capture
    float GetRawADC(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return raw_adc_values_[index];
    }
    
private:
    float raw_adc_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // Truly raw ADC
    float raw_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};       // After calibration
    float filtered_values_[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // After filtering
    CVInput filters_[4];
    const SystemCalibration* calibration_;
};

} // namespace mutables_ui
