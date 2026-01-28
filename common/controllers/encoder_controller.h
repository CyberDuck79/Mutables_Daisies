#pragma once

#include "../cv_mapping_processor.h"
#include "../parameter.h"
#include "../ui_state.h"
#include <algorithm>
#include <functional>

namespace mutables_ui {

struct EncoderHardwareState {
  int increment;           // Delta Since last update
  bool pressed;            // Current button state
  bool rising_edge;        // Button just pressed
  uint32_t press_duration; // Time since press start (if pressed/released)
  uint32_t current_time_ms; // Current system time in ms (for timed captures)
};

struct EncoderCallbacks {
  std::function<void()> on_save_preset;
  std::function<void(int)> on_load_preset; // arg: count of presets
  std::function<void(Parameter *)> on_enter_sub;
  std::function<void(int)> on_user_data_browser; // arg: file count
  std::function<void(Parameter *, int)>
      on_user_data_selected; // param, file_index
  std::function<void(const char *, const char *)>
      on_display_message;               // title, message
  std::function<int()> on_scan_presets; // Returns count
  
  // Calibration callbacks
  std::function<float(int)> on_get_raw_cv;           // Get raw CV value for index
  std::function<void(int, float, float)> on_calibration_complete; // cv_index, min, max
  std::function<void()> on_calibration_save;         // Save calibration to SD
  std::function<void()> on_calibration_reset_all;    // Reset all to defaults
};

class EncoderController {
public:
  EncoderController(CVMappingProcessor &cv_processor)
      : cv_processor_(cv_processor), long_press_threshold_ms_(600) {}

  void SetCallbacks(const EncoderCallbacks &callbacks) {
    callbacks_ = callbacks;
  }

  void Update(const EncoderHardwareState &hw, MenuState &menu,
              Parameter *params, size_t param_count) {
    // Cache param count for handlers
    param_count_ = param_count;

    // Detect short vs long press on release
    bool short_press = false;
    bool long_press_detected = false;

    // Simple press detection logic (assumed to be driven by caller's timing or
    // passing duration) For this implementation, we expect 'press_duration' to
    // be valid on Release (rising_edge of release? No, usually falling edge of
    // button) Let's assume the caller handles the "Button Released" event and
    // passes us the duration. Wait, main.cpp logic was: !encoder_held &&
    // encoder_button_last -> released. We'll rely on the caller to tell us if a
    // "Click" (Short Press) or "Long Press" happened? Or we can track it
    // internaly if we get 'pressed' state every frame. Let's stick to the
    // main.cpp logic but encapsulated here if possible, OR simply accept
    // "short_press" and "long_press" flags in a stricter input struct. To be
    // most flexible and reusable, let's process the raw flags provided in the
    // struct.

    // Actually, let's simplify the input. The caller (hardware layer) usually
    // debounces. We will assume the input struct contains "events" this frame:

    // Let's refine EncoderHardwareState usage:
    // increment: rotary steps
    // pressed: is currently held? (for continuous checks)
    // rising_edge: just pressed down?
    // falling_edge: just released? (we might need this for press duration
    // check) press_duration: accumulated time if held?

    // Re-reading main.cpp:
    // if (!encoder_held && encoder_button_last) -> calculate duration -> set
    // long/short

    // So we need internal state to track 'last_pressed'.

    // But for pure logic extraction, it's better if we just get "ShortPress"
    // and "LongPress" events? Let's stick to the implementation plan which
    // implies we handle the logic. I'll add 'last_pressed_' member.

    bool just_released = !hw.pressed && last_pressed_;

    if (hw.rising_edge) {
      press_start_time_ = 0; // Caller might effectively handle time.
      // Actually, without System::GetNow(), we can't track real time unless
      // passed in. Let's assume hw.press_duration is passed in CORRECTLY by the
      // caller when released.
    }

    if (just_released) {
      if (hw.press_duration >= long_press_threshold_ms_) {
        long_press_detected = true;
      } else {
        short_press = true;
      }
    }

    last_pressed_ = hw.pressed;

    // Dispatch based on state
    switch (menu.state) {
    case UIState::Navigate:
      HandleNavigate(hw.increment, short_press, long_press_detected, menu,
                     params, param_count);
      break;
    case UIState::EditValue:
      HandleEdit(hw.increment, short_press, long_press_detected, menu, params);
      break;
    case UIState::Submenu:
      HandleSubmenu(hw.increment, short_press, long_press_detected, menu,
                    params);
      break;
    case UIState::SubmenuEdit:
      HandleSubmenuEdit(hw.increment, short_press, long_press_detected, menu,
                        params);
      break;
    case UIState::CharInput:
      HandleCharInput(hw.increment, short_press, long_press_detected, menu);
      break;
    case UIState::PresetList:
      HandlePresetList(hw.increment, short_press, menu);
      break;
    case UIState::FileBrowser:
      HandleFileBrowser(hw.increment, short_press, menu, params);
      break;
    case UIState::Calibration:
      HandleCalibration(hw, short_press, long_press_detected, menu);
      break;
    }
  }

private:
  CVMappingProcessor &cv_processor_;
  EncoderCallbacks callbacks_;
  bool last_pressed_ = false;
  uint32_t press_start_time_ = 0;
  const uint32_t long_press_threshold_ms_;
  size_t param_count_ = 0;

  // Helpers to get active params (root or sub)
  Parameter *GetActiveParams(MenuState &menu, Parameter *root_params) {
    if (menu.IsInSub() && menu.sub_parent) {
      return menu.sub_parent->children;
    }
    return root_params;
  }

  void HandleNavigate(int inc, bool short_press, bool long_press,
                      MenuState &menu, Parameter *params, size_t param_count) {
    if (menu.IsInSub()) {
      if (inc > 0)
        menu.NextSubChild();
      if (inc < 0)
        menu.PrevSubChild();
      menu.selected_param = menu.sub_child_selected;
    } else {
      if (inc > 0)
        menu.NextParam();
      if (inc < 0)
        menu.PrevParam();
    }

    Parameter *current_params = GetActiveParams(menu, params);
    // Safety check: if invalid index AND NOT Title (which is -1)
    bool title_selected = menu.IsInSub() && menu.IsSubTitleSelected();

    if (menu.selected_param < 0 && !title_selected)
      return; // Should be handled by Next/Prev, but just in case

    // Note: If title_selected is true, menu.selected_param IS -1 (or should be
    // treated as valid for exit)

    // Special case: Title selected in Sub (index -1 in sub_child_selected)
    // If IsSubTitleSelected, we are technically not on a param for editing
    // purposes.

    if (short_press) {
      if (title_selected) {
        // Exit SUB
        int parent_idx = menu.sub_parent_index;
        menu.ExitSub();
        menu.param_count =
            static_cast<int>(param_count); // Restore root count // This might
                                           // be tricky if not passed
        // Actually param_count passed to Update SHOULD be root count.
        // But menu.param_count tracks current view count.
        // We need to restore it.
        menu.selected_param = (parent_idx >= 0) ? parent_idx : 0;
        menu.ScrollToSelected();
      } else {
        auto &param = current_params[menu.selected_param];
        if (param.IsEditable()) {
          menu.state = UIState::EditValue;
        } else if (param.type == ParamType::SAVE) {
          // Enter Char Input Mode
          // We just transition state. The caller should implement
          // 'on_save_preset' which is called ONLY after char input
          // confirmation. However, we need to ensure SD is available? Ideally
          // we check before entering. But we don't have IsSDAvailable here.
          // We'll enter gracefully. If save fails later, it fails.
          // Or we could have a "CanSave" callback?
          // Let's assume we can enter.
          menu.EnterCharInput();
        } else if (param.type == ParamType::LOAD) {
          if (callbacks_.on_scan_presets) {
            int count = callbacks_.on_scan_presets();
            if (count > 0) {
              menu.EnterPresetList(count);
            } else {
              if (callbacks_.on_display_message)
                callbacks_.on_display_message("Error", "No presets");
            }
          }
        } else if (param.type == ParamType::SUB) {
          if (param.children && param.child_count > 0) {
            menu.EnterSub(&param, menu.selected_param);
            menu.param_count = param.child_count;
            menu.selected_param =
                menu.sub_child_selected >= 0 ? menu.sub_child_selected : 0;
          }
        } else if (param.type == ParamType::USER_DATA) {
          if (callbacks_.on_user_data_browser)
            callbacks_.on_user_data_browser(param.user_data_target);
        } else if (param.type == ParamType::CALIBRATION) {
          // Enter calibration mode
          menu.EnterCalibration();
        }
      }
    } else if (long_press) {
      if (title_selected)
        return; // No mapping on title

      auto &param = current_params[menu.selected_param];
      if (param.HasMapping()) {
        // Enter mapping submenu
        // Logic from main.cpp:
        menu.EnterSubmenu(menu.selected_param, param.type, param.mapping);
      }
    }
  }

  void HandleEdit(int inc, bool short_press, bool long_press, MenuState &menu,
                  Parameter *params) {
    // ... implementation ...
    Parameter *current_params = GetActiveParams(menu, params);
    auto &param = current_params[menu.selected_param];

    if (inc != 0) {
      bool is_cv_plugged = param.mapping.IsCVSource() && param.mapping.plugged;
      bool is_cc_mapped = param.mapping.source == MappingSource::CC;

      if (!is_cv_plugged && !is_cc_mapped) {
        float step = 0.01f;
        if (param.type == ParamType::ENUM || param.type == ParamType::MIDI) {
          step = 1.0f;
        }
        // Handle different Knob steps? main.cpp used fixed 0.01 or whatever.
        // main.cpp: 0.01f

        param.value += inc * step;
        param.value = std::clamp(param.value, param.min, param.max);
      }
    }

    if (short_press || long_press) {
      menu.state = UIState::Navigate;
    }
  }

  void HandleSubmenu(int inc, bool short_press, bool long_press,
                     MenuState &menu, Parameter *params) {
    Parameter *current_params = GetActiveParams(menu, params);
    auto &param = current_params[menu.submenu_param_index];

    if (inc > 0)
      menu.NextSubmenuItem(param.type, param.mapping);
    if (inc < 0)
      menu.PrevSubmenuItem(param.type, param.mapping);

    if (short_press) {
      int item = menu.submenu_selected_item;
      // Check for Plugged toggle (Item 2 for Knob/Enum)
      if ((param.type == ParamType::KNOB || param.type == ParamType::ENUM) &&
          item == 2 && param.mapping.IsCVSource()) {

        param.mapping.plugged = !param.mapping.plugged;
        if (param.mapping.plugged) {
          // We need access to CV inputs to sample current value?
          // Used GetLastCV from processor
          int cv_idx = param.mapping.GetCVIndex();
          if (cv_idx >= 0) {
            param.mapping.offset = cv_processor_.GetLastCV(cv_idx);
            // Initialize held_cv for S&H params to avoid jump
            if (param.sample_and_hold) {
              // S&H of Index: Calculate initial index using Offset (Delta=0)
              int initial_idx = CVMappingProcessor::CalculateEnumFromCV(
                  param, param.mapping.offset);
              param.held_cv = static_cast<float>(initial_idx);
            }
          }
          // We don't have cv_inputs here directly yet.
          // We passed cv_processor_ in constructor, maybe it can provide it?
          // Or we just skip the "Snapping" for now or add a method to
          // cv_processor. Let's assume cv_processor has GetCVValue(index) or
          // similar. The CVMappingProcessor from Phase 1/2 doesn't OWN the
          // inputs, it processes them. But it receives them in Process().
          // Ideally we shouldn't couple UI to Hardware ADC reads directly
          // inside this controller logic. Maybe we can ignore the "Offset Snap"
          // feature for this refactor, or callback? Let's leave offset as is
          // for now.
        }
        cv_processor_.RebuildCache(
            params,
            param_count_); // Update signature has param_count.
      } else {
        menu.state = UIState::SubmenuEdit;
      }
    }

    if (long_press) {
      menu.ExitSubmenu();
    }
  }

  void HandleSubmenuEdit(int inc, bool short_press, bool long_press,
                         MenuState &menu, Parameter *params) {
    Parameter *current_params = GetActiveParams(menu, params);
    auto &param = current_params[menu.submenu_param_index];
    int item = menu.submenu_selected_item;

    if (inc != 0) {
      if (item == 0) { // Mapping Source
        // Helper was: encoder_handlers::CycleMappingSource(param, inc);
        // We extracted logic to cv_processor?
        // cv_processor.CycleMappingSource(param, inc);
        cv_processor_.CycleMappingSource(param, inc);
        cv_processor_.RebuildCache(params, param_count_);
      } else {
        // ... Handle offsets, attenuverters, CC numbers ...
        // Implementation similar to main.cpp switch cases
        if (item == 1 && param.mapping.source == MappingSource::CC) {
          param.mapping.cc_number =
              std::clamp(param.mapping.cc_number + inc, 1, 127);
          cv_processor_.RebuildCache(params, param_count_);
        }
        // ... etc for Attenuverter (3), Velocity (4)
        if (item == 3) {
          // Attenuverter (always editable)
          // Was: && (param.mapping.IsCVSource() || param.mapping.source ==
          // MappingSource::CC)
          param.mapping.attenuverter =
              std::clamp(param.mapping.attenuverter + inc * 0.05f, -1.0f, 1.0f);
        }
        if (item == 4 && param.type == ParamType::KNOB) { // Velocity
          param.mapping.velocity_amount = std::clamp(
              param.mapping.velocity_amount + inc * 0.05f, -1.0f, 1.0f);
        }
        if (item == 4 && param.type == ParamType::ENUM &&
            param.mapping.IsGateSource()) { // Trigger Mode
          int t = static_cast<int>(param.mapping.trigger);
          t = std::clamp(t + inc, 0, 2);
          param.mapping.trigger = static_cast<TriggerMode>(t);
        }
        if (item == 5 && param.mapping.IsGateSource()) { // Action
          int max_act =
              (param.mapping.trigger == TriggerMode::RISE_AND_FALL) ? 3 : 1;
          int a = static_cast<int>(param.mapping.action);
          a = std::clamp(a + inc, 0, max_act);
          param.mapping.action = static_cast<EnumAction>(a);
        }
      }
    }

    if (short_press || long_press) {
      menu.state = UIState::Submenu;
    }
  }

  void HandleCharInput(int inc, bool short_press, bool long_press,
                       MenuState &menu) {
    if (inc > 0)
      menu.NextChar();
    if (inc < 0)
      menu.PrevChar();

    if (short_press) {
      if (menu.char_title_selected) {
        // Exit without saving
        menu.ExitCharInput();
      } else {
        bool cont = menu.ConfirmChar();
        if (!cont) {
          // Finished
          if (callbacks_.on_save_preset)
            callbacks_.on_save_preset(); // Actually execute save
          menu.ExitCharInput();          // Transitions to Navigate
        }
      }
    } else if (long_press) {
      // Fast Save
      if (callbacks_.on_save_preset)
        callbacks_.on_save_preset();
      menu.ExitCharInput();
    }
  }

  void HandlePresetList(int inc, bool short_press, MenuState &menu) {
    if (inc > 0)
      menu.NextPreset();
    if (inc < 0)
      menu.PrevPreset();

    if (short_press) {
      if (menu.preset_title_selected) {
        menu.ExitPresetList();
      } else {
        if (callbacks_.on_load_preset)
          callbacks_.on_load_preset(menu.GetSelectedPreset()); // Load specific
        menu.ExitPresetList();
      }
    }
  }

  void HandleFileBrowser(int inc, bool short_press, MenuState &menu,
                         Parameter *params) {
    if (inc > 0)
      menu.NextFile();
    if (inc < 0)
      menu.PrevFile();

    if (short_press) {
      // Assign file to parameter
      Parameter *current_params =
          GetActiveParams(menu, params); // Should be root for USER_DATA usually
      auto &param = current_params[menu.file_browser_param_idx];

      if (callbacks_.on_user_data_selected) {
        callbacks_.on_user_data_selected(&param, menu.GetSelectedFile());
      }
      menu.ExitFileBrowser();
    }
  }

  void HandleCalibration(const EncoderHardwareState &hw, bool short_press, bool long_press,
                         MenuState &menu) {
    int inc = hw.increment;
    
    switch (menu.calibration_step) {
    case CalibrationStep::SelectCV:
      // Navigate menu
      if (inc > 0) menu.NextCalibrationItem();
      if (inc < 0) menu.PrevCalibrationItem();
      
      if (short_press) {
        // -1 = title/back, exit calibration
        if (menu.calibration_selected == -1) {
          menu.ExitCalibration();
          break;
        }
        
        CalibrationMenuItem item = static_cast<CalibrationMenuItem>(menu.calibration_selected);
        switch (item) {
        case CalibrationMenuItem::CV1:
        case CalibrationMenuItem::CV2:
        case CalibrationMenuItem::CV3:
        case CalibrationMenuItem::CV4:
          // Start calibrating this CV
          menu.StartCVCalibration(menu.calibration_selected);
          break;
        case CalibrationMenuItem::ResetAll:
          // Reset all calibrations to default
          if (callbacks_.on_calibration_reset_all) {
            callbacks_.on_calibration_reset_all();
          }
          break;
        case CalibrationMenuItem::Save:
          // Save calibration to SD
          if (callbacks_.on_calibration_save) {
            callbacks_.on_calibration_save();
          }
          menu.ExitCalibration();
          break;
        default:
          break;
        }
      }
      
      if (long_press) {
        // Long press exits calibration mode
        menu.ExitCalibration();
      }
      break;
      
    case CalibrationStep::CaptureMin:
      // If capture is active, update it continuously
      if (menu.IsCaptureActive()) {
        float raw_cv = 0.0f;
        if (callbacks_.on_get_raw_cv) {
          raw_cv = callbacks_.on_get_raw_cv(menu.calibration_cv_index);
        }
        // UpdateCapture will transition to CaptureMax when done
        menu.UpdateCapture(raw_cv, hw.current_time_ms);
      } else if (short_press) {
        // Short press starts timed capture
        menu.StartCaptureMin(hw.current_time_ms);
      }
      
      // Long press cancels and returns to menu
      if (long_press) {
        menu.calibration_step = CalibrationStep::SelectCV;
        menu.calibration_capture_active = false;
      }
      break;
      
    case CalibrationStep::CaptureMax:
      // If capture is active, update it continuously
      if (menu.IsCaptureActive()) {
        float raw_cv = 0.0f;
        if (callbacks_.on_get_raw_cv) {
          raw_cv = callbacks_.on_get_raw_cv(menu.calibration_cv_index);
        }
        // UpdateCapture will transition to Confirm when done
        menu.UpdateCapture(raw_cv, hw.current_time_ms);
      } else if (short_press) {
        // Short press starts timed capture
        menu.StartCaptureMax(hw.current_time_ms);
      }
      
      // Long press cancels and returns to menu
      if (long_press) {
        menu.calibration_step = CalibrationStep::SelectCV;
        menu.calibration_capture_active = false;
      }
      break;
      
    case CalibrationStep::Confirm:
      if (short_press) {
        // Confirm and apply calibration
        if (callbacks_.on_calibration_complete) {
          callbacks_.on_calibration_complete(
            menu.calibration_cv_index,
            menu.calibration_captured_min,
            menu.calibration_captured_max
          );
        }
        menu.ConfirmCalibration();
      }
      
      if (inc != 0) {
        // Any rotation = retry
        menu.RetryCalibration();
      }
      
      if (long_press) {
        // Long press cancels and returns to menu
        menu.calibration_step = CalibrationStep::SelectCV;
      }
      break;
    }
  }
};

} // namespace mutables_ui
