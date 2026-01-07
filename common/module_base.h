#pragma once

#include "daisy_patch.h"
#include "parameter.h"
#include <vector>

namespace mutables_ui {

// Base class for all Mutable Instruments module ports
class ModuleBase {
public:
    virtual ~ModuleBase() = default;
    
    // Module identification
    virtual const char* GetName() const = 0;
    virtual const char* GetShortName() const = 0;  // For filenames
    
    // Lifecycle
    virtual void Init(float sample_rate) = 0;
    virtual void Process(float** in, float** out, size_t size) = 0;
    
    // Parameter access
    virtual Parameter* GetParameters() = 0;
    virtual size_t GetParameterCount() const = 0;
    
    // Hardware configuration (module-specific)
    virtual void ConfigureIO(daisy::DaisyPatch& hw) {
        // Default: standard stereo audio
        // Modules can override for specific needs
    }
    
    // Gate/trigger handling (optional)
    virtual void ProcessGate(int gate_index, bool state) {}
    virtual bool GetGateOutput(int gate_index) { return false; }
    
    // CV output handling (optional)
    virtual float GetCVOutput(int cv_index) { return 0.0f; }
    
    // MIDI handling (optional)
    virtual void ProcessMidi(daisy::MidiEvent& event) {}
};

} // namespace mutables_ui
