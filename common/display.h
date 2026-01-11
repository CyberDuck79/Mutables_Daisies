#pragma once

#include "daisy_patch.h"
#include "parameter.h"
#include "ui_state.h"
#include <cstdio>
#include <cstring>

namespace mutables_ui {

class Display {
public:
    Display() : hw_(nullptr) {}
    
    void Init(daisy::DaisyPatch* hw) {
        hw_ = hw;
    }
    
    // Render boot screen with module name
    void RenderBootScreen(const char* module_name) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        int text_len = strlen(module_name);
        int x = (128 - (text_len * 7)) / 2;
        int y = (64 - 10) / 2;
        
        hw_->display.SetCursor(x, y);
        hw_->display.WriteString(module_name, Font_7x10, true);
        
        hw_->display.Update();
    }
    
    // Render main parameter menu
    void RenderMenu(const MenuState& menu, Parameter* params) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        int line = 0;
        for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                    (menu.scroll_offset + i) < menu.param_count; i++) {
            int param_idx = menu.scroll_offset + i;
            RenderParameter(params[param_idx], 
                          line, 
                          param_idx == menu.selected_param,
                          menu.state == UIState::EditValue && param_idx == menu.selected_param);
            line += 14;
        }
        
        hw_->display.Update();
    }
    
    // Render submenu for KNOB type
    void RenderKnobSubmenu(const MenuState& menu, Parameter& param) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        char buffer[32];
        
        // Title
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        hw_->display.SetCursor(0, 1);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        int line = 14;
        int visual_index = 0;  // For display positioning
        
        // Item 0: Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += 13;
        visual_index++;
        
        // Item 1: CC Number (only if CC mapped)
        if (param.mapping.source == MappingSource::CC) {
            snprintf(buffer, sizeof(buffer), "%d", param.mapping.cc_number);
            RenderSubmenuLine("CC#", buffer,
                             line, menu.submenu_selected_item == 1,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 1);
            line += 13;
            visual_index++;
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
            line += 13;
            visual_index++;
        }
        
        // Item 3: Attenuverter
        // Controls internal envelope modulation amount (works with both CV and CC mappings)
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.attenuverter * 100.0f));
        RenderSubmenuLine("Atten", buffer,
                         line, menu.submenu_selected_item == 3,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 3);
        line += 13;
        
        // Item 4: Velocity
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.velocity_amount * 100.0f));
        RenderSubmenuLine("Velocity", buffer,
                         line, menu.submenu_selected_item == 4,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 4);
        
        hw_->display.Update();
    }
    
    // Render submenu for CV type
    void RenderCVSubmenu(const MenuState& menu, Parameter& param) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        hw_->display.SetCursor(0, 1);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        int line = 14;
        
        // Item 0: Mapping
        const char* source_name = "None";
        if (param.mapping.IsCVSource()) {
            static const char* cv_names[] = {"CV1", "CV2", "CV3", "CV4"};
            source_name = cv_names[param.mapping.GetCVIndex()];
        }
        
        RenderSubmenuLine("Map", source_name,
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += 13;
        
        // Item 1: Plugged (only if CV mapped, read-only, auto-enabled)
        if (param.mapping.IsCVSource()) {
            RenderSubmenuLine("Plugged", "Yes",
                             line, menu.submenu_selected_item == 1,
                             false);  // Not editable
        }
        
        hw_->display.Update();
    }
    
    // Render submenu for ENUM type
    void RenderEnumSubmenu(const MenuState& menu, Parameter& param) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        hw_->display.SetCursor(0, 1);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        int line = 14;
        
        // Item 0: Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        line += 13;
        
        // Item 1: CC Number (only if CC mapped)
        if (param.mapping.source == MappingSource::CC) {
            snprintf(buffer, sizeof(buffer), "%d", param.mapping.cc_number);
            RenderSubmenuLine("CC#", buffer,
                             line, menu.submenu_selected_item == 1,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 1);
            line += 13;
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
                             false);  // Not editable, toggled on short press
            line += 13;
        }
        
        // Item 3: Attenuverter (only if CV or CC mapped)
        if (param.mapping.IsCVSource() || param.mapping.source == MappingSource::CC) {
            // Controls internal envelope modulation amount
            snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.attenuverter * 100.0f));
            RenderSubmenuLine("Atten", buffer,
                             line, menu.submenu_selected_item == 3,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 3);
            line += 13;
        }
        
        // Gate-specific items
        if (param.mapping.IsGateSource()) {
            // Item 4: Trigger
            static const char* trigger_names[] = {"Rise", "Fall", "Both"};
            RenderSubmenuLine("Trigger", trigger_names[static_cast<int>(param.mapping.trigger)],
                             line, menu.submenu_selected_item == 4,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 4);
            line += 13;
            
            // Item 5: Action - show valid actions based on trigger mode
            const char* action_str;
            if (param.mapping.trigger == TriggerMode::RISE_AND_FALL) {
                static const char* action_names_both[] = {"++", "--", "+-", "-+"};
                action_str = action_names_both[static_cast<int>(param.mapping.action)];
            } else {
                static const char* action_names_single[] = {"++", "--"};
                int action_idx = static_cast<int>(param.mapping.action);
                if (action_idx > 1) action_idx = 0;  // Clamp to valid
                action_str = action_names_single[action_idx];
            }
            RenderSubmenuLine("Action", action_str,
                             line, menu.submenu_selected_item == 5,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 5);
        }
        
        hw_->display.Update();
    }
    
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
                // Fallback - just show back option
                hw_->display.Fill(false);
                hw_->display.SetCursor(0, 1);
                hw_->display.WriteString(param.name, Font_7x10, true);
                RenderSubmenuLine("Back", "", 14, true, false);
                hw_->display.Update();
                break;
        }
    }
    
private:
    daisy::DaisyPatch* hw_;
    
    const char* GetMappingSourceName(MappingSource source, int cc_number) {
        switch (source) {
            case MappingSource::NONE: return "None";
            case MappingSource::CV1: return "CV1";
            case MappingSource::CV2: return "CV2";
            case MappingSource::CV3: return "CV3";
            case MappingSource::CV4: return "CV4";
            case MappingSource::GATE1: return "G1";
            case MappingSource::GATE2: return "G2";
            case MappingSource::CC: {
                static char cc_buf[8];
                snprintf(cc_buf, sizeof(cc_buf), "CC%d", cc_number);
                return cc_buf;
            }
            default: return "?";
        }
    }
    
    void RenderSubmenuLine(const char* label, const char* value, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Selection indicator
        if (selected) {
            hw_->display.SetCursor(0, y);
            hw_->display.WriteString(">", Font_7x10, true);
        }
        
        // Label
        hw_->display.SetCursor(8, y);
        snprintf(buffer, sizeof(buffer), "%.8s", label);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        // Value
        if (value && strlen(value) > 0) {
            int value_len = strlen(value);
            int value_width = value_len * 7;
            int value_x = 72;
            
            if (editing) {
                hw_->display.DrawLine(value_x, y, value_x + value_width - 1, y, true);
                hw_->display.DrawLine(value_x, y + 11, value_x + value_width - 1, y + 11, true);
            }
            
            hw_->display.SetCursor(value_x, y + 1);
            hw_->display.WriteString(value, Font_7x10, !editing);
        }
    }
    
    void RenderParameter(const Parameter& param, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Parameter name
        hw_->display.SetCursor(0, y + 1);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        int name_len = strlen(buffer);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        // Mapping indicator right after name (no white background)
        int indicator_x = name_len * 7;
        RenderMappingIndicator(param, y, indicator_x);
        
        // Underline if selected
        if (selected) {
            hw_->display.DrawLine(0, y + 11, name_len * 7 - 1, y + 11, true);
        }
        
        // Value (skip for SAVE/LOAD/SUB)
        if (param.type != ParamType::SAVE && 
            param.type != ParamType::LOAD && 
            param.type != ParamType::SUB) {
            FormatValue(param, buffer, sizeof(buffer));
            int value_len = strlen(buffer);
            int value_width = value_len * 7;
            
            if (editing) {
                hw_->display.DrawLine(76, y + 1, 76 + value_width - 1, y + 1, true);
                hw_->display.DrawLine(76, y + 12, 76 + value_width - 1, y + 12, true);
            }
            
            hw_->display.SetCursor(76, y + 2);
            hw_->display.WriteString(buffer, Font_7x10, !editing);
        }
        
        // Submenu indicator
        if (param.HasSubmenu()) {
            hw_->display.SetCursor(121, y + 1);
            hw_->display.WriteString(">", Font_7x10, true);
        }
    }
    
    void RenderMappingIndicator(const Parameter& param, int y, int x) {
        if (param.mapping.source == MappingSource::NONE) return;
        
        // Simple '*' indicator right after parameter name (no white background)
        hw_->display.SetCursor(x, y + 1);
        hw_->display.WriteString("*", Font_7x10, true);
    }
    
    void FormatValue(const Parameter& param, char* buffer, size_t size) {
        switch (param.type) {
            case ParamType::ENUM:
                snprintf(buffer, size, "%.6s", param.GetEnumLabel());
                break;
            case ParamType::MIDI:
                snprintf(buffer, size, "CH%d", param.GetIndex() + 1);
                break;
            case ParamType::CV:
            case ParamType::KNOB:
            default:
                {
                    int val_int = (int)(param.value * 100.0f);
                    int whole = val_int / 100;
                    int frac = val_int % 100;
                    snprintf(buffer, size, "%d.%02d", whole, frac);
                }
                break;
        }
    }
};

} // namespace mutables_ui
