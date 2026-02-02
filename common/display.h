#pragma once

#include "daisy_patch.h"
#include "calibration.h"
#include "parameter.h"
#include "ui_state.h"
#include "constants.h"
#include "renderers/menu_renderer.h"
#include "renderers/mapping_submenu_renderer.h"
#include "renderers/preset_renderer.h"
#include "renderers/calibration_renderer.h"
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
    Display() : hw_(nullptr), logo_draw_func_(nullptr), cpu_overload_(false),
                pending_message_expire_(0) {
        pending_message_.title[0] = '\0';
        pending_message_.message[0] = '\0';
    }
    
    void Init(daisy::DaisyPatch* hw, LogoDrawFunc logo_func = nullptr) {
        hw_ = hw;
        logo_draw_func_ = logo_func;
    }
    
    // Set CPU overload flag (call from main loop with CpuMonitor::IsOverloaded())
    void SetCpuOverload(bool overloaded) {
        cpu_overload_ = overloaded;
    }
    
    //=========================================================================
    // Non-Blocking Message Queue
    //=========================================================================
    
    /**
     * Queue a temporary message to display.
     * The message will be shown for the specified duration without blocking.
     * Call UpdatePendingMessages() each frame to handle display/expiration.
     * 
     * @param title Message title (max 15 chars)
     * @param message Message body (max 31 chars)
     * @param duration_ms How long to show the message (default 1500ms)
     * @param success If true, normal display; if false, error styling
     */
    void QueueMessage(const char* title, const char* message, 
                     uint32_t duration_ms = 1500, bool success = true) {
        strncpy(pending_message_.title, title, sizeof(pending_message_.title) - 1);
        pending_message_.title[sizeof(pending_message_.title) - 1] = '\0';
        strncpy(pending_message_.message, message, sizeof(pending_message_.message) - 1);
        pending_message_.message[sizeof(pending_message_.message) - 1] = '\0';
        pending_message_.success = success;
        pending_message_expire_ = daisy::System::GetNow() + duration_ms;
    }
    
    /**
     * Check if there's a pending message being displayed.
     * @return true if a message is currently showing
     */
    bool HasPendingMessage() const {
        return pending_message_expire_ > 0 && 
               daisy::System::GetNow() < pending_message_expire_;
    }
    
    /**
     * Update and render pending messages. Call this each frame.
     * If a message is pending, it renders the message and returns true.
     * If no message or message expired, returns false (caller should render normal UI).
     * 
     * @return true if a message was rendered (skip normal UI), false otherwise
     */
    bool UpdatePendingMessages() {
        if (!hw_) return false;
        
        uint32_t now = daisy::System::GetNow();
        if (pending_message_expire_ > 0 && now < pending_message_expire_) {
            // Render the pending message
            RenderMessage(pending_message_.title, pending_message_.message, 
                         pending_message_.success);
            hw_->display.Update();
            return true;
        }
        
        // Clear expired message
        if (pending_message_expire_ > 0 && now >= pending_message_expire_) {
            pending_message_expire_ = 0;
            pending_message_.title[0] = '\0';
            pending_message_.message[0] = '\0';
        }
        
        return false;
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
    
    //=========================================================================
    // Calibration Rendering (delegated to CalibrationRenderer)
    //=========================================================================
    
    void RenderCalibration(const MenuState& menu, const SystemCalibration& calibration,
                          float current_raw_cv) {
        if (!hw_) return;
        CalibrationRenderer<decltype(hw_->display)> renderer(hw_->display);
        renderer.Render(menu, calibration, current_raw_cv);
    }
    
private:
    daisy::DaisyPatch* hw_;
    LogoDrawFunc logo_draw_func_;
    bool cpu_overload_;
    
    // Non-blocking message queue
    struct PendingMessage {
        char title[16];
        char message[32];
        bool success;
    };
    PendingMessage pending_message_;
    uint32_t pending_message_expire_;
    
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
