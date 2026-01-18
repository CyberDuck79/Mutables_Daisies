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
// PresetRenderer - Preset save/load and file browser rendering
//=============================================================================

template<typename DisplayType>
class PresetRenderer {
public:
    PresetRenderer(DisplayType& display) : display_(display) {}
    
    // Render character input screen for preset save
    void RenderCharInput(const MenuState& menu) {
        display_.Fill(false);
        
        // Title - smaller font with underline, like submenu
        if (menu.char_title_selected) {
            display_.DrawRect(0, 0, kScreenWidth - 1, 9, true, true);
            display_.SetCursor(0, 0);
            display_.WriteString("< Save Preset", Font_6x8, false);
        } else {
            display_.SetCursor(0, 0);
            display_.WriteString("Save Preset", Font_6x8, true);
            display_.DrawLine(0, 9, kScreenWidth - 1, 9, true);
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
        display_.SetCursor(8, 20);
        char line1[9];
        strncpy(line1, display_name, 8);
        line1[8] = '\0';
        display_.WriteString(line1, Font_7x10, true);
        
        display_.SetCursor(8, 32);
        char line2[9];
        strncpy(line2, display_name + 8, 8);
        line2[8] = '\0';
        display_.WriteString(line2, Font_7x10, true);
        
        // Cursor underline (only if not on title)
        if (!menu.char_title_selected) {
            int cursor_row = menu.char_position / 8;
            int cursor_col = menu.char_position % 8;
            int cursor_x = 8 + cursor_col * kFont7x10Width;
            int cursor_y = (cursor_row == 0) ? 31 : 43;
            display_.DrawLine(cursor_x, cursor_y, cursor_x + 6, cursor_y, true);
        }
        
        // Instructions
        display_.SetCursor(0, 54);
        display_.WriteString("press:next  hold:save", Font_6x8, true);
        
        display_.Update();
    }
    
    // Render preset list for loading
    void RenderPresetList(const MenuState& menu, const char* (*getPresetName)(int)) {
        display_.Fill(false);
        
        // Title
        if (menu.preset_title_selected) {
            display_.DrawRect(0, 0, kScreenWidth - 1, 9, true, true);
            display_.SetCursor(0, 0);
            display_.WriteString("< Load Preset", Font_6x8, false);
        } else {
            display_.SetCursor(0, 0);
            display_.WriteString("Load Preset", Font_6x8, true);
            display_.DrawLine(0, 9, kScreenWidth - 1, 9, true);
        }
        
        if (menu.preset_count == 0) {
            display_.SetCursor(8, 28);
            display_.WriteString("No presets", Font_7x10, true);
        } else {
            int line = 12;
            for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                        (menu.preset_scroll_offset + i) < menu.preset_count; i++) {
                int preset_idx = menu.preset_scroll_offset + i;
                bool selected = (preset_idx == menu.preset_selected) && !menu.preset_title_selected;
                
                // Selection indicator
                if (selected) {
                    display_.SetCursor(0, line);
                    display_.WriteString(">", Font_7x10, true);
                }
                
                // Preset name
                const char* name = getPresetName(preset_idx);
                if (name) {
                    display_.SetCursor(8, line);
                    char truncated[17];
                    strncpy(truncated, name, 16);
                    truncated[16] = '\0';
                    display_.WriteString(truncated, Font_7x10, true);
                }
                
                line += 12;
            }
        }
        
        display_.Update();
    }
    
    // Render file browser for USER_DATA selection
    void RenderFileBrowser(const MenuState& menu, const char* title, const char* (*getFileName)(int)) {
        display_.Fill(false);
        
        // Title (e.g., "6-Op Bank 1")
        display_.SetCursor(0, 1);
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%.12s", title);
        display_.WriteString(buffer, Font_7x10, true);
        
        int line = kLineHeight;
        int total_items = menu.file_count + 1;  // +1 for "Default" option
        
        for (int i = 0; i < MenuState::VISIBLE_PARAMS && 
                    (menu.file_scroll_offset + i) < total_items; i++) {
            int item_idx = menu.file_scroll_offset + i;
            bool selected = (item_idx == menu.file_selected);
            
            // Selection indicator
            if (selected) {
                display_.SetCursor(0, line);
                display_.WriteString(">", Font_7x10, true);
            }
            
            // Item name
            display_.SetCursor(8, line);
            if (item_idx == 0) {
                // First item is always "Default" (firmware built-in)
                display_.WriteString("Default", Font_7x10, true);
            } else {
                // Get actual file name (item_idx - 1 because Default is at 0)
                const char* name = getFileName(item_idx - 1);
                if (name) {
                    char truncated[17];
                    strncpy(truncated, name, 16);
                    truncated[16] = '\0';
                    display_.WriteString(truncated, Font_7x10, true);
                }
            }
            
            line += 12;
        }
        
        display_.Update();
    }
    
    // Render a temporary message (success/error)
    void RenderMessage(const char* title, const char* message, bool success = true) {
        display_.Fill(false);
        
        // Title centered
        int title_len = strlen(title);
        int title_x = (kScreenWidth - title_len * kFont7x10Width) / 2;
        display_.SetCursor(title_x, 20);
        display_.WriteString(title, Font_7x10, true);
        
        // Message centered
        int msg_len = strlen(message);
        int msg_x = (kScreenWidth - msg_len * kFont7x10Width) / 2;
        display_.SetCursor(msg_x, 36);
        display_.WriteString(message, Font_7x10, true);
        
        display_.Update();
    }
    
private:
    DisplayType& display_;
};

} // namespace mutables_ui
