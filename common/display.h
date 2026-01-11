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
        int item_index = 0;
        
        // Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == item_index,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
        line += 13;
        item_index++;
        
        // Plugged (only if CV mapped, not CC)
        if (param.mapping.IsCVSource()) {
            RenderSubmenuLine("Plugged", param.mapping.plugged ? "ON" : "OFF",
                             line, menu.submenu_selected_item == item_index,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
        } else {
            RenderSubmenuLine("Plugged", "-",
                             line, menu.submenu_selected_item == item_index, false);
        }
        line += 13;
        item_index++;
        
        // Attenuverter
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.attenuverter * 100.0f));
        RenderSubmenuLine("Atten", buffer,
                         line, menu.submenu_selected_item == item_index,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
        line += 13;
        item_index++;
        
        // Velocity
        snprintf(buffer, sizeof(buffer), "%+d%%", (int)(param.mapping.velocity_amount * 100.0f));
        RenderSubmenuLine("Velocity", buffer,
                         line, menu.submenu_selected_item == item_index,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
        
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
        
        // Only mapping option for CV
        const char* source_name = "None";
        if (param.mapping.IsCVSource()) {
            static const char* cv_names[] = {"CV1", "CV2", "CV3", "CV4"};
            source_name = cv_names[param.mapping.GetCVIndex()];
        }
        
        RenderSubmenuLine("Map", source_name,
                         14, menu.submenu_selected_item == 0,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == 0);
        
        // Back
        RenderSubmenuLine("Back", "",
                         27, menu.submenu_selected_item == 1, false);
        
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
        int item_index = 0;
        
        // Mapping
        RenderSubmenuLine("Map", GetMappingSourceName(param.mapping.source, param.mapping.cc_number),
                         line, menu.submenu_selected_item == item_index,
                         menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
        line += 13;
        item_index++;
        
        // Trigger and Action only if Gate mapped
        if (param.mapping.IsGateSource()) {
            // Trigger
            static const char* trigger_names[] = {"Rise", "Fall", "Both"};
            RenderSubmenuLine("Trigger", trigger_names[static_cast<int>(param.mapping.trigger)],
                             line, menu.submenu_selected_item == item_index,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
            line += 13;
            item_index++;
            
            // Action
            static const char* action_names[] = {"++", "--", "+-", "-+"};
            RenderSubmenuLine("Action", action_names[static_cast<int>(param.mapping.action)],
                             line, menu.submenu_selected_item == item_index,
                             menu.state == UIState::SubmenuEdit && menu.submenu_selected_item == item_index);
            line += 13;
            item_index++;
        }
        
        // Back
        RenderSubmenuLine("Back", "",
                         line, menu.submenu_selected_item == item_index, false);
        
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
            
            // Mapping indicator
            RenderMappingIndicator(param, y, 76 + value_width + 7);
        }
        
        // Submenu indicator
        if (param.HasSubmenu()) {
            hw_->display.SetCursor(121, y + 1);
            hw_->display.WriteString(">", Font_7x10, true);
        }
    }
    
    void RenderMappingIndicator(const Parameter& param, int y, int x) {
        if (param.mapping.source == MappingSource::NONE) return;
        
        char indicator[4] = {0};
        
        if (param.mapping.IsCVSource()) {
            // [1]-[4] for CV
            indicator[0] = '1' + param.mapping.GetCVIndex();
        } else if (param.mapping.IsGateSource()) {
            // G1 or G2 for Gate
            indicator[0] = 'G';
            indicator[1] = '1' + param.mapping.GetGateIndex();
        } else if (param.mapping.source == MappingSource::CC) {
            // # for CC
            indicator[0] = '#';
        }
        
        if (indicator[0]) {
            int ind_len = strlen(indicator);
            int ind_width = ind_len * 7;
            
            // Draw box around indicator
            hw_->display.DrawLine(x, y + 1, x + ind_width - 1, y + 1, true);
            hw_->display.DrawLine(x, y + 12, x + ind_width - 1, y + 12, true);
            hw_->display.DrawRect(x, y + 2, ind_width, 10, true, true);
            
            hw_->display.SetCursor(x, y + 2);
            hw_->display.WriteString(indicator, Font_7x10, false);
        }
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
