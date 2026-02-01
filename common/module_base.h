#pragma once

#include "daisy_patch.h"
#include "parameter.h"
#include "user_data_manager_base.h"
#include <vector>

namespace mutables_ui {

//=============================================================================
// ModuleBase - Abstract base class for all Mutable Instruments module ports
//=============================================================================
//
// PURPOSE:
// Defines the interface that every ported module (Plaits, Elements, Rings, etc.)
// must implement. This allows the common UI and preset system to work with
// any module without knowing its specific implementation details.
//
// IMPLEMENTATION GUIDE FOR NEW PORTS:
// 1. Create a class inheriting from ModuleBase (e.g., ElementsPort)
// 2. Implement all pure virtual methods (marked = 0)
// 3. Override optional methods as needed for your module
// 4. If your module has user data (wavetables, etc.), also:
//    a) Create a UserDataManager inheriting from UserDataManagerBase
//    b) Override GetUserDataManager() to return it
//    c) Override OnPresetLoaded() to reload user data into DSP
//
// EXAMPLE SKELETON:
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  class ElementsPort : public ModuleBase {                               │
// │  public:                                                                │
// │      const char* GetName() const override { return "Elements"; }        │
// │      const char* GetShortName() const override { return "elements"; }   │
// │      void Init(float sample_rate) override { ... }                      │
// │      void Process(float** in, float** out, size_t size) override { ... }│
// │      Parameter* GetParameters() override { return params_.data(); }     │
// │      size_t GetParameterCount() const override { return params_.size(); }│
// │                                                                         │
// │      // If module has CV outputs:                                       │
// │      float GetCVOutput(int index) override { ... }                      │
// │                                                                         │
// │      // If module has user data:                                        │
// │      UserDataManagerBase* GetUserDataManager() override { return &udm_; }│
// │      void OnPresetLoaded() override { ReloadUserData(); }               │
// │  };                                                                     │
// └─────────────────────────────────────────────────────────────────────────┘
//
//=============================================================================

class ModuleBase {
public:
    virtual ~ModuleBase() = default;
    
    //=========================================================================
    // REQUIRED: Module Identification
    //=========================================================================
    // GetName():      Display name shown in UI (e.g., "Plaits", "Elements")
    // GetShortName(): Used for SD card paths (e.g., "plaits" -> /plaits/presets/)
    //=========================================================================
    virtual const char* GetName() const = 0;
    virtual const char* GetShortName() const = 0;
    
    //=========================================================================
    // REQUIRED: Lifecycle
    //=========================================================================
    // Init():    Called once at startup with the sample rate
    // Process(): Called every audio block from AudioCallback
    //=========================================================================
    virtual void Init(float sample_rate) = 0;
    virtual void Process(float** in, float** out, size_t size) = 0;
    
    //=========================================================================
    // REQUIRED: Parameter Access
    //=========================================================================
    // The parameter array defines all controllable values in your module.
    // These are used by:
    // - UI rendering (menu navigation, value display)
    // - Preset system (save/load)
    // - CV/MIDI mapping
    //=========================================================================
    virtual Parameter* GetParameters() = 0;
    virtual size_t GetParameterCount() const = 0;
    
    //=========================================================================
    // OPTIONAL: Hardware Configuration
    //=========================================================================
    // Override if your module needs special audio routing or hardware setup.
    // Default: standard 4-in/4-out audio, 2 gates, 2 CV outputs
    //=========================================================================
    virtual void ConfigureIO(daisy::DaisyPatch& hw) {
        // Default: no special configuration needed
    }
    
    //=========================================================================
    // OPTIONAL: Gate I/O
    //=========================================================================
    // ProcessGate(): Called when gate input state changes
    // GetGateOutput(): Return state for gate output jack
    //
    // gate_index: 0 = Gate 1, 1 = Gate 2
    //=========================================================================
    virtual void ProcessGate(int gate_index, bool state) {}
    virtual bool GetGateOutput(int gate_index) { return false; }
    
    //=========================================================================
    // OPTIONAL: CV Output
    //=========================================================================
    // Return value for CV output jacks (0.0 to 1.0, scaled to 0-5V)
    // cv_index: 0 = CV Out 1, 1 = CV Out 2
    //=========================================================================
    virtual float GetCVOutput(int cv_index) { return 0.0f; }
    
    //=========================================================================
    // OPTIONAL: MIDI Processing
    //=========================================================================
    // Called for each MIDI event on the module's channel.
    // Default: no MIDI support
    //=========================================================================
    virtual void ProcessMidi(daisy::MidiEvent& event) {}
    
    //=========================================================================
    // OPTIONAL: User Data Support
    //=========================================================================
    // GetUserDataManager(): Return pointer to module's user data manager,
    //                       or nullptr if module doesn't use user data.
    //
    // OnPresetLoaded(): Called after a preset is loaded OR after user data
    //                   is changed. Override this to reload data into DSP.
    //
    // EXAMPLE (Plaits):
    //   UserDataManagerBase* GetUserDataManager() override { 
    //       return &user_data_manager_; 
    //   }
    //   void OnPresetLoaded() override { 
    //       voice_->ReloadUserData(); 
    //   }
    //=========================================================================
    virtual UserDataManagerBase* GetUserDataManager() { return nullptr; }
    virtual void OnPresetLoaded() {}
};

} // namespace mutables_ui
