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
            line += 16;
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
        hw_->display.SetCursor(0, 0);
        hw_->display.WriteString(buffer, Font_6x8, true);
        
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
        
        // Selection indicator
        if (selected) {
            hw_->display.SetCursor(0, y);
            hw_->display.WriteString(">", Font_6x8, true);
        }
        
        // Parameter name (truncated)
        hw_->display.SetCursor(8, y);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        hw_->display.WriteString(buffer, Font_6x8, true);
        
        // Value
        hw_->display.SetCursor(72, y);
        FormatValue(param, buffer, sizeof(buffer));
        hw_->display.WriteString(buffer, Font_6x8, editing);
        
        // CV indicator
        if (param.cv_mapping.active && param.cv_mapping.cv_input >= 0) {
            hw_->display.SetCursor(104, y);
            snprintf(buffer, sizeof(buffer), "CV%d", param.cv_mapping.cv_input + 1);
            hw_->display.WriteString(buffer, Font_6x8, true);
        }
        
        // Submenu indicator
        hw_->display.SetCursor(122, y);
        hw_->display.WriteString(">", Font_6x8, true);
    }
    
    void RenderSubmenuItem(SubmenuItem item, int y, bool selected, bool editing, const Parameter& param) {
        char buffer[32];
        
        if (selected) {
            hw_->display.SetCursor(0, y);
            hw_->display.WriteString(">", Font_6x8, true);
        }
        
        hw_->display.SetCursor(8, y);
        
        switch (item) {
            case SubmenuItem::CVSource:
                hw_->display.WriteString("Source:", Font_6x8, true);
                hw_->display.SetCursor(60, y);
                if (param.cv_mapping.cv_input < 0) {
                    hw_->display.WriteString("None", Font_6x8, editing);
                } else {
                    snprintf(buffer, sizeof(buffer), "CV%d", param.cv_mapping.cv_input + 1);
                    hw_->display.WriteString(buffer, Font_6x8, editing);
                }
                break;
                
            case SubmenuItem::Attenuverter:
                hw_->display.WriteString("Atten:", Font_6x8, true);
                hw_->display.SetCursor(60, y);
                snprintf(buffer, sizeof(buffer), "%+.2f", param.cv_mapping.attenuverter);
                hw_->display.WriteString(buffer, Font_6x8, editing);
                break;
                
            case SubmenuItem::CaptureOrigin:
                hw_->display.WriteString("Capture Origin", Font_6x8, editing);
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
