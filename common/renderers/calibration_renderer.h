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
        // Title bar
        display_.SetCursor(0, 1);
        display_.WriteString("CV Calibration", Font_7x10, true);
        display_.DrawLine(0, 12, kScreenWidth - 1, 12, true);
        
        // Menu items
        int y = 14;
        for (int i = 0; i < kCalibrationMenuItemCount && y < kScreenHeight - 2; i++) {
            CalibrationMenuItem item = static_cast<CalibrationMenuItem>(i);
            bool selected = (menu.calibration_selected == item);
            
            // Draw selection indicator
            if (selected) {
                display_.DrawLine(0, y + 10, 70, y + 10, true);
            }
            
            display_.SetCursor(2, y);
            display_.WriteString(GetCalibrationMenuItemName(item), Font_7x10, true);
            
            // Show current calibration values for CV1-4
            if (i < 4) {
                char buf[16];
                const auto& cv_cal = calibration.cv_inputs[i];
                if (cv_cal.IsDefault()) {
                    snprintf(buf, sizeof(buf), "default");
                } else {
                    // Show min-max range
                    snprintf(buf, sizeof(buf), "%.2f-%.2f", cv_cal.min, cv_cal.max);
                }
                display_.SetCursor(72, y);
                display_.WriteString(buf, Font_6x8, true);
            }
            
            y += kCompactLineHeight;
        }
    }
    
    void RenderCapture(const MenuState& menu, const char* instruction, 
                       float current_raw_cv, bool is_min) {
        char buf[32];
        
        // Title
        snprintf(buf, sizeof(buf), "Calibrate CV%d", menu.calibration_cv_index + 1);
        display_.SetCursor(0, 1);
        display_.WriteString(buf, Font_7x10, true);
        display_.DrawLine(0, 12, kScreenWidth - 1, 12, true);
        
        // Instruction
        display_.SetCursor(0, 18);
        display_.WriteString(instruction, Font_7x10, true);
        
        // Current raw value (large, centered)
        snprintf(buf, sizeof(buf), "%.4f", current_raw_cv);
        int text_width = strlen(buf) * kFont7x10Width;
        int x = (kScreenWidth - text_width) / 2;
        display_.SetCursor(x, 32);
        display_.WriteString(buf, Font_7x10, true);
        
        // Visual bar showing raw value position
        int bar_y = 48;
        int bar_width = 100;
        int bar_x = (kScreenWidth - bar_width) / 2;
        
        // Draw bar outline
        display_.DrawRect(bar_x, bar_y, bar_x + bar_width, bar_y + 8, true, false);
        
        // Draw filled portion based on current value
        int fill_width = static_cast<int>(current_raw_cv * (bar_width - 2));
        if (fill_width > 0) {
            display_.DrawRect(bar_x + 1, bar_y + 1, 
                            bar_x + 1 + fill_width, bar_y + 7, true, true);
        }
        
        // Show captured min if we're capturing max
        if (!is_min && menu.calibration_captured_min > 0.0f) {
            snprintf(buf, sizeof(buf), "Min: %.4f", menu.calibration_captured_min);
            display_.SetCursor(0, 56);
            display_.WriteString(buf, Font_6x8, true);
        }
        
        // Instructions at bottom
        display_.SetCursor(72, 56);
        display_.WriteString("Press OK", Font_6x8, true);
    }
    
    void RenderConfirm(const MenuState& menu) {
        char buf[32];
        
        // Title
        snprintf(buf, sizeof(buf), "CV%d Calibrated", menu.calibration_cv_index + 1);
        display_.SetCursor(0, 1);
        display_.WriteString(buf, Font_7x10, true);
        display_.DrawLine(0, 12, kScreenWidth - 1, 12, true);
        
        // Show captured values
        display_.SetCursor(0, 20);
        display_.WriteString("Min:", Font_7x10, true);
        snprintf(buf, sizeof(buf), "%.4f", menu.calibration_captured_min);
        display_.SetCursor(60, 20);
        display_.WriteString(buf, Font_7x10, true);
        
        display_.SetCursor(0, 34);
        display_.WriteString("Max:", Font_7x10, true);
        snprintf(buf, sizeof(buf), "%.4f", menu.calibration_captured_max);
        display_.SetCursor(60, 34);
        display_.WriteString(buf, Font_7x10, true);
        
        // Instructions
        display_.SetCursor(0, 52);
        display_.WriteString("Press: OK", Font_6x8, true);
        display_.SetCursor(64, 52);
        display_.WriteString("Turn: Retry", Font_6x8, true);
    }
};

} // namespace mutables_ui
