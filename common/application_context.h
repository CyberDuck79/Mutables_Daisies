#pragma once

#include "calibration_manager.h"
#include "constants.h"
#include "controllers/encoder_controller.h"
#include "cv_input.h"
#include "cv_mapping_processor.h"
#include "daisy_patch.h"
#include "display.h"
#include "midi_processor.h"
#include "module_base.h"
#include "preset_manager.h"
#include "ui_state.h"

namespace mutables_ui {

//=============================================================================
// ApplicationContext - Central coordinator for Mutable Instruments ports
//=============================================================================
//
// PURPOSE:
// This class eliminates ~150 lines of repetitive callback setup code that
// would otherwise be duplicated in every port's main.cpp.
//
// It provides:
// 1. Standard callbacks for presets, calibration, messages (100% reusable)
// 2. User data callbacks that work with any module's UserDataManager
// 3. Consistent patterns for SD operations (StopAudio → Do → StartAudio)
//
// USAGE IN A NEW PORT:
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  // In main.cpp, after initializing all components:                     │
// │                                                                         │
// │  ApplicationContext app_ctx(                                            │
// │      hw,                    // DaisyPatch hardware                      │
// │      my_module,             // Your ModuleBase-derived class            │
// │      menu,                  // MenuState                                │
// │      display,               // Display                                  │
// │      cv_inputs,             // CVInputBank                              │
// │      cv_processor,          // CVMappingProcessor                       │
// │      midi_processor,        // MIDIProcessor                            │
// │      preset_manager,        // PresetManager                            │
// │      calibration_manager,   // CalibrationManager                       │
// │      encoder_controller     // EncoderController                        │
// │  );                                                                     │
// │                                                                         │
// │  // Set audio callback for StopAudio/StartAudio                         │
// │  app_ctx.audio_callback = AudioCallback;                                │
// │                                                                         │
// │  // Build and register all callbacks in one line!                       │
// │  encoder_controller.SetCallbacks(app_ctx.BuildStandardCallbacks());     │
// └─────────────────────────────────────────────────────────────────────────┘
//
// WHAT YOU GET FOR FREE:
// - on_save_preset:           Saves preset to SD with proper audio handling
// - on_scan_presets:          Scans preset directory
// - on_load_preset:           Loads preset + reloads user data + marks CV dirty
// - on_user_data_browser:     Opens file browser for user data (if module has it)
// - on_user_data_selected:    Loads selected user data file
// - on_display_message:       Shows message with standard delay
// - on_get_raw_cv:            Returns raw CV for calibration
// - on_calibration_complete:  Applies calibration values
// - on_calibration_save:      Saves calibration to SD
// - on_calibration_reset_all: Resets calibration to defaults
//
// MODULE-SPECIFIC HOOKS:
// If your module needs custom behavior after preset load (e.g., Plaits reloads
// user data into DSP), override ModuleBase::OnPresetLoaded() in your port.
//
//=============================================================================

// Audio callback function pointer type
// Each port defines its own AudioCallback function with this signature
using AudioCallbackFn = void (*)(daisy::AudioHandle::InputBuffer,
                                  daisy::AudioHandle::OutputBuffer, size_t);

class ApplicationContext {
public:
  //===========================================================================
  // Public References - All the components this context coordinates
  //===========================================================================
  // These are references (not owned), passed in from main.cpp
  
  daisy::DaisyPatch &hw;              // Hardware interface
  ModuleBase &module;                  // The DSP module (Plaits, Elements, etc.)
  MenuState &menu;                     // UI navigation state
  Display &display;                    // OLED display controller
  CVInputBank &cv_inputs;              // CV input processing
  CVMappingProcessor &cv_processor;    // CV/MIDI → Parameter mapping
  MIDIProcessor &midi_processor;       // MIDI channel filtering
  PresetManager &preset_manager;       // Preset save/load
  CalibrationManager &calibration_manager;  // CV calibration
  EncoderController &encoder_controller;    // Encoder state machine

  //===========================================================================
  // Audio Callback - MUST be set before calling BuildStandardCallbacks()
  //===========================================================================
  // This is needed because SD card operations require stopping audio to
  // prevent DMA conflicts. After the operation, we restart with this callback.
  //
  // Set this to your AudioCallback function:
  //   app_ctx.audio_callback = AudioCallback;
  //===========================================================================
  AudioCallbackFn audio_callback = nullptr;

  //===========================================================================
  // User Data File Browser State
  //===========================================================================
  // Shared buffer for file listings. Used by on_user_data_browser callback.
  // This lives here so it persists across callback invocations.
  //===========================================================================
  static constexpr int kMaxUserDataFiles = 32;
  char user_data_files[kMaxUserDataFiles][32];
  int user_data_file_count = 0;

  //===========================================================================
  // Constructor
  //===========================================================================
  ApplicationContext(daisy::DaisyPatch &hw_, ModuleBase &module_,
                     MenuState &menu_, Display &display_,
                     CVInputBank &cv_inputs_, CVMappingProcessor &cv_processor_,
                     MIDIProcessor &midi_processor_,
                     PresetManager &preset_manager_,
                     CalibrationManager &calibration_manager_,
                     EncoderController &encoder_controller_)
      : hw(hw_), module(module_), menu(menu_), display(display_),
        cv_inputs(cv_inputs_), cv_processor(cv_processor_),
        midi_processor(midi_processor_), preset_manager(preset_manager_),
        calibration_manager(calibration_manager_),
        encoder_controller(encoder_controller_) {}

  //===========================================================================
  // BuildStandardCallbacks - Creates all encoder callbacks
  //===========================================================================
  // Call this AFTER setting audio_callback.
  // Returns an EncoderCallbacks struct with all callbacks configured.
  //
  // Pattern: All SD operations follow this template:
  //   1. StopAudio (prevent DMA conflicts)
  //   2. Do the operation
  //   3. StartAudio (resume)
  //   4. Show result message
  //   5. Delay for user to read
  //===========================================================================
  EncoderCallbacks BuildStandardCallbacks() {
    EncoderCallbacks cb;

    //=========================================================================
    // PRESET CALLBACKS
    //=========================================================================

    // on_save_preset: Save current parameters to SD
    // Triggered when user confirms preset name in CharInput mode
    cb.on_save_preset = [this]() {
      char final_name[MenuState::MAX_PRESET_NAME_LEN + 1];
      menu.GetFinalPresetName(final_name, sizeof(final_name));

      if (!preset_manager.IsSDAvailable()) {
        ShowMessage("Error", "No SD Card");
        return;
      }

      hw.StopAudio();
      bool success = preset_manager.SavePreset(final_name, module.GetParameters(),
                                               module.GetParameterCount());
      if (audio_callback) hw.StartAudio(audio_callback);
      ShowResult(success, final_name, "Save failed");
    };

    // on_scan_presets: List available presets
    // Triggered when user enters LOAD menu item
    cb.on_scan_presets = [this]() -> int {
      if (!preset_manager.IsSDAvailable()) {
        ShowMessage("Error", "No SD Card");
        return 0;
      }
      return preset_manager.ScanPresets();
    };

    // on_load_preset: Load a preset by index
    // Triggered when user selects a preset from the list
    cb.on_load_preset = [this](int index) {
      const char *name = preset_manager.GetPresetName(index);
      if (!name)
        return;

      hw.StopAudio();
      bool success = preset_manager.LoadPreset(name, module.GetParameters(),
                                               module.GetParameterCount());
      if (success) {
        // IMPORTANT: After loading parameters, we must:
        // 1. Reload any user data referenced by USER_DATA parameters
        // 2. Notify the module (it may need to update internal state)
        // 3. Mark CV processor dirty (mappings may have changed)
        ReloadUserDataFromParams();
        module.OnPresetLoaded();
        cv_processor.MarkDirty();
      }
      if (audio_callback) hw.StartAudio(audio_callback);
      ShowResult(success, name, "Load failed");
    };

    //=========================================================================
    // USER DATA CALLBACKS
    //=========================================================================
    // These work with any module that has a UserDataManager.
    // If module.GetUserDataManager() returns nullptr, they no-op gracefully.

    // on_user_data_browser: Open file browser for a user data target
    // Triggered when user presses encoder on a USER_DATA parameter
    cb.on_user_data_browser = [this](int target_int) {
      auto *udm = module.GetUserDataManager();
      if (!udm)
        return;

      user_data_file_count =
          udm->ListFiles(target_int, user_data_files, kMaxUserDataFiles);
      menu.EnterFileBrowser(menu.selected_param, user_data_file_count);
    };

    // on_user_data_selected: Load selected file for a user data target
    // Triggered when user selects a file from the browser
    cb.on_user_data_selected = [this](Parameter *param, int file_idx) {
      auto *udm = module.GetUserDataManager();
      if (!udm)
        return;

      // Update the parameter's stored filename
      // Index 0 = "Default", Index 1+ = actual files
      if (file_idx == 0) {
        param->SetUserDataFile(""); // Empty = default
      } else if (file_idx > 0 && file_idx <= user_data_file_count) {
        param->SetUserDataFile(user_data_files[file_idx - 1]);
      }

      int target = param->user_data_target;
      const char *filename =
          param->user_data_filename[0] ? param->user_data_filename : nullptr;

      hw.StopAudio();
      bool success = filename ? udm->LoadTarget(target, filename)
                              : udm->LoadDefaultForTarget(target);
      // Notify module that user data changed
      module.OnPresetLoaded();
      if (audio_callback) hw.StartAudio(audio_callback);
      ShowResult(success, filename ? filename : "Default", "Load failed");
    };

    //=========================================================================
    // CALIBRATION CALLBACKS
    //=========================================================================

    // on_get_raw_cv: Return raw ADC value for calibration display
    cb.on_get_raw_cv = [this](int cv_index) -> float {
      return cv_inputs.GetRawADC(cv_index);
    };

    // on_calibration_complete: Apply captured calibration values
    cb.on_calibration_complete = [this](int cv_index, float min_val,
                                        float max_val) {
      calibration_manager.SetCVCalibration(cv_index, min_val, max_val);
      cv_inputs.SetCalibration(&calibration_manager.GetCalibration());
    };

    // on_calibration_save: Save calibration to SD
    cb.on_calibration_save = [this]() {
      hw.StopAudio();
      bool success = calibration_manager.Save();
      if (audio_callback) hw.StartAudio(audio_callback);
      ShowResult(success, "Calibration", "Save failed");
    };

    // on_calibration_reset_all: Reset all CVs to default calibration
    cb.on_calibration_reset_all = [this]() {
      calibration_manager.ResetAll();
      cv_inputs.SetCalibration(&calibration_manager.GetCalibration());
      ShowMessage("Reset", "Defaults loaded");
    };

    //=========================================================================
    // UTILITY CALLBACKS
    //=========================================================================

    // on_display_message: Show a message (used for errors, etc.)
    cb.on_display_message = [this](const char *title, const char *msg) {
      ShowMessage(title, msg);
    };

    // on_enter_sub: Called when entering a SUB menu (usually no-op)
    // Override in port if you need custom behavior when entering submenus
    cb.on_enter_sub = [](Parameter *) {};

    return cb;
  }
  
  //===========================================================================
  // GetUserDataFileName - Callback for file browser display
  //===========================================================================
  // Returns the filename at the given index for the RenderFileBrowser function.
  // Index 0 is typically "Default", indices 1+ are actual files.
  //===========================================================================
  const char* GetUserDataFileName(int index) const {
    if (index >= 0 && index < user_data_file_count) {
      return user_data_files[index];
    }
    return nullptr;
  }

private:

  //===========================================================================
  // Helper: Show message with standard delay
  //===========================================================================
  void ShowMessage(const char *title, const char *msg) {
    display.RenderMessage(title, msg);
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  }

  //===========================================================================
  // Helper: Show success/error result
  //===========================================================================
  void ShowResult(bool success, const char *success_msg,
                  const char *fail_msg) {
    display.RenderMessage(success ? "Success" : "Error",
                          success ? success_msg : fail_msg);
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  }

  //===========================================================================
  // Helper: Reload user data from parameter state
  //===========================================================================
  // After loading a preset, USER_DATA parameters contain filenames.
  // This helper iterates through them and loads the corresponding files.
  //
  // This is called automatically by on_load_preset.
  //===========================================================================
  void ReloadUserDataFromParams() {
    auto *udm = module.GetUserDataManager();
    if (!udm)
      return;

    auto *params = module.GetParameters();
    for (size_t i = 0; i < module.GetParameterCount(); i++) {
      // USER_DATA params are always inside SUB menus
      if (params[i].type == ParamType::SUB && params[i].children) {
        for (int c = 0; c < params[i].child_count; c++) {
          auto &child = params[i].children[c];
          if (child.type == ParamType::USER_DATA) {
            int target = child.user_data_target;
            if (child.user_data_filename[0]) {
              udm->LoadTarget(target, child.user_data_filename);
            } else {
              udm->LoadDefaultForTarget(target);
            }
          }
        }
      }
    }
  }
};

} // namespace mutables_ui
