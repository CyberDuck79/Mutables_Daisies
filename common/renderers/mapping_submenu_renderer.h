#pragma once

#include "daisy_patch.h"
#include "../parameter.h"
#include "../ui_state.h"
#include "../constants.h"
#include "menu_renderer.h"  // For GetMappingSourceName
#include <cstdio>
#include <cstring>

namespace mutables_ui {

using namespace mutables;

//=============================================================================
// MappingSubmenuRenderer - Renders mapping configuration submenus
//=============================================================================

template<typename DisplayType>
class MappingSubmenuRenderer {
public:
    MappingSubmenuRenderer(DisplayType& display) : display_(display) {}
    
    // Generic submenu renderer that dispatches based on type
    void RenderSubmenu(const MenuState& menu, Parameter& param) {
        switch (param.type) {
            case ParamType::KNOB:
                RenderKnobSubmenu(menu, param);
                break;
            case ParamType::CV:
                RenderCVSubmenu(menu, param);
                break;
            case ParamType::ENUM:
                RenderEnumSubmenu(menu, param);
                break;
            default:
                RenderFallbackSubmenu(param);
                break;
        }
    }
    
private:
    DisplayType& display_;
    
    // Render submenu for KNOB type
    void RenderKnobSubmenu(const MenuState& menu, Parameter& param) {
        display_.Fill(false);
        
        char buffer[32];
        
        // Title
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        display_.SetCursor(0, 1);
        display_.WriteString(buffer, Font_7x10, true);
        
        int line = kLineHeight;
        
        // Item 0: Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += kCompactLineHeight;
        
        // Item 1: CC Number (only if CC mapped)
        if (param.mapping.source == MappingSource::CC) {
            snprintf(buffer, sizeof(buffer), "%d", param.mapping.cc_number);
            RenderSubmenuLine("CC#", buffer,
                             line, menu.submenu_selected_item == 1,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 1);
            line += kCompactLineHeight;
        }
        
        // Item 2: Plugged (only if CV mapped)
        if (param.mapping.IsCVSource()) {
            if (param.mapping.plugged) {
                int offset_int = (int)(param.mapping.offset * 100.0f);
                int whole = offset_int / 100;
                int frac = offset_int % 100;
                snprintf(buffer, sizeof(buffer), "Yes %d.%02d", whole, frac);
            } else {
                snprintf(buffer, sizeof(buffer), "No");
            }
            RenderSubmenuLine("Plugged", buffer,
                             line, menu.submenu_selected_item == 2,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 2);
            line += kCompactLineHeight;
        }
        
        // Item 3: Attenuverter
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.attenuverter * 100.0f));
        RenderSubmenuLine("Atten", buffer,
                         line, menu.submenu_selected_item == 3,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 3);
        line += kCompactLineHeight;
        
        // Item 4: Velocity
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.velocity_amount * 100.0f));
        RenderSubmenuLine("Velocity", buffer,
                         line, menu.submenu_selected_item == 4,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 4);
        
        display_.Update();
    }
    
    // Render submenu for CV type
    void RenderCVSubmenu(const MenuState& menu, Parameter& param) {
        display_.Fill(false);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        display_.SetCursor(0, 1);
        display_.WriteString(buffer, Font_7x10, true);
        
        int line = kLineHeight;
        
        // Item 0: Mapping
        const char* source_name = "None";
        if (param.mapping.IsCVSource()) {
            static const char* cv_names[] = {"CV1", "CV2", "CV3", "CV4"};
            source_name = cv_names[param.mapping.GetCVIndex()];
        }
        
        RenderSubmenuLine("Map", source_name,
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += kCompactLineHeight;
        
        // Item 1: Plugged (only if CV mapped, read-only)
        if (param.mapping.IsCVSource()) {
            RenderSubmenuLine("Plugged", "Yes",
                             line, menu.submenu_selected_item == 1,
                             false);
        }
        
        display_.Update();
    }
    
    // Render submenu for ENUM type
    void RenderEnumSubmenu(const MenuState& menu, Parameter& param) {
        display_.Fill(false);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        display_.SetCursor(0, 1);
        display_.WriteString(buffer, Font_7x10, true);
        
        int line = kLineHeight;
        
        // Item 0: Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += kCompactLineHeight;
        
        // Item 1: CC Number (only if CC mapped)
        if (param.mapping.source == MappingSource::CC) {
            snprintf(buffer, sizeof(buffer), "%d", param.mapping.cc_number);
            RenderSubmenuLine("CC#", buffer,
                             line, menu.submenu_selected_item == 1,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 1);
            line += kCompactLineHeight;
        }
        
        // Item 2: Plugged (only if CV mapped)
        if (param.mapping.IsCVSource()) {
            if (param.mapping.plugged) {
                int offset_int = (int)(param.mapping.offset * 100.0f);
                int whole = offset_int / 100;
                int frac = offset_int % 100;
                snprintf(buffer, sizeof(buffer), "Yes %d.%02d", whole, frac);
            } else {
                snprintf(buffer, sizeof(buffer), "No");
            }
            RenderSubmenuLine("Plugged", buffer,
                             line, menu.submenu_selected_item == 2,
                             false);
            line += kCompactLineHeight;
        }
        
        // Item 3: Attenuverter (only if CV mapped)
        if (param.mapping.IsCVSource()) {
            snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.attenuverter * 100.0f));
            RenderSubmenuLine("Atten", buffer,
                             line, menu.submenu_selected_item == 3,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 3);
            line += kCompactLineHeight;
        }
        
        // Gate-specific items
        if (param.mapping.IsGateSource()) {
            // Item 4: Trigger
            static const char* trigger_names[] = {"Rise", "Fall", "Both"};
            RenderSubmenuLine("Trigger", trigger_names[static_cast<int>(param.mapping.trigger)],
                             line, menu.submenu_selected_item == 4,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 4);
            line += kCompactLineHeight;
            
            // Item 5: Action
            const char* action_str;
            if (param.mapping.trigger == TriggerMode::RISE_AND_FALL) {
                static const char* action_names_both[] = {"++", "--", "+-", "-+"};
                action_str = action_names_both[static_cast<int>(param.mapping.action)];
            } else {
                static const char* action_names_single[] = {"++", "--"};
                int action_idx = static_cast<int>(param.mapping.action);
                if (action_idx > 1) action_idx = 0;
                action_str = action_names_single[action_idx];
            }
            RenderSubmenuLine("Action", action_str,
                             line, menu.submenu_selected_item == 5,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 5);
        }
        
        display_.Update();
    }
    
    void RenderFallbackSubmenu(Parameter& param) {
        display_.Fill(false);
        display_.SetCursor(0, 1);
        display_.WriteString(param.name, Font_7x10, true);
        RenderSubmenuLine("Back", "", kLineHeight, true, false);
        display_.Update();
    }
    
    void RenderSubmenuLine(const char* label, const char* value, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Selection indicator
        if (selected) {
            display_.SetCursor(0, y);
            display_.WriteString(">", Font_7x10, true);
        }
        
        // Label
        display_.SetCursor(8, y);
        snprintf(buffer, sizeof(buffer), "%.8s", label);
        display_.WriteString(buffer, Font_7x10, true);
        
        // Value
        if (value && strlen(value) > 0) {
            int value_len = strlen(value);
            int value_width = value_len * kFont7x10Width;
            int value_x = 72;
            
            if (editing) {
                display_.DrawLine(value_x, y, value_x + value_width - 1, y, true);
                display_.DrawLine(value_x, y + 11, value_x + value_width - 1, y + 11, true);
            }
            
            display_.SetCursor(value_x, y + 1);
            display_.WriteString(value, Font_7x10, !editing);
        }
    }
};

} // namespace mutables_ui
