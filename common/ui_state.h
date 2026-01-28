#pragma once

#include "calibration.h"
#include "parameter.h"
#include "state/ui_enums.h" // Enums extracted to separate file
#include <cstdint>
#include <cstring>

namespace mutables_ui {

// Re-export enums and constants for backwards compatibility
// (they're now defined in state/ui_enums.h)

struct MenuState {
  // Re-export constants from ui_enums.h for backwards compatibility
  // (must be declared before use in array size)
  static constexpr const char *kCharSet = mutables_ui::kCharSet;
  static constexpr int kCharSetSize = mutables_ui::kCharSetSize;
  static constexpr int MAX_PRESET_NAME_LEN = mutables_ui::MAX_PRESET_NAME_LEN;
  static constexpr int VISIBLE_PARAMS = mutables_ui::VISIBLE_PARAMS;
  static constexpr int VISIBLE_SUBMENU_ITEMS =
      mutables_ui::VISIBLE_SUBMENU_ITEMS;

  // Main state
  UIState state;
  int selected_param;
  int param_count;
  int scroll_offset;

  // Submenu state
  int submenu_param_index;   // Which parameter's submenu we're in
  int submenu_selected_item; // Current submenu item index
  int submenu_scroll_offset; // For scrolling long submenus

  // For SUB type: track if we're inside a SUB's children
  Parameter *sub_parent; // Non-null if browsing SUB children
  int sub_parent_index; // Index of the SUB in root params (to return to correct
                        // position)
  int sub_child_selected; // Selected child in SUB (-1 = title/back selected)

  // For preset Save (CharInput state)
  char preset_name[MAX_PRESET_NAME_LEN + 1]; // Current name being typed
  int char_position;                         // Current cursor position (0-15)
  int char_index; // Index in character set for current position

  // For preset Load (PresetList state)
  int preset_selected;        // Selected preset in list
  int preset_scroll_offset;   // Scroll offset for preset list
  int preset_count;           // Total presets found
  bool preset_title_selected; // True if title bar is selected

  // For file browser (FileBrowser state for USER_DATA)
  int file_selected;          // Selected file in list
  int file_scroll_offset;     // Scroll offset for file list
  int file_count;             // Total files found
  int file_browser_param_idx; // Which USER_DATA parameter we're editing

  // For calibration (Calibration state)
  CalibrationStep calibration_step;
  CalibrationMenuItem calibration_selected;
  int calibration_cv_index;      // Which CV is being calibrated (0-3)
  float calibration_captured_min; // Captured min value
  float calibration_captured_max; // Captured max value

  MenuState()
      : state(UIState::Navigate), selected_param(0), param_count(0),
        scroll_offset(0), submenu_param_index(-1), submenu_selected_item(0),
        submenu_scroll_offset(0), sub_parent(nullptr), sub_parent_index(-1),
        sub_child_selected(0), char_position(0), char_index(0),
        preset_selected(0), preset_scroll_offset(0), preset_count(0),
        file_selected(0), file_scroll_offset(0), file_count(0),
        file_browser_param_idx(-1),
        calibration_step(CalibrationStep::SelectCV),
        calibration_selected(CalibrationMenuItem::CV1),
        calibration_cv_index(0),
        calibration_captured_min(0.0f),
        calibration_captured_max(1.0f) {
    preset_name[0] = '\0';
  }

  void ScrollToSelected() {
    if (selected_param < scroll_offset) {
      scroll_offset = selected_param;
    } else if (selected_param >= scroll_offset + VISIBLE_PARAMS) {
      scroll_offset = selected_param - VISIBLE_PARAMS + 1;
    }
  }

  void NextParam() {
    selected_param++;
    if (selected_param >= param_count) {
      selected_param = 0;
      scroll_offset = 0;
    } else {
      ScrollToSelected();
    }
  }

  void PrevParam() {
    selected_param--;
    if (selected_param < 0) {
      selected_param = param_count - 1;
      scroll_offset = selected_param - VISIBLE_PARAMS + 1;
      if (scroll_offset < 0)
        scroll_offset = 0;
    } else {
      ScrollToSelected();
    }
  }

  // Get number of submenu items for a parameter type
  int GetSubmenuItemCount(ParamType type, const MappingConfig &mapping) const {
    switch (type) {
    case ParamType::KNOB:
      return 5; // Mapping, CCNumber, Plugged, Attenuverter, Velocity
    case ParamType::CV:
      return 2; // Mapping, Plugged (read-only)
    case ParamType::ENUM:
      return 6; // Mapping, CCNumber, Plugged, Attenuverter, Trigger, Action
    default:
      return 0;
    }
  }

  // Check if submenu item is visible based on parameter state
  bool IsSubmenuItemVisible(ParamType type, int item_index,
                            const MappingConfig &mapping) const {
    if (type == ParamType::KNOB) {
      // CCNumber only visible if CC mapped
      if (item_index == 1)
        return mapping.source == MappingSource::CC;
      // Plugged only visible if CV mapped
      if (item_index == 2)
        return mapping.IsCVSource();
      // Velocity hidden for Frequency parameter (index 3) - not useful for
      // pitch
      if (item_index == 4 && submenu_param_index == 3)
        return false;
    } else if (type == ParamType::CV) {
      // Plugged only visible if CV mapped
      if (item_index == 1)
        return mapping.IsCVSource();
      // Plugged only visible if CV mapped
      if (item_index == 2)
        return mapping.IsCVSource();
    } else if (type == ParamType::ENUM) {
      // CCNumber only visible if CC mapped
      if (item_index == 1)
        return mapping.source == MappingSource::CC;
      // Plugged only visible if CV mapped
      if (item_index == 2)
        return mapping.IsCVSource();
      // Attenuverter always visible (user request)
      // if (item_index == 3) return ...

      // Trigger only visible if Gate mapped
      if (item_index == 4)
        return mapping.IsGateSource();
      // Action only visible if Gate mapped
      if (item_index == 5)
        return mapping.IsGateSource();
    }
    return true;
  }

  // Check if action value is valid for current trigger mode
  bool IsActionValidForTrigger(EnumAction action, TriggerMode trigger) const {
    // +- and -+ only make sense with RISE_AND_FALL trigger
    if (action == EnumAction::TOGGLE_PLUS ||
        action == EnumAction::TOGGLE_MINUS) {
      return trigger == TriggerMode::RISE_AND_FALL;
    }
    return true;
  }

  void EnterSubmenu(int param_index, ParamType type,
                    const MappingConfig &mapping) {
    (void)type;
    (void)mapping;
    submenu_param_index = param_index;
    submenu_selected_item = 0;
    submenu_scroll_offset = 0;
    state = UIState::Submenu;
  }

  void ExitSubmenu() {
    submenu_param_index = -1;
    submenu_selected_item = 0;
    submenu_scroll_offset = 0;
    state = UIState::Navigate;
  }

  void NextSubmenuItem(ParamType type, const MappingConfig &mapping) {
    int count = GetSubmenuItemCount(type, mapping);
    submenu_selected_item++;

    // Skip hidden items for ENUM
    while (submenu_selected_item < count &&
           !IsSubmenuItemVisible(type, submenu_selected_item, mapping)) {
      submenu_selected_item++;
    }

    if (submenu_selected_item >= count) {
      submenu_selected_item = 0;
    }

    // Update scroll
    if (submenu_selected_item < submenu_scroll_offset) {
      submenu_scroll_offset = submenu_selected_item;
    } else if (submenu_selected_item >=
               submenu_scroll_offset + VISIBLE_SUBMENU_ITEMS) {
      submenu_scroll_offset = submenu_selected_item - VISIBLE_SUBMENU_ITEMS + 1;
    }
  }

  void PrevSubmenuItem(ParamType type, const MappingConfig &mapping) {
    int count = GetSubmenuItemCount(type, mapping);
    submenu_selected_item--;

    // Skip hidden items for ENUM
    while (submenu_selected_item >= 0 &&
           !IsSubmenuItemVisible(type, submenu_selected_item, mapping)) {
      submenu_selected_item--;
    }

    if (submenu_selected_item < 0) {
      submenu_selected_item = count - 1;
      // Find last visible item
      while (!IsSubmenuItemVisible(type, submenu_selected_item, mapping) &&
             submenu_selected_item > 0) {
        submenu_selected_item--;
      }
    }

    // Update scroll
    if (submenu_selected_item < submenu_scroll_offset) {
      submenu_scroll_offset = submenu_selected_item;
    } else if (submenu_selected_item >=
               submenu_scroll_offset + VISIBLE_SUBMENU_ITEMS) {
      submenu_scroll_offset = submenu_selected_item - VISIBLE_SUBMENU_ITEMS + 1;
    }
  }

  bool IsInSubmenu() const {
    return state == UIState::Submenu || state == UIState::SubmenuEdit;
  }

  // Enter SUB parameter's children
  void EnterSub(Parameter *sub_param, int parent_index) {
    sub_parent = sub_param;
    sub_parent_index = parent_index;
    // Start with first visible child selected (not title)
    sub_child_selected = -1; // Will be advanced to first visible
    if (sub_param && sub_param->children) {
      for (int i = 0; i < sub_param->child_count; i++) {
        if (sub_param->children[i].IsVisible(sub_param->children,
                                             sub_param->child_count, i)) {
          sub_child_selected = i;
          break;
        }
      }
    }
    scroll_offset = 0; // Reset scroll when entering
                       // param_count will be updated by caller
  }

  // Exit SUB back to root
  void ExitSub() {
    sub_parent = nullptr;
    sub_parent_index = -1;
    sub_child_selected = 0;
    scroll_offset = 0;
  }

  // Check if title/back is selected in SUB
  bool IsSubTitleSelected() const {
    return sub_parent != nullptr && sub_child_selected == -1;
  }

  bool IsInSub() const { return sub_parent != nullptr; }

  // Navigate to next visible SUB child
  void NextSubChild() {
    if (!sub_parent || !sub_parent->children)
      return;

    int count = sub_parent->child_count;

    if (sub_child_selected == -1) {
      // Currently on title, move to first visible child
      for (int i = 0; i < count; i++) {
        if (sub_parent->children[i].IsVisible(sub_parent->children, count, i)) {
          sub_child_selected = i;
          UpdateSubScrollOffset();
          return;
        }
      }
      // No visible children, stay on title
      return;
    }

    int start = sub_child_selected;

    // Find next visible item
    do {
      sub_child_selected++;
      if (sub_child_selected >= count) {
        // Wrap to title
        sub_child_selected = -1;
        scroll_offset = 0;
        return;
      }
    } while (!sub_parent->children[sub_child_selected].IsVisible(
                 sub_parent->children, count, sub_child_selected) &&
             sub_child_selected != start);

    // Update scroll offset for visible items
    UpdateSubScrollOffset();
  }

  // Navigate to previous visible SUB child
  void PrevSubChild() {
    if (!sub_parent || !sub_parent->children)
      return;

    int count = sub_parent->child_count;

    if (sub_child_selected == -1) {
      // Currently on title, move to last visible child
      for (int i = count - 1; i >= 0; i--) {
        if (sub_parent->children[i].IsVisible(sub_parent->children, count, i)) {
          sub_child_selected = i;
          UpdateSubScrollOffset();
          return;
        }
      }
      // No visible children, stay on title
      return;
    }

    int start = sub_child_selected;

    // Find previous visible item
    do {
      sub_child_selected--;
      if (sub_child_selected < 0) {
        // Wrap to title
        sub_child_selected = -1;
        scroll_offset = 0;
        return;
      }
    } while (!sub_parent->children[sub_child_selected].IsVisible(
                 sub_parent->children, count, sub_child_selected) &&
             sub_child_selected != start);

    // Update scroll offset for visible items
    UpdateSubScrollOffset();
  }

  // Update scroll offset based on visible items
  void UpdateSubScrollOffset() {
    if (!sub_parent || !sub_parent->children)
      return;

    // Count visible items before current selection
    int visible_before = 0;
    for (int i = 0; i < sub_child_selected; i++) {
      if (sub_parent->children[i].IsVisible(sub_parent->children,
                                            sub_parent->child_count, i)) {
        visible_before++;
      }
    }

    // Adjust scroll offset based on visible position
    if (visible_before < scroll_offset) {
      scroll_offset = visible_before;
    } else if (visible_before >= scroll_offset + VISIBLE_PARAMS) {
      scroll_offset = visible_before - VISIBLE_PARAMS + 1;
    }
  }

  // Count visible children in current SUB
  int CountVisibleSubChildren() const {
    if (!sub_parent || !sub_parent->children)
      return 0;

    int count = 0;
    for (int i = 0; i < sub_parent->child_count; i++) {
      if (sub_parent->children[i].IsVisible(sub_parent->children,
                                            sub_parent->child_count, i)) {
        count++;
      }
    }
    return count;
  }

  // === Preset Save (CharInput) Methods ===

  // Enter character input mode for preset save
  void EnterCharInput() {
    state = UIState::CharInput;
    preset_name[0] = '\0';
    char_position = 0;
    char_index = 0;              // Start at 'a'
    char_title_selected = false; // Start on first character, not title
  }

  // Flag for title selection in CharInput
  bool char_title_selected;

  // Rotate through character set
  void NextChar() {
    if (char_title_selected) {
      // From title, go to first char
      char_title_selected = false;
    } else {
      if (char_index == kCharSetSize - 1) {
        // From last char (space), go to title
        char_title_selected = true;
      } else {
        char_index++;
      }
    }
  }

  void PrevChar() {
    if (char_title_selected) {
      // From title, go to last char (space)
      char_title_selected = false;
      char_index = kCharSetSize - 1;
    } else {
      if (char_index == 0) {
        // From first char, go to title
        char_title_selected = true;
      } else {
        char_index--;
      }
    }
  }

  // Get current character being selected
  char GetCurrentChar() const { return kCharSet[char_index]; }

  // Confirm current character and move to next position
  // Returns true if still editing, false if name complete (at max length)
  bool ConfirmChar() {
    char c = GetCurrentChar();

    if (c == ' ' && char_position > 0) {
      // Space at non-first position = backspace
      char_position--;
      preset_name[char_position] = '\0';
      // Set char_index to match the character we're now on (or 'a' if empty)
      if (char_position > 0) {
        // Find the index of the previous character
        for (int i = 0; i < kCharSetSize; i++) {
          if (kCharSet[i] == preset_name[char_position - 1]) {
            char_index = i;
            break;
          }
        }
      } else {
        char_index = 0;
      }
      return true;
    }

    if (c == ' ' && char_position == 0) {
      // Space at first position = exit without saving
      ExitCharInput();
      return false;
    }

    // Add character
    if (char_position < MAX_PRESET_NAME_LEN) {
      preset_name[char_position] = c;
      char_position++;
      preset_name[char_position] = '\0';
      char_index = 0; // Reset to 'a' for next char
    }

    return char_position < MAX_PRESET_NAME_LEN;
  }

  // Exit character input (back to Navigate)
  void ExitCharInput() {
    state = UIState::Navigate;
    char_position = 0;
    preset_name[0] = '\0';
  }

  // Check if preset name is valid (non-empty, or has current char)
  bool IsPresetNameValid() const {
    return char_position > 0 ||
           (!char_title_selected &&
            char_index != kCharSetSize - 1); // space is last
  }

  // Get the current preset name (without unconfirmed char)
  const char *GetPresetName() const { return preset_name; }

  // Get the final preset name including unconfirmed current character
  // Returns length of final name
  int GetFinalPresetName(char *buffer, size_t buffer_size) const {
    int len = strlen(preset_name);
    if (len < (int)buffer_size - 1) {
      strcpy(buffer, preset_name);
      // Add current character if it's not space and we're not on title
      if (!char_title_selected && char_position < MAX_PRESET_NAME_LEN) {
        char c = GetCurrentChar();
        if (c != ' ') {
          buffer[len] = c;
          buffer[len + 1] = '\0';
          len++;
        }
      }
    }
    return len;
  }

  // Check if final name (with current char) is valid
  bool IsFinalPresetNameValid() const {
    if (char_title_selected)
      return false;
    if (char_position > 0)
      return true;
    // Empty but has non-space current char
    char c = GetCurrentChar();
    return c != ' ';
  }

  // === Preset Load (PresetList) Methods ===

  // Enter preset list mode
  void EnterPresetList(int count) {
    state = UIState::PresetList;
    preset_count = count;
    preset_selected = 0;
    preset_scroll_offset = 0;
    preset_title_selected = false;
  }

  // Navigate preset list
  void NextPreset() {
    if (preset_title_selected) {
      // From title, go to first preset
      preset_title_selected = false;
      preset_selected = 0;
      ScrollPresetToSelected();
    } else if (preset_count == 0) {
      return;
    } else if (preset_selected == preset_count - 1) {
      // From last preset, go to title
      preset_title_selected = true;
    } else {
      preset_selected++;
      ScrollPresetToSelected();
    }
  }

  void PrevPreset() {
    if (preset_title_selected) {
      // From title, go to last preset
      preset_title_selected = false;
      preset_selected = preset_count > 0 ? preset_count - 1 : 0;
      ScrollPresetToSelected();
    } else if (preset_count == 0) {
      return;
    } else if (preset_selected == 0) {
      // From first preset, go to title
      preset_title_selected = true;
    } else {
      preset_selected--;
      ScrollPresetToSelected();
    }
  }

  void ScrollPresetToSelected() {
    if (preset_selected < preset_scroll_offset) {
      preset_scroll_offset = preset_selected;
    } else if (preset_selected >= preset_scroll_offset + VISIBLE_PARAMS) {
      preset_scroll_offset = preset_selected - VISIBLE_PARAMS + 1;
    }
  }

  // Exit preset list (back to Navigate)
  void ExitPresetList() {
    state = UIState::Navigate;
    preset_selected = 0;
    preset_count = 0;
  }

  // Get selected preset index
  int GetSelectedPreset() const { return preset_selected; }

  // === File Browser (for USER_DATA) Methods ===

  // Enter file browser mode for a USER_DATA parameter
  void EnterFileBrowser(int param_idx, int count) {
    state = UIState::FileBrowser;
    file_browser_param_idx = param_idx;
    file_count = count;
    file_selected = 0; // 0 = "Default" option
    file_scroll_offset = 0;
  }

  // Navigate file list
  void NextFile() {
    // file_count includes "Default" option at index 0
    file_selected =
        (file_selected + 1) % (file_count + 1); // +1 for Default option
    ScrollFileToSelected();
  }

  void PrevFile() {
    file_selected = (file_selected - 1 + file_count + 1) % (file_count + 1);
    ScrollFileToSelected();
  }

  void ScrollFileToSelected() {
    if (file_selected < file_scroll_offset) {
      file_scroll_offset = file_selected;
    } else if (file_selected >= file_scroll_offset + VISIBLE_PARAMS) {
      file_scroll_offset = file_selected - VISIBLE_PARAMS + 1;
    }
  }

  // Exit file browser (back to Navigate)
  void ExitFileBrowser() {
    state = UIState::Navigate;
    file_browser_param_idx = -1;
    file_selected = 0;
    file_count = 0;
  }

  // Get selected file index (0 = Default, 1+ = actual file indices)
  int GetSelectedFile() const { return file_selected; }

  // Check if "Default" is selected
  bool IsDefaultSelected() const { return file_selected == 0; }

  // === Calibration Methods ===

  // Enter calibration mode
  void EnterCalibration() {
    state = UIState::Calibration;
    calibration_step = CalibrationStep::SelectCV;
    calibration_selected = CalibrationMenuItem::CV1;
    calibration_cv_index = 0;
    calibration_captured_min = 0.0f;
    calibration_captured_max = 1.0f;
  }

  // Exit calibration mode (back to Navigate)
  void ExitCalibration() {
    state = UIState::Navigate;
    calibration_step = CalibrationStep::SelectCV;
  }

  // Navigate calibration menu
  void NextCalibrationItem() {
    int current = static_cast<int>(calibration_selected);
    current = (current + 1) % kCalibrationMenuItemCount;
    calibration_selected = static_cast<CalibrationMenuItem>(current);
  }

  void PrevCalibrationItem() {
    int current = static_cast<int>(calibration_selected);
    current = (current - 1 + kCalibrationMenuItemCount) % kCalibrationMenuItemCount;
    calibration_selected = static_cast<CalibrationMenuItem>(current);
  }

  // Start calibrating a specific CV
  void StartCVCalibration(int cv_index) {
    calibration_cv_index = cv_index;
    calibration_step = CalibrationStep::CaptureMin;
    calibration_captured_min = 0.0f;
    calibration_captured_max = 1.0f;
  }

  // Capture the current CV value as min
  void CaptureCalibrationMin(float raw_value) {
    calibration_captured_min = raw_value;
    calibration_step = CalibrationStep::CaptureMax;
  }

  // Capture the current CV value as max
  void CaptureCalibrationMax(float raw_value) {
    calibration_captured_max = raw_value;
    calibration_step = CalibrationStep::Confirm;
  }

  // Confirm calibration and return to menu
  void ConfirmCalibration() {
    calibration_step = CalibrationStep::SelectCV;
  }

  // Retry calibration (go back to CaptureMin)
  void RetryCalibration() {
    calibration_step = CalibrationStep::CaptureMin;
    calibration_captured_min = 0.0f;
    calibration_captured_max = 1.0f;
  }

  // Check if in calibration capture mode
  bool IsCalibrationCapturing() const {
    return state == UIState::Calibration && 
           (calibration_step == CalibrationStep::CaptureMin ||
            calibration_step == CalibrationStep::CaptureMax);
  }

  // Get the CV index being calibrated
  int GetCalibrationCVIndex() const { return calibration_cv_index; }

  // Get captured values
  float GetCapturedMin() const { return calibration_captured_min; }
  float GetCapturedMax() const { return calibration_captured_max; }
};

} // namespace mutables_ui
