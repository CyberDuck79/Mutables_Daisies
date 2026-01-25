#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace mutables_ui {

// Parameter types per new specification
enum class ParamType {
  KNOB,      // Continuous with CV/CC mapping, attenuverter, velocity
  CV,        // Direct CV input (read-only, no attenuverter emulation)
  ENUM,      // Discrete selection with Gate/CV/CC mapping
  MIDI,      // MIDI channel selection (1-16)
  SUB,       // Submenu container
  SAVE,      // Preset save action
  LOAD,      // Preset load action
  USER_DATA, // User data file selector (shows filename, triggers file browser)
  BACK       // Back button to exit SUB submenu
};

// Mapping source options
enum class MappingSource {
  NONE,
  CV1,
  CV2,
  CV3,
  CV4,
  GATE1,
  GATE2,
  CC // MIDI CC (cc_number specifies which)
};

// Gate trigger modes for ENUM
enum class TriggerMode {
  RISE,         // Rising edge only
  FALL,         // Falling edge only
  RISE_AND_FALL // Both edges
};

// Gate actions for ENUM
enum class EnumAction {
  INCREMENT,   // ++
  DECREMENT,   // --
  TOGGLE_PLUS, // +- (increment on first trigger, decrement on second)
  TOGGLE_MINUS // -+ (decrement on first trigger, increment on second)
};

// Mapping configuration for all mappable parameters
struct MappingConfig {
  MappingSource source;
  int cc_number; // CC number if source is CC (1-127)

  // For KNOB type
  bool plugged;       // When true, offset is captured and attenuverter active
  float offset;       // Captured knob position when plugged enabled
  float attenuverter; // -1.0 to +1.0 - scales CV around offset
  float velocity_amount; // -1.0 to +1.0 - adds velocity modulation

  // For ENUM with Gate mapping
  TriggerMode trigger;
  EnumAction action;
  bool last_gate_state; // For edge detection
  bool toggle_state;    // For +- / -+ actions

  MappingConfig()
      : source(MappingSource::NONE), cc_number(1), plugged(false), offset(0.5f),
        attenuverter(0.0f), velocity_amount(0.0f), trigger(TriggerMode::RISE),
        action(EnumAction::INCREMENT), last_gate_state(false),
        toggle_state(false) {}

  // Helper to check if source is a CV input
  bool IsCVSource() const {
    return source >= MappingSource::CV1 && source <= MappingSource::CV4;
  }

  // Helper to check if source is a Gate input
  bool IsGateSource() const {
    return source == MappingSource::GATE1 || source == MappingSource::GATE2;
  }

  // Get CV index (0-3) or -1 if not CV source
  int GetCVIndex() const {
    if (!IsCVSource())
      return -1;
    return static_cast<int>(source) - static_cast<int>(MappingSource::CV1);
  }

  // Get Gate index (0-1) or -1 if not Gate source
  int GetGateIndex() const {
    if (!IsGateSource())
      return -1;
    return static_cast<int>(source) - static_cast<int>(MappingSource::GATE1);
  }
};

// Callback types for dynamic parameter behavior
// siblings is the array of sibling parameters, sibling_count is the number of
// siblings param_index is the index of the current parameter in the siblings
// array
typedef bool (*VisibilityCallback)(const struct Parameter *siblings,
                                   uint8_t sibling_count, uint8_t param_index);
typedef void (*ValueFormatCallback)(const struct Parameter *param,
                                    const struct Parameter *siblings,
                                    uint8_t sibling_count, uint8_t param_index,
                                    char *buffer, size_t buffer_size);

struct Parameter {
  const char *name;
  ParamType type;
  float value;
  float min;
  float max;
  MappingConfig mapping;

  // For ENUM type
  const char **enum_labels;
  uint8_t enum_count;

  // For SUB type
  Parameter *children;
  uint8_t child_count;

  // For USER_DATA type
  uint8_t user_data_target;    // Target index (0-4 for plaits)
  char user_data_filename[32]; // Current filename or empty for firmware default

  // For CV mapping (e.g. Bank/Engine)
  bool sample_and_hold;
  float held_cv; // Last sampled CV value

  // Dynamic visibility and formatting callbacks
  VisibilityCallback visibility_callback;
  ValueFormatCallback format_callback;

  // Default constructor
  Parameter()
      : name(""), type(ParamType::KNOB), value(0.0f), min(0.0f), max(1.0f),
        enum_labels(nullptr), enum_count(0), children(nullptr), child_count(0),
        user_data_target(0), sample_and_hold(false), held_cv(0.0f),
        visibility_callback(nullptr), format_callback(nullptr) {
    user_data_filename[0] = '\0';
  }

  // KNOB constructor
  static Parameter Knob(const char *name, float min = 0.0f, float max = 1.0f,
                        float default_value = 0.5f) {
    Parameter p;
    p.name = name;
    p.type = ParamType::KNOB;
    p.value = default_value;
    p.min = min;
    p.max = max;
    return p;
  }

  // CV constructor (read-only)
  static Parameter CV(const char *name) {
    Parameter p;
    p.name = name;
    p.type = ParamType::CV;
    p.value = 0.0f;
    p.min = 0.0f;
    p.max = 1.0f;
    return p;
  }

  // ENUM constructor
  static Parameter Enum(const char *name, const char **labels, uint8_t count,
                        uint8_t default_index = 0) {
    Parameter p;
    p.name = name;
    p.type = ParamType::ENUM;
    p.value = static_cast<float>(default_index);
    p.min = 0.0f;
    p.max = static_cast<float>(count - 1);
    p.enum_labels = labels;
    p.enum_count = count;
    return p;
  }

  // MIDI channel constructor
  static Parameter MidiChannel(const char *name = "MIDI Ch") {
    Parameter p;
    p.name = name;
    p.type = ParamType::MIDI;
    p.value = 0.0f; // Channel 1
    p.min = 0.0f;
    p.max = 15.0f; // Channels 1-16
    return p;
  }

  // SUB constructor
  static Parameter Sub(const char *name, Parameter *children, uint8_t count) {
    Parameter p;
    p.name = name;
    p.type = ParamType::SUB;
    p.children = children;
    p.child_count = count;
    return p;
  }

  // SAVE constructor
  static Parameter Save() {
    Parameter p;
    p.name = "Save";
    p.type = ParamType::SAVE;
    return p;
  }

  // LOAD constructor
  static Parameter Load() {
    Parameter p;
    p.name = "Load";
    p.type = ParamType::LOAD;
    return p;
  }

  // BACK constructor (for exiting SUB submenus)
  static Parameter Back() {
    Parameter p;
    p.name = "Back";
    p.type = ParamType::BACK;
    return p;
  }

  // USER_DATA constructor
  static Parameter UserData(const char *name, uint8_t target) {
    Parameter p;
    p.name = name;
    p.type = ParamType::USER_DATA;
    p.user_data_target = target;
    p.user_data_filename[0] = '\0'; // Empty = use firmware default
    return p;
  }

  // Set user data filename (or empty string for firmware default)
  void SetUserDataFile(const char *filename) {
    if (filename && filename[0]) {
      strncpy(user_data_filename, filename, sizeof(user_data_filename) - 1);
      user_data_filename[sizeof(user_data_filename) - 1] = '\0';
    } else {
      user_data_filename[0] = '\0';
    }
  }

  // Get display label for user data (filename or "firmware")
  const char *GetUserDataLabel() const {
    return user_data_filename[0] ? user_data_filename : "firmware";
  }

  // Check if using firmware default
  bool IsUserDataFirmwareDefault() const {
    return user_data_filename[0] == '\0';
  }

  // Get normalized value (0.0 to 1.0)
  float GetNormalized() const {
    if (max == min)
      return 0.0f;
    return (value - min) / (max - min);
  }

  // Set from normalized value (0.0 to 1.0)
  void SetNormalized(float normalized) {
    value = min + normalized * (max - min);
    value = std::clamp(value, min, max);
  }

  // Set from normalized value with hysteresis to prevent jitter
  bool SetNormalizedWithHysteresis(float normalized, float tolerance = 0.005f) {
    float new_value = min + normalized * (max - min);
    new_value = std::clamp(new_value, min, max);

    float diff = std::abs(new_value - value);
    float range = max - min;
    if (diff > range * tolerance) {
      value = new_value;
      return true;
    }
    return false;
  }

  // Get integer index for ENUM/MIDI params
  int GetIndex() const { return static_cast<int>(value + 0.5f); }

  // Set index for ENUM/MIDI params
  void SetIndex(int index) {
    value = std::clamp(static_cast<float>(index), min, max);
  }

  // Get enum label
  const char *GetEnumLabel() const {
    if (type == ParamType::ENUM && enum_labels) {
      int idx = GetIndex();
      if (idx >= 0 && idx < enum_count) {
        return enum_labels[idx];
      }
    }
    return "";
  }

  // Check if parameter has submenu (KNOB, CV, ENUM have mapping submenus)
  bool HasSubmenu() const {
    return type == ParamType::KNOB || type == ParamType::CV ||
           type == ParamType::ENUM || type == ParamType::SUB;
  }

  // Check if value is editable (not CV, SAVE, LOAD)
  bool IsEditable() const {
    return type == ParamType::KNOB || type == ParamType::ENUM ||
           type == ParamType::MIDI;
  }

  // Check if has mapping options
  bool HasMapping() const {
    return type == ParamType::KNOB || type == ParamType::CV ||
           type == ParamType::ENUM;
  }

  // Check if parameter is visible (uses callback if set)
  bool IsVisible(const Parameter *siblings, uint8_t sibling_count,
                 uint8_t param_index) const {
    if (visibility_callback) {
      return visibility_callback(siblings, sibling_count, param_index);
    }
    return true; // Visible by default
  }

  // Format value for display (uses callback if set)
  void FormatDisplayValue(const Parameter *siblings, uint8_t sibling_count,
                          uint8_t param_index, char *buffer,
                          size_t buffer_size) const {
    if (format_callback) {
      format_callback(this, siblings, sibling_count, param_index, buffer,
                      buffer_size);
    } else {
      // Default formatting
      if (type == ParamType::ENUM && enum_labels) {
        snprintf(buffer, buffer_size, "%.6s", GetEnumLabel());
      } else if (type == ParamType::USER_DATA) {
        // Show filename or "Default" if not set
        if (user_data_filename[0] != '\0') {
          snprintf(buffer, buffer_size, "%.7s", user_data_filename);
        } else {
          snprintf(buffer, buffer_size, "origin");
        }
      } else {
        int val_int = (int)(value * 100.0f);
        int whole = val_int / 100;
        int frac = val_int % 100;
        snprintf(buffer, buffer_size, "%d.%02d", whole, frac);
      }
    }
  }
};

} // namespace mutables_ui
