#pragma once

#include <cstring>
#include "ui_enums.h"

namespace mutables_ui {

// Handles preset save (CharInput), load (PresetList), and file browser states
struct PresetState {
    // For preset Save (CharInput state)
    char preset_name[MAX_PRESET_NAME_LEN + 1] = {0};  // Current name being typed
    int char_position = 0;                             // Current cursor position (0-15)
    int char_index = 0;                                // Index in character set for current position
    bool char_title_selected = false;                  // True if title bar is selected
    
    // For preset Load (PresetList state)
    int preset_selected = 0;          // Selected preset in list
    int preset_scroll_offset = 0;     // Scroll offset for preset list
    int preset_count = 0;             // Total presets found
    bool preset_title_selected = false;  // True if title bar is selected
    
    // For file browser (FileBrowser state for USER_DATA)
    int file_selected = 0;            // Selected file in list
    int file_scroll_offset = 0;       // Scroll offset for file list
    int file_count = 0;               // Total files found
    int file_browser_param_idx = -1;  // Which USER_DATA parameter we're editing
    
    // === Preset Save (CharInput) Methods ===
    
    void EnterCharInput() {
        preset_name[0] = '\0';
        char_position = 0;
        char_index = 0;  // Start at 'a'
        char_title_selected = false;
    }
    
    void NextChar() {
        if (char_title_selected) {
            char_title_selected = false;
        } else {
            if (char_index == kCharSetSize - 1) {
                char_title_selected = true;
            } else {
                char_index++;
            }
        }
    }
    
    void PrevChar() {
        if (char_title_selected) {
            char_title_selected = false;
            char_index = kCharSetSize - 1;
        } else {
            if (char_index == 0) {
                char_title_selected = true;
            } else {
                char_index--;
            }
        }
    }
    
    char GetCurrentChar() const {
        return kCharSet[char_index];
    }
    
    // Confirm current character and move to next position
    // Returns true if still editing, false if name complete or cancelled
    bool ConfirmChar() {
        char c = GetCurrentChar();
        
        if (c == ' ' && char_position > 0) {
            // Space at non-first position = backspace
            char_position--;
            preset_name[char_position] = '\0';
            // Set char_index to match the character we're now on
            if (char_position > 0) {
                for (int i = 0; i < kCharSetSize; i++) {
                    if (kCharSet[i] == preset_name[char_position - 1]) {
                        char_index = i;
                        break;
                    }
                }
            } else {
                char_index = 0;
            }
            return true;
        }
        
        if (c == ' ' && char_position == 0) {
            // Space at first position = exit without saving
            return false;
        }
        
        // Add character
        if (char_position < MAX_PRESET_NAME_LEN) {
            preset_name[char_position] = c;
            char_position++;
            preset_name[char_position] = '\0';
            char_index = 0;  // Reset to 'a' for next char
        }
        
        return char_position < MAX_PRESET_NAME_LEN;
    }
    
    void ExitCharInput() {
        char_position = 0;
        preset_name[0] = '\0';
    }
    
    bool IsPresetNameValid() const {
        return char_position > 0 || (!char_title_selected && char_index != kCharSetSize - 1);
    }
    
    const char* GetPresetName() const {
        return preset_name;
    }
    
    int GetFinalPresetName(char* buffer, size_t buffer_size) const {
        int len = strlen(preset_name);
        if (len < (int)buffer_size - 1) {
            strcpy(buffer, preset_name);
            // Add current character if it's not space and we're not on title
            if (!char_title_selected && char_position < MAX_PRESET_NAME_LEN) {
                char c = GetCurrentChar();
                if (c != ' ') {
                    buffer[len] = c;
                    buffer[len + 1] = '\0';
                    len++;
                }
            }
        }
        return len;
    }
    
    bool IsFinalPresetNameValid() const {
        if (char_title_selected) return false;
        if (char_position > 0) return true;
        char c = GetCurrentChar();
        return c != ' ';
    }
    
    // === Preset Load (PresetList) Methods ===
    
    void EnterPresetList(int count) {
        preset_count = count;
        preset_selected = 0;
        preset_scroll_offset = 0;
        preset_title_selected = false;
    }
    
    void NextPreset() {
        if (preset_title_selected) {
            preset_title_selected = false;
            preset_selected = 0;
            ScrollPresetToSelected();
        } else if (preset_count == 0) {
            return;
        } else if (preset_selected == preset_count - 1) {
            preset_title_selected = true;
        } else {
            preset_selected++;
            ScrollPresetToSelected();
        }
    }
    
    void PrevPreset() {
        if (preset_title_selected) {
            preset_title_selected = false;
            preset_selected = preset_count > 0 ? preset_count - 1 : 0;
            ScrollPresetToSelected();
        } else if (preset_count == 0) {
            return;
        } else if (preset_selected == 0) {
            preset_title_selected = true;
        } else {
            preset_selected--;
            ScrollPresetToSelected();
        }
    }
    
    void ScrollPresetToSelected() {
        if (preset_selected < preset_scroll_offset) {
            preset_scroll_offset = preset_selected;
        } else if (preset_selected >= preset_scroll_offset + VISIBLE_PARAMS) {
            preset_scroll_offset = preset_selected - VISIBLE_PARAMS + 1;
        }
    }
    
    void ExitPresetList() {
        preset_selected = 0;
        preset_count = 0;
    }
    
    int GetSelectedPreset() const {
        return preset_selected;
    }
    
    // === File Browser (for USER_DATA) Methods ===
    
    void EnterFileBrowser(int param_idx, int count) {
        file_browser_param_idx = param_idx;
        file_count = count;
        file_selected = 0;  // 0 = "Default" option
        file_scroll_offset = 0;
    }
    
    void NextFile() {
        file_selected = (file_selected + 1) % (file_count + 1);  // +1 for Default option
        ScrollFileToSelected();
    }
    
    void PrevFile() {
        file_selected = (file_selected - 1 + file_count + 1) % (file_count + 1);
        ScrollFileToSelected();
    }
    
    void ScrollFileToSelected() {
        if (file_selected < file_scroll_offset) {
            file_scroll_offset = file_selected;
        } else if (file_selected >= file_scroll_offset + VISIBLE_PARAMS) {
            file_scroll_offset = file_selected - VISIBLE_PARAMS + 1;
        }
    }
    
    void ExitFileBrowser() {
        file_browser_param_idx = -1;
        file_selected = 0;
        file_count = 0;
    }
    
    int GetSelectedFile() const {
        return file_selected;
    }
    
    bool IsDefaultSelected() const {
        return file_selected == 0;
    }
};

} // namespace mutables_ui
