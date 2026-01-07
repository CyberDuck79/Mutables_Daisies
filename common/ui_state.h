#pragma once

#include <cstdint>

namespace mutables_ui {

enum class UIState {
    Navigate,       // Encoder rotation scrolls parameters
    EditValue,      // Encoder rotation changes value
    Submenu,        // CV mapping options (Navigate mode)
    SubmenuEdit     // Editing submenu values
};

enum class SubmenuItem {
    CVSource,       // Select CV input (None, CV1-4)
    Attenuverter,   // Set attenuverter amount (-1.0 to 1.0)
    CaptureOrigin,  // Capture current knob as origin
    Back            // Exit submenu
};

struct MenuState {
    UIState state;
    int selected_param;
    int param_count;
    int scroll_offset;
    
    // Submenu state
    SubmenuItem selected_submenu_item;
    int submenu_param_index;  // Which parameter's submenu we're in
    
    // Display settings
    static constexpr int VISIBLE_PARAMS = 4;
    
    MenuState() 
        : state(UIState::Navigate)
        , selected_param(0)
        , param_count(0)
        , scroll_offset(0)
        , selected_submenu_item(SubmenuItem::CVSource)
        , submenu_param_index(-1) {}
    
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
    
    void EnterSubmenu(int param_index) {
        submenu_param_index = param_index;
        selected_submenu_item = SubmenuItem::CVSource;
        state = UIState::Submenu;
    }
    
    void ExitSubmenu() {
        submenu_param_index = -1;
        state = UIState::Navigate;
    }
    
    bool IsInSubmenu() const {
        return state == UIState::Submenu || state == UIState::SubmenuEdit;
    }
};

} // namespace mutables_ui
