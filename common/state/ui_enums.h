#pragma once

#include <cstdint>

namespace mutables_ui {

// Main UI states
enum class UIState {
    Navigate,       // Encoder rotation scrolls parameters
    EditValue,      // Encoder rotation changes value
    Submenu,        // In submenu (Navigate mode)
    SubmenuEdit,    // Editing submenu values
    CharInput,      // Typing preset name for Save
    PresetList,     // Browsing presets for Load
    FileBrowser,    // Browsing files for USER_DATA selection
    Calibration     // CV input calibration mode
};

// Submenu items for KNOB type (no Back - long press to exit)
enum class KnobSubmenuItem {
    Mapping,        // Select source: None, CV1-4, CC
    CCNumber,       // CC number 1-127 (only if CC mapped)
    Plugged,        // Toggle + captures offset when enabled (only if CV mapped)
    Attenuverter,   // -100% to +100%
    Velocity        // -100% to +100%
};

// Submenu items for CV type (no Back - long press to exit)
enum class CVSubmenuItem {
    Mapping         // Select source: None, CV1-4
};

// Submenu items for ENUM type (no Back - long press to exit)
enum class EnumSubmenuItem {
    Mapping,        // Select source: None, Gate1-2, CV1-4, CC
    CCNumber,       // CC number 1-127 (only if CC mapped)
    Attenuverter,   // -100% to +100% (only if CV or CC mapped)
    Trigger,        // rise, fall, both (only if Gate mapped)
    Action          // ++, -- (always), +-, -+ (only if trigger=both)
};

// Display settings
static constexpr int VISIBLE_PARAMS = 4;
static constexpr int VISIBLE_SUBMENU_ITEMS = 4;

// Character set for preset name input
static constexpr const char* kCharSet = "abcdefghijklmnopqrstuvwxyz0123456789-_. ";
static constexpr int kCharSetSize = 40;
static constexpr int MAX_PRESET_NAME_LEN = 16;

} // namespace mutables_ui
