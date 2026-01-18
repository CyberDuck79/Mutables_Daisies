#pragma once

#include "daisy_patch.h"
#include "../parameter.h"
#include "../ui_state.h"
#include "../constants.h"
#include <cstdio>
#include <cstring>

namespace mutables_ui {

using namespace mutables;

//=============================================================================
// Rendering helper functions (stateless, can be used by any renderer)
//=============================================================================

// Get human-readable name for mapping source
inline const char* GetMappingSourceName(MappingSource source, int cc_number) {
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

// Format parameter value to string
inline void FormatValue(const Parameter& param, char* buffer, size_t size, 
                       const Parameter* siblings = nullptr, uint8_t sibling_count = 0, 
                       uint8_t param_index = 0) {
    // Check for custom format callback first
    if (param.format_callback) {
        param.format_callback(&param, siblings, sibling_count, param_index, buffer, size);
        return;
    }
    
    switch (param.type) {
        case ParamType::ENUM:
            snprintf(buffer, size, "%.7s", param.GetEnumLabel());
            break;
        case ParamType::MIDI:
            snprintf(buffer, size, "CH%d", param.GetIndex() + 1);
            break;
        case ParamType::USER_DATA:
            if (param.user_data_filename[0] == '\0') {
                snprintf(buffer, size, "Def");
            } else {
                snprintf(buffer, size, "%.5s", param.user_data_filename);
            }
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

//=============================================================================
// MenuRenderer - Main menu and parameter rendering
//=============================================================================

template<typename DisplayType>
class MenuRenderer {
public:
    MenuRenderer(DisplayType& display) : display_(display) {}
    
    // Render main parameter menu
    void RenderMenu(const MenuState& menu, Parameter* params) {
        display_.Fill(false);
        
        int line = 0;
        for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                    (menu.scroll_offset + i) < menu.param_count; i++) {
            int param_idx = menu.scroll_offset + i;
            RenderParameter(params[param_idx], 
                          line, 
                          param_idx == menu.selected_param,
                          menu.state == UIState::EditValue && param_idx == menu.selected_param);
            line += kLineHeight;
        }
        
        display_.Update();
    }
    
    // Render SUB children with visibility support
    void RenderSubMenu(const MenuState& menu, Parameter* parent) {
        if (!parent || !parent->children) return;
        
        display_.Fill(false);
        
        // Title bar with parent name - selectable as "back" button
        bool title_selected = (menu.sub_child_selected == -1);
        char title[20];
        if (title_selected) {
            snprintf(title, sizeof(title), "< %.10s", parent->name);
            display_.DrawRect(0, 0, kScreenWidth - 1, 9, true, true);
            display_.SetCursor(0, 1);
            display_.WriteString(title, Font_6x8, false);  // Inverted
        } else {
            snprintf(title, sizeof(title), "%.12s", parent->name);
            display_.SetCursor(0, 1);
            display_.WriteString(title, Font_6x8, true);
            display_.DrawLine(0, 10, kScreenWidth - 1, 10, true);
        }
        
        int line = 12;
        int visible_count = 0;
        int rendered_count = 0;
        
        for (int param_idx = 0; param_idx < parent->child_count && line < kScreenHeight; param_idx++) {
            Parameter& param = parent->children[param_idx];
            
            if (!param.IsVisible(parent->children, parent->child_count, param_idx)) {
                continue;
            }
            
            if (visible_count < menu.scroll_offset) {
                visible_count++;
                continue;
            }
            
            RenderSubParameter(param, parent->children, parent->child_count, param_idx,
                              line, 
                              param_idx == menu.sub_child_selected,
                              menu.state == UIState::EditValue && param_idx == menu.sub_child_selected);
            line += kCompactLineHeight;
            visible_count++;
            rendered_count++;
            
            if (rendered_count >= MenuState::VISIBLE_PARAMS) break;
        }
        
        display_.Update();
    }
    
private:
    DisplayType& display_;
    
    void RenderParameter(const Parameter& param, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Parameter name
        display_.SetCursor(0, y + 1);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        int name_len = strlen(buffer);
        display_.WriteString(buffer, Font_7x10, true);
        
        // Mapping indicator right after name
        if (param.mapping.source != MappingSource::NONE) {
            display_.SetCursor(name_len * kFont7x10Width, y + 1);
            display_.WriteString("*", Font_7x10, true);
        }
        
        // Underline if selected
        if (selected) {
            display_.DrawLine(0, y + 11, name_len * kFont7x10Width - 1, y + 11, true);
        }
        
        // Value (skip for SAVE/LOAD/SUB)
        if (param.type != ParamType::SAVE && 
            param.type != ParamType::LOAD && 
            param.type != ParamType::SUB) {
            FormatValue(param, buffer, sizeof(buffer));
            int value_len = strlen(buffer);
            int value_width = value_len * kFont7x10Width;
            
            if (editing) {
                display_.DrawLine(kValueColumn, y + 1, kValueColumn + value_width - 1, y + 1, true);
                display_.DrawLine(kValueColumn, y + 12, kValueColumn + value_width - 1, y + 12, true);
            }
            
            display_.SetCursor(kValueColumn, y + 2);
            display_.WriteString(buffer, Font_7x10, !editing);
        }
        
        // Submenu indicator
        if (param.type == ParamType::SUB || param.type == ParamType::SAVE || param.type == ParamType::LOAD) {
            display_.SetCursor(121, y + 1);
            display_.WriteString(">", Font_7x10, true);
        }
    }
    
    void RenderSubParameter(const Parameter& param, const Parameter* siblings, uint8_t sibling_count, 
                           uint8_t param_index, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Parameter name
        display_.SetCursor(0, y + 1);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        int name_len = strlen(buffer);
        display_.WriteString(buffer, Font_7x10, true);
        
        // Mapping indicator
        if (param.mapping.source != MappingSource::NONE) {
            display_.SetCursor(name_len * kFont7x10Width, y + 1);
            display_.WriteString("*", Font_7x10, true);
        }
        
        // Underline if selected
        if (selected) {
            display_.DrawLine(0, y + 11, name_len * kFont7x10Width - 1, y + 11, true);
        }
        
        // Value
        if (param.type != ParamType::SAVE && 
            param.type != ParamType::LOAD && 
            param.type != ParamType::SUB) {
            param.FormatDisplayValue(siblings, sibling_count, param_index, buffer, sizeof(buffer));
            int value_len = strlen(buffer);
            int value_width = value_len * kFont7x10Width;
            
            if (editing) {
                display_.DrawLine(kValueColumn, y + 1, kValueColumn + value_width - 1, y + 1, true);
                display_.DrawLine(kValueColumn, y + 12, kValueColumn + value_width - 1, y + 12, true);
            }
            
            display_.SetCursor(kValueColumn, y + 2);
            display_.WriteString(buffer, Font_7x10, !editing);
        }
        
        // Submenu indicator for SUB type
        if (param.type == ParamType::SUB) {
            display_.SetCursor(121, y + 1);
            display_.WriteString(">", Font_7x10, true);
        }
    }
};

} // namespace mutables_ui
