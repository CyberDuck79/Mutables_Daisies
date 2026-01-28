#pragma once

#include <cstddef>
#include <cstdint>

namespace mutables_ui {

//=============================================================================
// CV Input Calibration Data
//=============================================================================

// Default calibration values (typical Daisy Patch hardware)
static constexpr float kDefaultCVMin = 0.03f;
static constexpr float kDefaultCVMax = 0.96f;

// Small margin to ensure we can reach true 0.0 and 1.0 after calibration
// Keep small since we now capture over time to get actual min/max
static constexpr float kCalibrationMargin = 0.0005f;

// Capture duration in milliseconds (time to sample ADC for min/max)
static constexpr uint32_t kCalibrationCaptureDurationMs = 4000;

// Calibration data for a single CV input
struct CVCalibration {
    float min;  // Raw ADC value when knob is fully CCW
    float max;  // Raw ADC value when knob is fully CW
    
    CVCalibration() : min(kDefaultCVMin), max(kDefaultCVMax) {}
    CVCalibration(float min_val, float max_val) : min(min_val), max(max_val) {}
    
    // Scale a raw ADC value (0.0-1.0) to calibrated range (0.0-1.0)
    // Applies margin to ensure we can reach true 0.0 and 1.0
    float Scale(float raw) const {
        // Apply margin: slightly expand the range
        float effective_min = min - kCalibrationMargin;
        float effective_max = max + kCalibrationMargin;
        float range = effective_max - effective_min;
        
        if (range <= 0.0f) return 0.5f;  // Safety: avoid division by zero
        
        float scaled = (raw - effective_min) / range;
        
        // Clamp to 0.0-1.0
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 1.0f) scaled = 1.0f;
        
        return scaled;
    }
    
    // Check if using default values
    bool IsDefault() const {
        return (min == kDefaultCVMin && max == kDefaultCVMax);
    }
    
    // Reset to default values
    void Reset() {
        min = kDefaultCVMin;
        max = kDefaultCVMax;
    }
};

// File format constants
static constexpr uint32_t kCalibrationMagic = 0x43414C49;  // "CALI"
static constexpr uint16_t kCalibrationVersion = 1;

// System-wide calibration data (stored in /system/calibration.bin)
struct SystemCalibration {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    CVCalibration cv_inputs[4];
    uint32_t checksum;
    
    SystemCalibration() 
        : magic(kCalibrationMagic)
        , version(kCalibrationVersion)
        , reserved(0)
        , checksum(0) {}
    
    // Calculate checksum for validation
    uint32_t CalculateChecksum() const {
        uint32_t sum = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        // Sum all bytes except the checksum field itself
        for (size_t i = 0; i < sizeof(SystemCalibration) - sizeof(checksum); i++) {
            sum += data[i];
        }
        return sum;
    }
    
    // Validate the calibration data
    bool IsValid() const {
        return (magic == kCalibrationMagic && 
                version == kCalibrationVersion &&
                checksum == const_cast<SystemCalibration*>(this)->CalculateChecksum());
    }
    
    // Update checksum before saving
    void UpdateChecksum() {
        checksum = CalculateChecksum();
    }
    
    // Reset all calibrations to default
    void ResetToDefaults() {
        magic = kCalibrationMagic;
        version = kCalibrationVersion;
        reserved = 0;
        for (int i = 0; i < 4; i++) {
            cv_inputs[i].Reset();
        }
        UpdateChecksum();
    }
};

//=============================================================================
// Calibration UI State
//=============================================================================

// Calibration steps
enum class CalibrationStep {
    SelectCV,       // Select which CV to calibrate (CV1-CV4, Reset, Save, Cancel)
    CaptureMin,     // Turn knob to minimum, press to capture
    CaptureMax,     // Turn knob to maximum, press to capture
    Confirm         // Show result, press to accept or rotate to retry
};

// Calibration menu items
enum class CalibrationMenuItem {
    CV1 = 0,
    CV2 = 1,
    CV3 = 2,
    CV4 = 3,
    ResetAll,
    Save,
    COUNT
};

static constexpr int kCalibrationMenuItemCount = static_cast<int>(CalibrationMenuItem::COUNT);

// Get display name for menu item
inline const char* GetCalibrationMenuItemName(CalibrationMenuItem item) {
    switch (item) {
        case CalibrationMenuItem::CV1: return "CV1";
        case CalibrationMenuItem::CV2: return "CV2";
        case CalibrationMenuItem::CV3: return "CV3";
        case CalibrationMenuItem::CV4: return "CV4";
        case CalibrationMenuItem::ResetAll: return "Reset All";
        case CalibrationMenuItem::Save: return "Save";
        default: return "?";
    }
}

} // namespace mutables_ui
