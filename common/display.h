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
    
    // Render SUB children with visibility support
    void RenderSubMenu(const MenuState& menu, Parameter* parent) {
        if (!hw_ || !parent || !parent->children) return;
        
        hw_->display.Fill(false);
        
        // Title bar with parent name - selectable as "back" button
        bool title_selected = (menu.sub_child_selected == -1);
        char title[20];
        if (title_selected) {
            snprintf(title, sizeof(title), "< %.10s", parent->name);
            // Draw white background for selected title
            hw_->display.DrawRect(0, 0, 127, 9, true, true);
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString(title, Font_6x8, false);  // Inverted
        } else {
            snprintf(title, sizeof(title), "%.12s", parent->name);
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString(title, Font_6x8, true);
            hw_->display.DrawLine(0, 9, 127, 9, true);
        }
        
        int line = 12;
        int visible_count = 0;  // Count of visible items we've seen
        int rendered_count = 0;  // Count of items we've rendered
        
        // Render visible items, skipping based on scroll offset
        for (int param_idx = 0; param_idx < parent->child_count && line < 64; param_idx++) {
            Parameter& param = parent->children[param_idx];
            
            // Check visibility
            if (!param.IsVisible(parent->children, parent->child_count, param_idx)) {
                continue;  // Skip invisible params
            }
            
            // Skip items before scroll offset (scroll_offset is a visible-item count)
            if (visible_count < menu.scroll_offset) {
                visible_count++;
                continue;
            }
            
            // Render this parameter
            RenderSubParameter(param, parent->children, parent->child_count, param_idx,
                              line, 
                              param_idx == menu.sub_child_selected,
                              menu.state == UIState::EditValue && param_idx == menu.sub_child_selected);
            line += 13;
            visible_count++;
            rendered_count++;
            
            // Stop after visible params limit
            if (rendered_count >= MenuState::VISIBLE_PARAMS) break;
        }
        
        hw_->display.Update();
    }
    
    // Render a parameter within a SUB (with context for formatting)
    void RenderSubParameter(const Parameter& param, const Parameter* siblings, uint8_t sibling_count, 
                           uint8_t param_index, int y, bool selected, bool editing) {
        char buffer[32];
        
        // Parameter name
        hw_->display.SetCursor(0, y + 1);
        snprintf(buffer, sizeof(buffer), "%.10s", param.name);
        int name_len = strlen(buffer);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        // Mapping indicator '*' right after name
        if (param.mapping.source != MappingSource::NONE) {
            hw_->display.SetCursor(name_len * 7, y + 1);
            hw_->display.WriteString("*", Font_7x10, true);
        }
        
        // Underline if selected
        if (selected) {
            hw_->display.DrawLine(0, y + 11, name_len * 7 - 1, y + 11, true);
        }
        
        // Value (skip for SAVE/LOAD/SUB)
        if (param.type != ParamType::SAVE && 
            param.type != ParamType::LOAD && 
            param.type != ParamType::SUB) {
            // Use the custom formatter if available
            param.FormatDisplayValue(siblings, sibling_count, param_index, buffer, sizeof(buffer));
            int value_len = strlen(buffer);
            int value_width = value_len * 7;
            
            if (editing) {
                hw_->display.DrawLine(76, y + 1, 76 + value_width - 1, y + 1, true);
                hw_->display.DrawLine(76, y + 12, 76 + value_width - 1, y + 12, true);
            }
            
            hw_->display.SetCursor(76, y + 2);
            hw_->display.WriteString(buffer, Font_7x10, !editing);
        }
        
        // Submenu indicator (only for SUB type, not mappable params)
        if (param.type == ParamType::SUB) {
            hw_->display.SetCursor(121, y + 1);
            hw_->display.WriteString(">", Font_7x10, true);
        }
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
        
        // Item 3: Attenuverter (only if CV mapped, not CC - CC directly sets value)
        if (param.mapping.IsCVSource()) {
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
    
    // Render character input screen for preset save
    void RenderCharInput(const MenuState& menu) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        // Title - smaller font with underline, like submenu
        if (menu.char_title_selected) {
            hw_->display.DrawRect(0, 0, 127, 9, true, true);  // White background
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString("< Save Preset", Font_6x8, false);  // Inverted
        } else {
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString("Save Preset", Font_6x8, true);
            hw_->display.DrawLine(0, 9, 127, 9, true);  // Underline
        }
        
        // Current name with cursor
        char display_name[20];
        int name_len = strlen(menu.preset_name);
        
        // Build display string with cursor character
        for (int i = 0; i < MenuState::MAX_PRESET_NAME_LEN; i++) {
            if (i < name_len) {
                display_name[i] = menu.preset_name[i];
            } else if (i == menu.char_position && !menu.char_title_selected) {
                display_name[i] = menu.GetCurrentChar();
            } else {
                display_name[i] = '_';
            }
        }
        display_name[MenuState::MAX_PRESET_NAME_LEN] = '\0';
        
        // Draw name (centered, 2 lines of 8 chars)
        hw_->display.SetCursor(8, 20);
        char line1[9];
        strncpy(line1, display_name, 8);
        line1[8] = '\0';
        hw_->display.WriteString(line1, Font_7x10, true);
        
        hw_->display.SetCursor(8, 32);
        char line2[9];
        strncpy(line2, display_name + 8, 8);
        line2[8] = '\0';
        hw_->display.WriteString(line2, Font_7x10, true);
        
        // Cursor underline (only if not on title)
        if (!menu.char_title_selected) {
            int cursor_row = menu.char_position / 8;
            int cursor_col = menu.char_position % 8;
            int cursor_x = 8 + cursor_col * 7;
            int cursor_y = (cursor_row == 0) ? 31 : 43;
            hw_->display.DrawLine(cursor_x, cursor_y, cursor_x + 6, cursor_y, true);
        }
        
        // Instructions
        hw_->display.SetCursor(0, 54);
        hw_->display.WriteString("press:next  hold:save", Font_6x8, true);
        
        hw_->display.Update();
    }
    
    // Render preset list for loading
    void RenderPresetList(const MenuState& menu, const char* (*getPresetName)(int)) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        // Title - smaller font with underline, like submenu
        if (menu.preset_title_selected) {
            hw_->display.DrawRect(0, 0, 127, 9, true, true);  // White background
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString("< Load Preset", Font_6x8, false);  // Inverted
        } else {
            hw_->display.SetCursor(0, 0);
            hw_->display.WriteString("Load Preset", Font_6x8, true);
            hw_->display.DrawLine(0, 9, 127, 9, true);  // Underline
        }
        
        if (menu.preset_count == 0) {
            hw_->display.SetCursor(8, 28);
            hw_->display.WriteString("No presets", Font_7x10, true);
        } else {
            int line = 12;
            for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                        (menu.preset_scroll_offset + i) < menu.preset_count; i++) {
                int preset_idx = menu.preset_scroll_offset + i;
                bool selected = (preset_idx == menu.preset_selected) && !menu.preset_title_selected;
                
                // Selection indicator
                if (selected) {
                    hw_->display.SetCursor(0, line);
                    hw_->display.WriteString(">", Font_7x10, true);
                }
                
                // Preset name
                const char* name = getPresetName(preset_idx);
                if (name) {
                    hw_->display.SetCursor(8, line);
                    char truncated[17];
                    strncpy(truncated, name, 16);
                    truncated[16] = '\0';
                    hw_->display.WriteString(truncated, Font_7x10, true);
                }
                
                line += 12;
            }
        }
        
        hw_->display.Update();
    }
    
    // Render a temporary message (success/error)
    void RenderMessage(const char* title, const char* message, bool success = true) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        // Title centered
        int title_len = strlen(title);
        int title_x = (128 - title_len * 7) / 2;
        hw_->display.SetCursor(title_x, 20);
        hw_->display.WriteString(title, Font_7x10, true);
        
        // Message centered
        int msg_len = strlen(message);
        int msg_x = (128 - msg_len * 7) / 2;
        hw_->display.SetCursor(msg_x, 36);
        hw_->display.WriteString(message, Font_7x10, true);
        
        hw_->display.Update();
    }
    
    // Render file browser for USER_DATA selection
    void RenderFileBrowser(const MenuState& menu, const char* title, const char* (*getFileName)(int)) {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        // Title (e.g., "6-Op Bank 1")
        hw_->display.SetCursor(0, 1);
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%.12s", title);
        hw_->display.WriteString(buffer, Font_7x10, true);
        
        int line = 14;
        int total_items = menu.file_count + 1;  // +1 for "Default" option
        
        for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                    (menu.file_scroll_offset + i) < total_items; i++) {
            int item_idx = menu.file_scroll_offset + i;
            bool selected = (item_idx == menu.file_selected);
            
            // Selection indicator
            if (selected) {
                hw_->display.SetCursor(0, line);
                hw_->display.WriteString(">", Font_7x10, true);
            }
            
            // Item name
            hw_->display.SetCursor(8, line);
            if (item_idx == 0) {
                // First item is always "Default" (firmware built-in)
                hw_->display.WriteString("Default", Font_7x10, true);
            } else {
                // Get actual file name (item_idx - 1 because Default is at 0)
                const char* name = getFileName(item_idx - 1);
                if (name) {
                    char truncated[17];
                    strncpy(truncated, name, 16);
                    truncated[16] = '\0';
                    hw_->display.WriteString(truncated, Font_7x10, true);
                }
            }
            
            line += 12;
        }
        
        hw_->display.Update();
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
        
        // Submenu indicator (only for SUB type, not mappable params)
        if (param.type == ParamType::SUB) {
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
            case ParamType::USER_DATA:
                // Show filename (truncated) or "Def" for firmware default
                if (param.user_data_filename[0] == '\0') {
                    snprintf(buffer, size, "Def");
                } else {
                    // Just show first 5 chars of filename
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
};

} // namespace mutables_ui
