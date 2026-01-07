#pragma once

#include "fatfs.h"
#include "parameter.h"
#include <vector>
#include <cstring>

namespace mutables_ui {

// Simple preset structure
struct Preset {
    char name[32];
    std::vector<float> param_values;
    std::vector<CVMapping> cv_mappings;
};

class PresetManager {
public:
    PresetManager() : initialized_(false) {}
    
    bool Init(const char* module_name) {
        module_name_ = module_name;
        
        // TODO: Initialize SD card and create directory structure
        // Path: /<module_name>/presets/
        
        initialized_ = true;
        return initialized_;
    }
    
    bool SavePreset(const char* preset_name, 
                   const std::vector<Parameter>& params) {
        if (!initialized_) return false;
        
        // TODO: Implement binary or JSON serialization
        // Path: /<module_name>/presets/<preset_name>.bin
        
        return false; // Placeholder
    }
    
    bool LoadPreset(const char* preset_name, 
                   std::vector<Parameter>& params) {
        if (!initialized_) return false;
        
        // TODO: Implement deserialization
        
        return false; // Placeholder
    }
    
    // List available presets
    std::vector<const char*> ListPresets() {
        std::vector<const char*> presets;
        
        // TODO: Scan directory for preset files
        
        return presets;
    }
    
private:
    bool initialized_;
    const char* module_name_;
};

} // namespace mutables_ui
