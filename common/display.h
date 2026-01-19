#pragma once

#include "daisy_patch.h"
#include "parameter.h"
#include "ui_state.h"
#include "constants.h"
#include "renderers/menu_renderer.h"
#include "renderers/mapping_submenu_renderer.h"
#include "renderers/preset_renderer.h"
#include <cstdio>
#include <cstring>

namespace mutables_ui {

using namespace mutables;

// Logo drawing callback type
using LogoDrawFunc = void (*)(daisy::OledDisplay<daisy::SSD130x4WireSpi128x64Driver>&, int, int);

//=============================================================================
// Display - Main display controller using composition
//=============================================================================

class Display {
public:
    Display() : hw_(nullptr), logo_draw_func_(nullptr), cpu_overload_(false) {}
    
    void Init(daisy::DaisyPatch* hw, LogoDrawFunc logo_func = nullptr) {
        hw_ = hw;
        logo_draw_func_ = logo_func;
    }
    
    // Set CPU overload flag (call from main loop with CpuMonitor::IsOverloaded())
    void SetCpuOverload(bool overloaded) {
        cpu_overload_ = overloaded;
    }
    
    //=========================================================================
    // Boot Screen
    //=========================================================================
    
    void RenderBootScreen(const char* module_name, const char* brand_name = "Ducktronics") {
        if (!hw_) return;
        
        hw_->display.Fill(false);
        
        if (logo_draw_func_) {
            // Layout: Logo on left (54x54), text on right (74 pixels)
            // Logo centered vertically: (64-54)/2 = 5
            logo_draw_func_(hw_->display, 0, 5);
            
            // Text area starts at x=56 (54 + 2px margin)
            const int text_x = 56;
            const int text_width = kScreenWidth - text_x;
            
            // Brand name centered in text area, at y=18
            int brand_len = strlen(brand_name);
            int brand_x = text_x + (text_width - brand_len * kFont6x8Width) / 2;
            hw_->display.SetCursor(brand_x, 18);
            hw_->display.WriteString(brand_name, Font_6x8, true);
            
            // Separator line
            hw_->display.DrawLine(text_x + 4, 30, 124, 30, true);
            
            // Module name centered in text area, at y=40
            int name_len = strlen(module_name);
            int name_x = text_x + (text_width - name_len * kFont7x10Width) / 2;
            hw_->display.SetCursor(name_x, 40);
            hw_->display.WriteString(module_name, Font_7x10, true);
        } else {
            // Fallback: centered text only (no logo)
            int text_len = strlen(module_name);
            int x = (kScreenWidth - (text_len * kFont7x10Width)) / 2;
            int y = (kScreenHeight - kFont7x10Height) / 2;
            
            hw_->display.SetCursor(x, y);
            hw_->display.WriteString(module_name, Font_7x10, true);
        }
        
        hw_->display.Update();
    }
    
    //=========================================================================
    // Menu Rendering (delegated to MenuRenderer)
    //=========================================================================
    
    void RenderMenu(const MenuState& menu, Parameter* params) {
        if (!hw_) return;
        MenuRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderMenu(menu, params);
        
        // Draw CPU overload indicator if needed
        if (cpu_overload_) {
            DrawCpuOverloadIndicator();
        }
    }
    
    void RenderSubMenu(const MenuState& menu, Parameter* parent) {
        if (!hw_) return;
        MenuRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderSubMenu(menu, parent);
        
        // Draw CPU overload indicator if needed
        if (cpu_overload_) {
            DrawCpuOverloadIndicator();
        }
    }
    
    //=========================================================================
    // Mapping Submenu Rendering (delegated to MappingSubmenuRenderer)
    //=========================================================================
    
    void RenderSubmenu(const MenuState& menu, Parameter& param) {
        if (!hw_) return;
        MappingSubmenuRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderSubmenu(menu, param);
    }
    
    // Legacy individual methods for compatibility
    void RenderKnobSubmenu(const MenuState& menu, Parameter& param) {
        RenderSubmenu(menu, param);
    }
    
    void RenderCVSubmenu(const MenuState& menu, Parameter& param) {
        RenderSubmenu(menu, param);
    }
    
    void RenderEnumSubmenu(const MenuState& menu, Parameter& param) {
        RenderSubmenu(menu, param);
    }
    
    //=========================================================================
    // Preset/File Rendering (delegated to PresetRenderer)
    //=========================================================================
    
    void RenderCharInput(const MenuState& menu) {
        if (!hw_) return;
        PresetRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderCharInput(menu);
    }
    
    void RenderPresetList(const MenuState& menu, const char* (*getPresetName)(int)) {
        if (!hw_) return;
        PresetRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderPresetList(menu, getPresetName);
    }
    
    void RenderFileBrowser(const MenuState& menu, const char* title, const char* (*getFileName)(int)) {
        if (!hw_) return;
        PresetRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderFileBrowser(menu, title, getFileName);
    }
    
    void RenderMessage(const char* title, const char* message, bool success = true) {
        if (!hw_) return;
        PresetRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.RenderMessage(title, message, success);
    }
    
private:
    daisy::DaisyPatch* hw_;
    LogoDrawFunc logo_draw_func_;
    bool cpu_overload_;
    
    // Draw CPU overload indicator (flashing "!" in top-right corner)
    void DrawCpuOverloadIndicator() {
        if (!hw_) return;
        
        // Flash at ~2Hz using system time
        uint32_t now = daisy::System::GetNow();
        if ((now / 250) % 2 == 0) {
            // Draw inverted "!" indicator in top-right corner
            // Position: x=120-127, y=0-9 (small corner)
            const int x = 121;
            const int y = 0;
            
            // Draw black background
            hw_->display.DrawRect(x - 1, y, 127, 9, true, true);
            
            // Draw "!" in inverse video
            hw_->display.SetCursor(x, y + 1);
            hw_->display.WriteString("!", Font_6x8, false);
        }
        
        // Note: We need to call Update() again since the menu already did
        hw_->display.Update();
    }
};

} // namespace mutables_ui
