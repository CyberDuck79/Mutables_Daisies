#pragma once

#include "ui_enums.h"
#include "../parameter.h"

namespace mutables_ui {

// Handles submenu and SUB parameter navigation
struct SubmenuState {
    // Mapping submenu state (KNOB/CV/ENUM)
    int submenu_param_index = -1;      // Which parameter's submenu we're in
    int submenu_selected_item = 0;     // Current submenu item index
    int submenu_scroll_offset = 0;     // For scrolling long submenus
    
    // For SUB type: track if we're inside a SUB's children
    Parameter* sub_parent = nullptr;   // Non-null if browsing SUB children
    int sub_parent_index = -1;         // Index of the SUB in root params (to return to correct position)
    int sub_child_selected = 0;        // Selected child in SUB (-1 = title/back selected)
    int sub_scroll_offset = 0;         // Scroll offset within SUB
    
    // === Mapping Submenu Methods ===
    
    // Get number of submenu items for a parameter type
    int GetSubmenuItemCount(ParamType type, const MappingConfig& mapping) const {
        switch (type) {
            case ParamType::KNOB:
                return 5;  // Mapping, CCNumber, Plugged, Attenuverter, Velocity
            case ParamType::CV:
                return 2;  // Mapping, Plugged (read-only)
            case ParamType::ENUM:
                return 6;  // Mapping, CCNumber, Plugged, Attenuverter, Trigger, Action
            default:
                return 0;
        }
    }
    
    // Check if submenu item is visible based on parameter state
    bool IsSubmenuItemVisible(ParamType type, int item_index, const MappingConfig& mapping) const {
        if (type == ParamType::KNOB) {
            // CCNumber only visible if CC mapped
            if (item_index == 1) return mapping.source == MappingSource::CC;
            // Plugged only visible if CV mapped
            if (item_index == 2) return mapping.IsCVSource();
            // Velocity hidden for Frequency parameter (index 3) - not useful for pitch
            if (item_index == 4 && submenu_param_index == 3) return false;
        } else if (type == ParamType::CV) {
            // Plugged only visible if CV mapped
            if (item_index == 1) return mapping.IsCVSource();
            // Plugged only visible if CV mapped
            if (item_index == 2) return mapping.IsCVSource();
        } else if (type == ParamType::ENUM) {
            // CCNumber only visible if CC mapped
            if (item_index == 1) return mapping.source == MappingSource::CC;
            // Plugged only visible if CV mapped
            if (item_index == 2) return mapping.IsCVSource();
            // Attenuverter only visible if CV or CC mapped
            if (item_index == 3) return mapping.IsCVSource() || mapping.source == MappingSource::CC;
            // Trigger only visible if Gate mapped
            if (item_index == 4) return mapping.IsGateSource();
            // Action only visible if Gate mapped
            if (item_index == 5) return mapping.IsGateSource();
        }
        return true;
    }
    
    // Check if action value is valid for current trigger mode
    bool IsActionValidForTrigger(EnumAction action, TriggerMode trigger) const {
        // +- and -+ only make sense with RISE_AND_FALL trigger
        if (action == EnumAction::TOGGLE_PLUS || action == EnumAction::TOGGLE_MINUS) {
            return trigger == TriggerMode::RISE_AND_FALL;
        }
        return true;
    }
    
    void EnterSubmenu(int param_index, ParamType type, const MappingConfig& mapping) {
        submenu_param_index = param_index;
        submenu_selected_item = 0;
        submenu_scroll_offset = 0;
    }
    
    void ExitSubmenu() {
        submenu_param_index = -1;
        submenu_selected_item = 0;
        submenu_scroll_offset = 0;
    }
    
    void NextSubmenuItem(ParamType type, const MappingConfig& mapping) {
        int count = GetSubmenuItemCount(type, mapping);
        submenu_selected_item++;
        
        // Skip hidden items
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
        
        // Skip hidden items
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
        return submenu_param_index >= 0;
    }
    
    // === SUB Parameter Methods ===
    
    void EnterSub(Parameter* sub_param, int parent_index) {
        sub_parent = sub_param;
        sub_parent_index = parent_index;
        sub_child_selected = -1;  // Will be advanced to first visible
        sub_scroll_offset = 0;
        
        if (sub_param && sub_param->children) {
            for (int i = 0; i < sub_param->child_count; i++) {
                if (sub_param->children[i].IsVisible(sub_param->children, sub_param->child_count, i)) {
                    sub_child_selected = i;
                    break;
                }
            }
        }
    }
    
    void ExitSub() {
        sub_parent = nullptr;
        sub_parent_index = -1;
        sub_child_selected = 0;
        sub_scroll_offset = 0;
    }
    
    bool IsSubTitleSelected() const {
        return sub_parent != nullptr && sub_child_selected == -1;
    }
    
    bool IsInSub() const {
        return sub_parent != nullptr;
    }
    
    void NextSubChild() {
        if (!sub_parent || !sub_parent->children) return;
        
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
            return;
        }
        
        int start = sub_child_selected;
        
        // Find next visible item
        do {
            sub_child_selected++;
            if (sub_child_selected >= count) {
                // Wrap to title
                sub_child_selected = -1;
                sub_scroll_offset = 0;
                return;
            }
        } while (!sub_parent->children[sub_child_selected].IsVisible(
                    sub_parent->children, count, sub_child_selected) &&
                 sub_child_selected != start);
        
        UpdateSubScrollOffset();
    }
    
    void PrevSubChild() {
        if (!sub_parent || !sub_parent->children) return;
        
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
            return;
        }
        
        int start = sub_child_selected;
        
        // Find previous visible item
        do {
            sub_child_selected--;
            if (sub_child_selected < 0) {
                // Wrap to title
                sub_child_selected = -1;
                sub_scroll_offset = 0;
                return;
            }
        } while (!sub_parent->children[sub_child_selected].IsVisible(
                    sub_parent->children, count, sub_child_selected) &&
                 sub_child_selected != start);
        
        UpdateSubScrollOffset();
    }
    
    void UpdateSubScrollOffset() {
        if (!sub_parent || !sub_parent->children) return;
        
        // Count visible items before current selection
        int visible_before = 0;
        for (int i = 0; i < sub_child_selected; i++) {
            if (sub_parent->children[i].IsVisible(sub_parent->children, sub_parent->child_count, i)) {
                visible_before++;
            }
        }
        
        // Adjust scroll offset based on visible position
        if (visible_before < sub_scroll_offset) {
            sub_scroll_offset = visible_before;
        } else if (visible_before >= sub_scroll_offset + VISIBLE_PARAMS) {
            sub_scroll_offset = visible_before - VISIBLE_PARAMS + 1;
        }
    }
    
    int CountVisibleSubChildren() const {
        if (!sub_parent || !sub_parent->children) return 0;
        
        int count = 0;
        for (int i = 0; i < sub_parent->child_count; i++) {
            if (sub_parent->children[i].IsVisible(sub_parent->children, sub_parent->child_count, i)) {
                count++;
            }
        }
        return count;
    }
};

} // namespace mutables_ui
