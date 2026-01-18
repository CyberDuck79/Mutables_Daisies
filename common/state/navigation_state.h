#pragma once

#include "ui_enums.h"

namespace mutables_ui {

// Handles main menu navigation (parameter scrolling)
struct NavigationState {
    int selected_param = 0;
    int param_count = 0;
    int scroll_offset = 0;
    
    void ScrollToSelected() {
        if (selected_param < scroll_offset) {
            scroll_offset = selected_param;
        } else if (selected_param >= scroll_offset + VISIBLE_PARAMS) {
            scroll_offset = selected_param - VISIBLE_PARAMS + 1;
        }
    }
    
    void NextParam() {
        selected_param++;
        if (selected_param >= param_count) {
            selected_param = 0;
            scroll_offset = 0;
        } else {
            ScrollToSelected();
        }
    }
    
    void PrevParam() {
        selected_param--;
        if (selected_param < 0) {
            selected_param = param_count - 1;
            scroll_offset = selected_param - VISIBLE_PARAMS + 1;
            if (scroll_offset < 0) scroll_offset = 0;
        } else {
            ScrollToSelected();
        }
    }
    
    void Reset() {
        selected_param = 0;
        param_count = 0;
        scroll_offset = 0;
    }
};

} // namespace mutables_ui
