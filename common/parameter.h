#pragma once

#include <cstdint>
#include <algorithm>

namespace mutables_ui {

enum class ParamType {
    Continuous,    // Float 0.0-1.0 (most MI params)
    Bipolar,       // Float -1.0 to 1.0
    Enum,          // Discrete selection (engine, algorithm)
    Toggle,        // Boolean on/off
    Integer        // Stepped values
};

struct CVMapping {
    int8_t cv_input;        // -1 = unmapped, 0-3 = CV1-4
    float attenuverter;     // -1.0 to 1.0 (amount and polarity)
    float origin_offset;    // Captured knob value as center point
    bool active;
    
    CVMapping() 
        : cv_input(-1)
        , attenuverter(1.0f)
        , origin_offset(0.5f)
        , active(false) {}
};

struct Parameter {
    const char* name;
    ParamType type;
    float value;
    float min;
    float max;
    CVMapping cv_mapping;
    
    // For enums:
    const char** enum_labels;
    uint8_t enum_count;
    
    // For integer params
    int step_count;
    
    // Default constructor
    Parameter()
        : name("")
        , type(ParamType::Continuous)
        , value(0.0f)
        , min(0.0f)
        , max(1.0f)
        , enum_labels(nullptr)
        , enum_count(0)
        , step_count(0) {}
    
    Parameter(const char* name, float min = 0.0f, float max = 1.0f)
        : name(name)
        , type(ParamType::Continuous)
        , value((min + max) / 2.0f)
        , min(min)
        , max(max)
        , enum_labels(nullptr)
        , enum_count(0)
        , step_count(0) {}
    
    Parameter(const char* name, const char** labels, uint8_t count)
        : name(name)
        , type(ParamType::Enum)
        , value(0.0f)
        , min(0.0f)
        , max(count - 1.0f)
        , enum_labels(labels)
        , enum_count(count)
        , step_count(count) {}
        
    // Get normalized value (0.0 to 1.0)
    float GetNormalized() const {
        if (max == min) return 0.0f;
        return (value - min) / (max - min);
    }
    
    // Set from normalized value (0.0 to 1.0)
    void SetNormalized(float normalized) {
        value = min + normalized * (max - min);
        value = std::clamp(value, min, max);
    }
    
    // Get integer index for enum/integer params
    int GetIndex() const {
        return static_cast<int>(value + 0.5f);
    }
    
    // Get enum label
    const char* GetEnumLabel() const {
        if (type == ParamType::Enum && enum_labels) {
            int idx = GetIndex();
            if (idx >= 0 && idx < enum_count) {
                return enum_labels[idx];
            }
        }
        return "";
    }
    
    // Check if parameter has CV mapping submenu
    bool HasSubmenu() const {
        return true; // All parameters can have CV mapping
    }
};

} // namespace mutables_ui
