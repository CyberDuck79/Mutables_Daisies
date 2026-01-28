#pragma once

#include "calibration.h"
#include "sd_dma_buffer.h"
#include "daisy_patch.h"
#include "fatfs.h"
#include <cstring>

// Debug logging - set to nullptr to disable
extern daisy::Logger<daisy::LOGGER_INTERNAL>* g_logger;

namespace mutables_ui {

// Calibration file path (system-wide, not per-module)
static constexpr const char* kCalibrationFilePath = "/system/calibration.bin";
static constexpr const char* kSystemDir = "/system";

class CalibrationManager {
public:
    CalibrationManager() 
        : fsi_(nullptr)
        , initialized_(false)
        , modified_(false) {}
    
    // Initialize with file system interface
    // Call after FatFS is mounted
    void Init(daisy::FatFSInterface& fsi) {
        fsi_ = &fsi;
        initialized_ = true;
        modified_ = false;
        
        // Start with defaults
        calibration_.ResetToDefaults();
    }
    
    // Load calibration from SD card
    // Returns true if loaded successfully, false if using defaults
    bool Load() {
        if (!initialized_ || !fsi_) {
            if (g_logger) g_logger->PrintLine("Cal: Not initialized");
            return false;
        }
        
        // Ensure system directory exists
        EnsureSystemDirectory();
        
        FIL file;
        FRESULT fr = f_open(&file, kCalibrationFilePath, FA_READ);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Cal: No file, using defaults");
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Read into temporary buffer
        SystemCalibration temp;
        UINT bytes_read;
        fr = f_read(&file, &temp, sizeof(SystemCalibration), &bytes_read);
        f_close(&file);
        
        if (fr != FR_OK || bytes_read != sizeof(SystemCalibration)) {
            if (g_logger) g_logger->PrintLine("Cal: Read error, using defaults");
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Validate
        if (!temp.IsValid()) {
            if (g_logger) g_logger->PrintLine("Cal: Invalid data, using defaults");
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Success
        calibration_ = temp;
        modified_ = false;
        
        if (g_logger) {
            g_logger->PrintLine("Cal: Loaded OK");
            for (int i = 0; i < 4; i++) {
                g_logger->PrintLine("  CV%d: %.4f - %.4f", i + 1, 
                    calibration_.cv_inputs[i].min,
                    calibration_.cv_inputs[i].max);
            }
        }
        
        return true;
    }
    
    // Save calibration to SD card
    // NOTE: Audio should be stopped before calling this!
    bool Save() {
        if (!initialized_ || !fsi_) {
            if (g_logger) g_logger->PrintLine("Cal: Not initialized");
            return false;
        }
        
        // Ensure system directory exists
        EnsureSystemDirectory();
        
        // Update checksum
        calibration_.UpdateChecksum();
        
        // Use shared DMA buffer for SD write
        auto& dma_buffer = sd_utils::GetSharedDmaBuffer();
        
        // Copy calibration data to DMA buffer
        if (sizeof(SystemCalibration) > sd_utils::DMA_BUFFER_SIZE) {
            if (g_logger) g_logger->PrintLine("Cal: Buffer too small");
            return false;
        }
        memcpy(dma_buffer.data, &calibration_, sizeof(SystemCalibration));
        
        // Write to file
        FIL file;
        FRESULT fr = f_open(&file, kCalibrationFilePath, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Cal: Open failed: %d", (int)fr);
            return false;
        }
        
        UINT bytes_written;
        fr = f_write(&file, dma_buffer.data, sizeof(SystemCalibration), &bytes_written);
        f_close(&file);
        
        if (fr != FR_OK || bytes_written != sizeof(SystemCalibration)) {
            if (g_logger) g_logger->PrintLine("Cal: Write failed");
            return false;
        }
        
        modified_ = false;
        if (g_logger) g_logger->PrintLine("Cal: Saved OK");
        
        return true;
    }
    
    // Get current calibration data (read-only)
    const SystemCalibration& GetCalibration() const {
        return calibration_;
    }
    
    // Get calibration for a specific CV input
    const CVCalibration& GetCVCalibration(int index) const {
        if (index < 0 || index >= 4) {
            static CVCalibration default_cal;
            return default_cal;
        }
        return calibration_.cv_inputs[index];
    }
    
    // Set calibration for a specific CV input
    void SetCVCalibration(int index, float min_val, float max_val) {
        if (index < 0 || index >= 4) return;
        calibration_.cv_inputs[index].min = min_val;
        calibration_.cv_inputs[index].max = max_val;
        modified_ = true;
    }
    
    // Reset a specific CV to defaults
    void ResetCV(int index) {
        if (index < 0 || index >= 4) return;
        calibration_.cv_inputs[index].Reset();
        modified_ = true;
    }
    
    // Reset all CVs to defaults
    void ResetAll() {
        calibration_.ResetToDefaults();
        modified_ = true;
    }
    
    // Check if calibration has been modified since last save/load
    bool IsModified() const { return modified_; }
    
    // Scale a raw CV value using calibration
    float ScaleCV(int index, float raw) const {
        if (index < 0 || index >= 4) return raw;
        return calibration_.cv_inputs[index].Scale(raw);
    }
    
private:
    daisy::FatFSInterface* fsi_;
    SystemCalibration calibration_;
    bool initialized_;
    bool modified_;
    
    void EnsureSystemDirectory() {
        // Try to create /system directory (ignore if exists)
        f_mkdir(kSystemDir);
    }
};

} // namespace mutables_ui
