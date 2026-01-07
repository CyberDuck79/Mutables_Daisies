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
            line += 14;  // 10px font + 4px padding for descenders
        }
        
        hw_->display.Update();
    }
    
    // Render CV mapping submenu
    void RenderSubmenu(const MenuState& menu, Parameter& param) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        char buffer[32];
        
        // Title
        snprintf(buffer, sizeof(buffer), "CV MAP: %.10s", param.name);
        hw_->display.SetCursor(0, 1);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        // CV Source
        RenderSubmenuItem(SubmenuItem::CVSource, 16, 
                         menu.selected_submenu_item == SubmenuItem::CVSource,
                         menu.state == UIState::SubmenuEdit,
                         param);
        
        // Attenuverter
        RenderSubmenuItem(SubmenuItem::Attenuverter, 32,
                         menu.selected_submenu_item == SubmenuItem::Attenuverter,
                         menu.state == UIState::SubmenuEdit,
                         param);
        
        // Capture Origin
        RenderSubmenuItem(SubmenuItem::CaptureOrigin, 48,
                         menu.selected_submenu_item == SubmenuItem::CaptureOrigin,
                         false,
                         param);
        
        hw_->display.Update();
    }
    
private:
    daisy::DaisyPatch* hw_;
    
    void RenderParameter(const Parameter& param, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Parameter name (truncated)
        hw_->display.SetCursor(0, y + 1);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        int name_len = strlen(buffer);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        // Underline if selected
        if (selected) {
            hw_->display.DrawLine(0, y + 11, name_len * 7 - 1, y + 11, true);
        }
        
        // Value - draw top and bottom lines if editing, with inverted text
        FormatValue(param, buffer, sizeof(buffer));
        int value_len = strlen(buffer);
        int value_width = value_len * 7;
        if (editing) {
            // Draw white line above and below text
            hw_->display.DrawLine(76, y + 1, 76 + value_width - 1, y + 1, true);
            hw_->display.DrawLine(76, y + 12, 76 + value_width - 1, y + 12, true);
        }
        // Write text: if editing use on=false for black-on-white, otherwise on=true for white-on-black
        hw_->display.SetCursor(76, y + 2);
        hw_->display.WriteString(buffer, Font_7x10, !editing);
        
        // CV indicator
        if (param.cv_mapping.active && param.cv_mapping.cv_input >= 0) {
            hw_->display.SetCursor(107, y + 1);
            snprintf(buffer, sizeof(buffer), "CV%d", param.cv_mapping.cv_input + 1);
            hw_->display.WriteString(buffer, Font_7x10, true);
        }
        
        // Submenu indicator
        hw_->display.SetCursor(121, y + 1);
        hw_->display.WriteString(">", Font_7x10, true);
    }
    
    void RenderSubmenuItem(SubmenuItem item, int y, bool selected, bool editing, const Parameter& param) {
        char buffer[32];
        
        if (selected) {
            hw_->display.SetCursor(0, y + 1);
            hw_->display.WriteString(">", Font_7x10, true);
        }
        
        hw_->display.SetCursor(8, y + 1);
        
        switch (item) {
            case SubmenuItem::CVSource:
                {
                    hw_->display.WriteString("Source:", Font_7x10, true);
                    
                    if (param.cv_mapping.cv_input < 0) {
                        snprintf(buffer, sizeof(buffer), "None");
                    } else {
                        snprintf(buffer, sizeof(buffer), "CV%d", param.cv_mapping.cv_input + 1);
                    }
                    int text_len = strlen(buffer);
                    int text_width = text_len * 7;
                    if (editing) {
                        // Draw white line above and below
                        hw_->display.DrawLine(61, y + 1, 61 + text_width - 1, y + 1, true);
                        hw_->display.DrawLine(61, y + 12, 61 + text_width - 1, y + 12, true);
                    }
                    hw_->display.SetCursor(61, y + 2);
                    hw_->display.WriteString(buffer, Font_7x10, !editing);
                }
                break;
                
            case SubmenuItem::Attenuverter:
                {
                    hw_->display.WriteString("Atten:", Font_7x10, true);
                    snprintf(buffer, sizeof(buffer), "%+.2f", param.cv_mapping.attenuverter);
                    int atten_len = strlen(buffer);
                    int atten_width = atten_len * 7;
                    if (editing) {
                        // Draw white line above and below
                        hw_->display.DrawLine(61, y + 1, 61 + atten_width - 1, y + 1, true);
                        hw_->display.DrawLine(61, y + 12, 61 + atten_width - 1, y + 12, true);
                    }
                    hw_->display.SetCursor(61, y + 2);
                    hw_->display.WriteString(buffer, Font_7x10, !editing);
                }
                break;
                
            case SubmenuItem::CaptureOrigin:
                {
                    const char* text = "Capture Origin";
                    int capture_width = 14 * 7;  // 14 chars
                    if (editing) {
                        // Draw white line above and below
                        hw_->display.DrawLine(8, y + 1, 8 + capture_width - 1, y + 1, true);
                        hw_->display.DrawLine(8, y + 12, 8 + capture_width - 1, y + 12, true);
                    }
                    hw_->display.SetCursor(8, y + 2);
                    hw_->display.WriteString(text, Font_7x10, !editing);
                }
                break;
                
            default:
                break;
        }
    }
    
    void FormatValue(const Parameter& param, char* buffer, size_t size) {
        switch (param.type) {
            case ParamType::Enum:
                snprintf(buffer, size, "%.8s", param.GetEnumLabel());
                break;
            case ParamType::Toggle:
                snprintf(buffer, size, "%s", param.value > 0.5f ? "ON" : "OFF");
                break;
            case ParamType::Integer:
                snprintf(buffer, size, "%d", param.GetIndex());
                break;
            case ParamType::Bipolar:
                snprintf(buffer, size, "%+.2f", param.value);
                break;
            case ParamType::Continuous:
            default:
                snprintf(buffer, size, "%.2f", param.value);
                break;
        }
    }
};

} // namespace mutables_ui
