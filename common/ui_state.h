#pragma once

#include <cstdint>
#include "parameter.h"

namespace mutables_ui {

enum class UIState {
    Navigate,       // Encoder rotation scrolls parameters
    EditValue,      // Encoder rotation changes value
    Submenu,        // In submenu (Navigate mode)
    SubmenuEdit     // Editing submenu values
};

// Submenu items for KNOB type
enum class KnobSubmenuItem {
    Mapping,        // Select source: None, CV1-4, CC1-127
    Plugged,        // Toggle + captures offset when enabled
    Attenuverter,   // -100% to +100%
    Velocity,       // -100% to +100%
    Back
};

// Submenu items for CV type
enum class CVSubmenuItem {
    Mapping,        // Select source: None, CV1-4
    Back
};

// Submenu items for ENUM type
enum class EnumSubmenuItem {
    Mapping,        // Select source: None, Gate1-2, CV1-4, CC1-127
    Trigger,        // rise, fall, rise & fall (only if Gate mapped)
    Action,         // ++, --, +-, -+ (only if Gate mapped)
    Back
};

struct MenuState {
    UIState state;
    int selected_param;
    int param_count;
    int scroll_offset;
    
    // Submenu state
    int submenu_param_index;      // Which parameter's submenu we're in
    int submenu_selected_item;    // Current submenu item index
    int submenu_scroll_offset;    // For scrolling long submenus
    
    // For SUB type: track if we're inside a SUB's children
    Parameter* sub_parent;        // Non-null if browsing SUB children
    int sub_child_selected;       // Selected child in SUB
    
    // Display settings
    static constexpr int VISIBLE_PARAMS = 4;
    static constexpr int VISIBLE_SUBMENU_ITEMS = 4;
    
    MenuState() 
        : state(UIState::Navigate)
        , selected_param(0)
        , param_count(0)
        , scroll_offset(0)
        , submenu_param_index(-1)
        , submenu_selected_item(0)
        , submenu_scroll_offset(0)
        , sub_parent(nullptr)
        , sub_child_selected(0) {}
    
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
            if (scroll_offset < 0) scroll_offset = 0;
        } else {
            ScrollToSelected();
        }
    }
    
    // Get number of submenu items for a parameter type
    int GetSubmenuItemCount(ParamType type, const MappingConfig& mapping) const {
        switch (type) {
            case ParamType::KNOB:
                return 5;  // Mapping, Plugged, Attenuverter, Velocity, Back
            case ParamType::CV:
                return 2;  // Mapping, Back
            case ParamType::ENUM:
                if (mapping.IsGateSource()) {
                    return 4;  // Mapping, Trigger, Action, Back
                }
                return 2;  // Mapping, Back
            default:
                return 1;  // Back only
        }
    }
    
    // Check if submenu item is visible based on parameter state
    bool IsSubmenuItemVisible(ParamType type, int item_index, const MappingConfig& mapping) const {
        if (type == ParamType::ENUM) {
            // Trigger and Action only visible if Gate mapped
            if (item_index == 1 || item_index == 2) {
                return mapping.IsGateSource();
            }
        }
        return true;
    }
    
    void EnterSubmenu(int param_index, ParamType type, const MappingConfig& mapping) {
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
    
    void NextSubmenuItem(ParamType type, const MappingConfig& mapping) {
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
        } else if (submenu_selected_item >= submenu_scroll_offset + VISIBLE_SUBMENU_ITEMS) {
            submenu_scroll_offset = submenu_selected_item - VISIBLE_SUBMENU_ITEMS + 1;
        }
    }
    
    void PrevSubmenuItem(ParamType type, const MappingConfig& mapping) {
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
        } else if (submenu_selected_item >= submenu_scroll_offset + VISIBLE_SUBMENU_ITEMS) {
            submenu_scroll_offset = submenu_selected_item - VISIBLE_SUBMENU_ITEMS + 1;
        }
    }
    
    bool IsInSubmenu() const {
        return state == UIState::Submenu || state == UIState::SubmenuEdit;
    }
    
    // Enter SUB parameter's children
    void EnterSub(Parameter* sub_param) {
        sub_parent = sub_param;
        sub_child_selected = 0;
        // param_count will be updated by caller
    }
    
    // Exit SUB back to root
    void ExitSub() {
        sub_parent = nullptr;
        sub_child_selected = 0;
    }
    
    bool IsInSub() const {
        return sub_parent != nullptr;
    }
};

} // namespace mutables_ui
