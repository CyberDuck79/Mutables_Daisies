#pragma once

#include "daisy_patch.h"
#include "../calibration.h"
#include "../ui_state.h"
#include "../constants.h"
#include <cstdio>

namespace mutables_ui {

using namespace mutables;

//=============================================================================
// CalibrationRenderer - CV Input Calibration UI
//=============================================================================

// Helper to format float as integer parts (for embedded platforms without float printf)
// Prints value like "0.0345" using 4 decimal places
inline void FormatFloatAsInt(float value, char* buf, size_t size) {
    // Handle negative values
    bool negative = value < 0.0f;
    if (negative) value = -value;
    
    // Split into integer and fractional parts (4 decimal places = 10000)
    int whole = static_cast<int>(value);
    int frac = static_cast<int>((value - whole) * 10000.0f + 0.5f);
    
    // Handle rounding overflow
    if (frac >= 10000) {
        frac = 0;
        whole++;
    }
    
    if (negative) {
        snprintf(buf, size, "-%d.%04d", whole, frac);
    } else {
        snprintf(buf, size, "%d.%04d", whole, frac);
    }
}

template<typename DisplayType>
class CalibrationRenderer {
public:
    CalibrationRenderer(DisplayType& display) : display_(display) {}
    
    // Render calibration UI based on current step
    void Render(const MenuState& menu, const SystemCalibration& calibration, 
                float current_raw_cv) {
        display_.Fill(false);
        
        switch (menu.calibration_step) {
            case CalibrationStep::SelectCV:
                RenderMenu(menu, calibration);
                break;
            case CalibrationStep::CaptureMin:
                RenderCapture(menu, "Turn to MIN", current_raw_cv, true);
                break;
            case CalibrationStep::CaptureMax:
                RenderCapture(menu, "Turn to MAX", current_raw_cv, false);
                break;
            case CalibrationStep::Confirm:
                RenderConfirm(menu);
                break;
        }
        
        display_.Update();
    }
    
private:
    DisplayType& display_;
    
    void RenderMenu(const MenuState& menu, const SystemCalibration& calibration) {
        // Title bar with back option - matches RenderSubMenu style
        bool title_selected = (menu.calibration_selected == -1);
        char title[20];
        if (title_selected) {
            snprintf(title, sizeof(title), "< Calibrate");
            display_.DrawRect(0, 0, kScreenWidth - 1, 9, true, true);
            display_.SetCursor(0, 1);
            display_.WriteString(title, Font_6x8, false);  // Inverted
        } else {
            snprintf(title, sizeof(title), "Calibrate");
            display_.SetCursor(0, 1);
            display_.WriteString(title, Font_6x8, true);
            display_.DrawLine(0, 10, kScreenWidth - 1, 10, true);
        }
        
        // Calculate visible range with scrolling
        int visible_start = menu.calibration_scroll_offset;
        int max_visible = 4;  // Only 4 items fit below title
        
        // Menu items with scrolling
        int y = 12;
        int rendered = 0;
        for (int i = visible_start; i < kCalibrationMenuItemCount && rendered < max_visible; i++) {
            CalibrationMenuItem item = static_cast<CalibrationMenuItem>(i);
            bool selected = (menu.calibration_selected == i);
            
            const char* item_name = GetCalibrationMenuItemName(item);
            int name_len = strlen(item_name);
            
            display_.SetCursor(2, y);
            display_.WriteString(item_name, Font_7x10, true);
            
            // Draw selection underline (based on text width)
            if (selected) {
                display_.DrawLine(2, y + 10, 2 + name_len * kFont7x10Width - 1, y + 10, true);
            }
            
            // Show current calibration values for CV1-4
            if (i < 4) {
                char buf[16];
                const auto& cv_cal = calibration.cv_inputs[i];
                if (cv_cal.IsDefault()) {
                    snprintf(buf, sizeof(buf), "default");
                } else {
                    // Show min-max range using integer formatting
                    int min_whole = static_cast<int>(cv_cal.min);
                    int min_frac = static_cast<int>((cv_cal.min - min_whole) * 100.0f + 0.5f);
                    int max_whole = static_cast<int>(cv_cal.max);
                    int max_frac = static_cast<int>((cv_cal.max - max_whole) * 100.0f + 0.5f);
                    snprintf(buf, sizeof(buf), "%d.%02d-%d.%02d", 
                             min_whole, min_frac, max_whole, max_frac);
                }
                display_.SetCursor(60, y);
                display_.WriteString(buf, Font_6x8, true);
            }
            
            y += kCompactLineHeight;
            rendered++;
        }
    }
    
    void RenderCapture(const MenuState& menu, const char* instruction, 
                       float current_raw_cv, bool is_min) {
        char buf[32];
        
        // Title - smaller font like submenus
        snprintf(buf, sizeof(buf), "Calibrate CV%d", menu.calibration_cv_index + 1);
        display_.SetCursor(0, 1);
        display_.WriteString(buf, Font_6x8, true);
        display_.DrawLine(0, 10, kScreenWidth - 1, 10, true);
        
        // Check if timed capture is active
        bool capturing = menu.IsCaptureActive();
        
        if (capturing) {
            // Show "Capturing..." and the running min/max value
            display_.SetCursor(0, 14);
            display_.WriteString("Capturing...", Font_7x10, true);
            
            // Show the running captured value
            float running_value = menu.GetCaptureRunningValue();
            FormatFloatAsInt(running_value, buf, sizeof(buf));
            int text_width = strlen(buf) * kFont7x10Width;
            int x = (kScreenWidth - text_width) / 2;
            display_.SetCursor(x, 26);
            display_.WriteString(buf, Font_7x10, true);
            
            // Progress bar - we need the progress, but we don't have time here
            // So we'll show current raw value position instead
            int bar_y = 38;
            int bar_width = 100;
            int bar_x = (kScreenWidth - bar_width) / 2;
            
            // Draw bar outline
            display_.DrawRect(bar_x, bar_y, bar_x + bar_width, bar_y + 6, true, false);
            
            // Draw filled portion based on current raw value
            int fill_width = static_cast<int>(current_raw_cv * (bar_width - 2));
            if (fill_width > 0) {
                display_.DrawRect(bar_x + 1, bar_y + 1, 
                                bar_x + 1 + fill_width, bar_y + 5, true, true);
            }
            
            // Show hold steady message
            display_.SetCursor(0, 48);
            display_.WriteString("Hold position...", Font_6x8, true);
            
        } else {
            // Not capturing yet - show instruction to start
            display_.SetCursor(0, 14);
            display_.WriteString(instruction, Font_7x10, true);
            
            // Current raw value (large, centered) - using integer formatting
            FormatFloatAsInt(current_raw_cv, buf, sizeof(buf));
            int text_width = strlen(buf) * kFont7x10Width;
            int x = (kScreenWidth - text_width) / 2;
            display_.SetCursor(x, 26);
            display_.WriteString(buf, Font_7x10, true);
            
            // Visual bar showing raw value position
            int bar_y = 38;
            int bar_width = 100;
            int bar_x = (kScreenWidth - bar_width) / 2;
            
            // Draw bar outline
            display_.DrawRect(bar_x, bar_y, bar_x + bar_width, bar_y + 6, true, false);
            
            // Draw filled portion based on current value
            int fill_width = static_cast<int>(current_raw_cv * (bar_width - 2));
            if (fill_width > 0) {
                display_.DrawRect(bar_x + 1, bar_y + 1, 
                                bar_x + 1 + fill_width, bar_y + 5, true, true);
            }
            
            // Show captured min if we're capturing max
            if (!is_min && menu.calibration_captured_min > 0.0f) {
                display_.SetCursor(0, 48);
                display_.WriteString("Min:", Font_6x8, true);
                FormatFloatAsInt(menu.calibration_captured_min, buf, sizeof(buf));
                display_.SetCursor(28, 48);
                display_.WriteString(buf, Font_6x8, true);
            }
            
            // Instructions at bottom
            display_.SetCursor(0, 56);
            display_.WriteString("Press to capture", Font_6x8, true);
        }
    }
    
    void RenderConfirm(const MenuState& menu) {
        char buf[32];
        
        // Title - smaller font like submenus
        snprintf(buf, sizeof(buf), "CV%d Calibrated", menu.calibration_cv_index + 1);
        display_.SetCursor(0, 1);
        display_.WriteString(buf, Font_6x8, true);
        display_.DrawLine(0, 10, kScreenWidth - 1, 10, true);
        
        // Show captured values using integer formatting
        display_.SetCursor(0, 16);
        display_.WriteString("Min:", Font_7x10, true);
        FormatFloatAsInt(menu.calibration_captured_min, buf, sizeof(buf));
        display_.SetCursor(40, 16);
        display_.WriteString(buf, Font_7x10, true);
        
        display_.SetCursor(0, 30);
        display_.WriteString("Max:", Font_7x10, true);
        FormatFloatAsInt(menu.calibration_captured_max, buf, sizeof(buf));
        display_.SetCursor(40, 30);
        display_.WriteString(buf, Font_7x10, true);
        
        // Instructions
        display_.SetCursor(0, 43);
        display_.WriteString("Press: OK", Font_6x8, true);
        display_.SetCursor(0, 55);
        display_.WriteString("Turn: Retry", Font_6x8, true);
    }
};

} // namespace mutables_ui
