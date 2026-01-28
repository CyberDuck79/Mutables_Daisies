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
        
        FRESULT fr = f_open(&file_, kCalibrationFilePath, FA_READ);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Cal: No file (fr=%d), using defaults", (int)fr);
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Log file size for debugging
        FSIZE_t file_size = f_size(&file_);
        if (g_logger) g_logger->PrintLine("Cal: File opened, size=%u", (unsigned int)file_size);
        
        // Use shared DMA buffer for SD read (required for proper SD card access)
        auto& dma_buffer = sd_utils::GetSharedDmaBuffer();
        
        if (sizeof(SystemCalibration) > sd_utils::DMA_BUFFER_SIZE) {
            if (g_logger) g_logger->PrintLine("Cal: Buffer too small");
            f_close(&file_);
            calibration_.ResetToDefaults();
            return false;
        }
        
        UINT bytes_read;
        fr = f_read(&file_, dma_buffer.data, sizeof(SystemCalibration), &bytes_read);
        f_close(&file_);
        
        if (fr != FR_OK || bytes_read != sizeof(SystemCalibration)) {
            if (g_logger) {
                g_logger->PrintLine("Cal: Read error (fr=%d, read=%u, expect=%u)", 
                    (int)fr, (unsigned int)bytes_read, (unsigned int)sizeof(SystemCalibration));
            }
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Copy from DMA buffer to temp struct for validation
        SystemCalibration temp;
        memcpy(&temp, dma_buffer.data, sizeof(SystemCalibration));
        
        // Validate
        if (!temp.IsValid()) {
            if (g_logger) {
                g_logger->PrintLine("Cal: Invalid data, using defaults");
                g_logger->PrintLine("  Magic: 0x%08X (expect 0x%08X)", 
                    (unsigned int)temp.magic, (unsigned int)kCalibrationMagic);
                g_logger->PrintLine("  Version: %d (expect %d)", 
                    (int)temp.version, (int)kCalibrationVersion);
                g_logger->PrintLine("  Checksum: %u (calc: %u)", 
                    (unsigned int)temp.checksum, (unsigned int)temp.CalculateChecksum());
            }
            calibration_.ResetToDefaults();
            return false;
        }
        
        // Success
        calibration_ = temp;
        modified_ = false;
        
        if (g_logger) {
            g_logger->PrintLine("Cal: Loaded OK");
            for (int i = 0; i < 4; i++) {
                // Use integer formatting for floats (embedded platform)
                int min_whole = static_cast<int>(calibration_.cv_inputs[i].min);
                int min_frac = static_cast<int>((calibration_.cv_inputs[i].min - min_whole) * 10000);
                int max_whole = static_cast<int>(calibration_.cv_inputs[i].max);
                int max_frac = static_cast<int>((calibration_.cv_inputs[i].max - max_whole) * 10000);
                g_logger->PrintLine("  CV%d: %d.%04d - %d.%04d", i + 1, 
                    min_whole, min_frac, max_whole, max_frac);
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
        
        // Write to file (use DMA-aligned file handle)
        FRESULT fr = f_open(&file_, kCalibrationFilePath, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Cal: Open failed: %d", (int)fr);
            return false;
        }
        
        UINT bytes_written;
        fr = f_write(&file_, dma_buffer.data, sizeof(SystemCalibration), &bytes_written);
        if (fr != FR_OK || bytes_written != sizeof(SystemCalibration)) {
            if (g_logger) g_logger->PrintLine("Cal: Write failed (fr=%d, wrote=%u)", 
                (int)fr, (unsigned int)bytes_written);
            f_close(&file_);
            return false;
        }
        
        // Close file (this flushes buffers to disk)
        fr = f_close(&file_);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Cal: Close failed: %d", (int)fr);
            return false;
        }
        
        // Verify file was written correctly
        FILINFO fno;
        fr = f_stat(kCalibrationFilePath, &fno);
        if (fr != FR_OK || fno.fsize != sizeof(SystemCalibration)) {
            if (g_logger) g_logger->PrintLine("Cal: Verify failed (fr=%d, size=%u)", 
                (int)fr, (unsigned int)fno.fsize);
            return false;
        }
        
        modified_ = false;
        if (g_logger) {
            g_logger->PrintLine("Cal: Saved OK (%u bytes)", (unsigned int)bytes_written);
            g_logger->PrintLine("  Checksum: %u", (unsigned int)calibration_.checksum);
            for (int i = 0; i < 4; i++) {
                int min_whole = static_cast<int>(calibration_.cv_inputs[i].min);
                int min_frac = static_cast<int>((calibration_.cv_inputs[i].min - min_whole) * 10000);
                int max_whole = static_cast<int>(calibration_.cv_inputs[i].max);
                int max_frac = static_cast<int>((calibration_.cv_inputs[i].max - max_whole) * 10000);
                g_logger->PrintLine("  CV%d: %d.%04d - %d.%04d", i + 1, 
                    min_whole, min_frac, max_whole, max_frac);
            }
        }
        
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
    alignas(32) FIL file_;  // DMA-aligned file handle for SD operations
    
    void EnsureSystemDirectory() {
        // Try to create /system directory (ignore if exists)
        f_mkdir(kSystemDir);
    }
};

} // namespace mutables_ui
