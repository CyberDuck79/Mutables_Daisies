#pragma once

#include "daisy_patch.h"
#include "fatfs.h"
#include "parameter.h"
#include <cstring>
#include <cstdint>

// Debug logging - set to nullptr to disable
extern daisy::Logger<daisy::LOGGER_INTERNAL>* g_logger;

namespace mutables_ui {

// Preset file format constants
static constexpr uint32_t PRESET_MAGIC = 0x4D495044;  // "MIPD" - Mutable Instruments Patch Daisy
static constexpr uint16_t PRESET_VERSION = 1;
static constexpr size_t MAX_PRESET_NAME = 16;
static constexpr size_t MAX_PRESETS = 99;             // Practical limit for embedded RAM
static constexpr size_t MAX_PARAMS = 32;

// Binary format for a single parameter's mapping config
struct MappingData {
    uint8_t source;           // MappingSource enum value
    int8_t cc_number;
    uint8_t plugged;          // bool as uint8
    uint8_t trigger;          // TriggerMode enum value
    uint8_t action;           // EnumAction enum value
    uint8_t padding[3];       // Align to 8 bytes
    float offset;
    float attenuverter;
    float velocity_amount;
};  // 20 bytes

// Binary format for a single parameter
struct ParameterData {
    float value;
    MappingData mapping;
};  // 24 bytes

// Preset file header
struct PresetHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t param_count;
    uint32_t data_size;       // Size of parameter data following header
    uint32_t checksum;        // Simple sum checksum
};  // 16 bytes

// Preset list entry (for browsing)
struct PresetEntry {
    char name[MAX_PRESET_NAME + 1];
};

class PresetManager {
public:
    PresetManager() 
        : initialized_(false)
        , sd_mounted_(false)
        , sd_init_attempted_(false)
        , sdmmc_(nullptr)
        , fsi_(nullptr)
        , preset_count_(0) {
        module_name_[0] = '\0';
        preset_dir_[0] = '\0';
    }
    
    // Initialize SD card and file system
    // Call this once at startup after hardware init
    bool Init(daisy::SdmmcHandler& sdmmc, daisy::FatFSInterface& fsi, const char* module_name) {
        // Store references for lazy init
        sdmmc_ = &sdmmc;
        fsi_ = &fsi;
        
        // Store module name
        strncpy(module_name_, module_name, sizeof(module_name_) - 1);
        module_name_[sizeof(module_name_) - 1] = '\0';
        
        // Build preset directory path: /<module>/presets/
        snprintf(preset_dir_, sizeof(preset_dir_), "/%s/presets", module_name_);
        
        initialized_ = true;
        sd_mounted_ = false;  // Will be set true on first successful access
        sd_init_attempted_ = false;
        
        return true;  // Always return true - SD check is deferred
    }
    
    // Lazy SD card initialization - called on first access
    bool EnsureSDMounted() {
        if (sd_mounted_) return true;
        if (!initialized_ || !sdmmc_ || !fsi_) {
            if (g_logger) g_logger->PrintLine("SD: Not initialized");
            return false;
        }
        if (sd_init_attempted_) {
            return false;  // Don't retry if already failed
        }
        
        sd_init_attempted_ = true;
        if (g_logger) g_logger->PrintLine("SD: Starting init...");
        
        // Initialize SDMMC peripheral
        // NOTE: Using SLOW speed and 1-bit mode because DMA mode in libDaisy
        // has issues - we modified sd_diskio.c to use polling instead
        daisy::SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd_cfg.speed = daisy::SdmmcHandler::Speed::SLOW;  // 400kHz for reliability
        sd_cfg.width = daisy::SdmmcHandler::BusWidth::BITS_1;
        
        auto sd_result = sdmmc_->Init(sd_cfg);
        if (sd_result != daisy::SdmmcHandler::Result::OK) {
            if (g_logger) g_logger->PrintLine("SD: SDMMC init failed: %d", (int)sd_result);
            return false;
        }
        if (g_logger) g_logger->PrintLine("SD: SDMMC OK");
        
        // Setup FatFS interface
        fsi_->Init(daisy::FatFSInterface::Config::MEDIA_SD);
        
        // Mount filesystem with immediate mount (1)
        FRESULT fr = f_mount(&fsi_->GetSDFileSystem(), "/", 1);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("SD: Mount failed: %d", (int)fr);
            return false;
        }
        if (g_logger) g_logger->PrintLine("SD: Mount OK");
        
        sd_mounted_ = true;
        
        // Create directory structure if needed
        EnsureDirectoryExists();
        if (g_logger) g_logger->PrintLine("SD: Ready");
        
        return true;
    }
    
    // Check if SD card is available (triggers lazy init)
    bool IsSDAvailable() {
        if (!sd_mounted_ && !sd_init_attempted_) {
            EnsureSDMounted();
        }
        return sd_mounted_;
    }
    
    // Check if initialized (may be initialized without SD)
    bool IsInitialized() const { return initialized_; }
    
    // Save current parameters to a preset file
    // NOTE: Audio should be stopped before calling this!
    bool SavePreset(const char* preset_name, Parameter* params, size_t param_count) {
        if (g_logger) g_logger->PrintLine("Save: Starting...");
        
        if (!EnsureSDMounted() || !preset_name || !params || param_count == 0) {
            if (g_logger) g_logger->PrintLine("Save: Mount/param check failed");
            return false;
        }
        
        // Build file path
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "%s/%s.bin", preset_dir_, preset_name);
        if (g_logger) g_logger->PrintLine("Save: Path=%s", filepath);
        
        // Create a single contiguous buffer for all data (like official example uses stack)
        // Total size: header (16) + params (24 * count)
        size_t actual_count = (param_count > MAX_PARAMS) ? MAX_PARAMS : param_count;
        size_t total_size = sizeof(PresetHeader) + actual_count * sizeof(ParameterData);
        
        // Use a static buffer in DMA-safe memory
        static uint8_t DMA_BUFFER_MEM_SECTION write_buffer[sizeof(PresetHeader) + MAX_PARAMS * sizeof(ParameterData)];
        
        // Fill header at start of buffer
        PresetHeader* header = reinterpret_cast<PresetHeader*>(write_buffer);
        header->magic = PRESET_MAGIC;
        header->version = PRESET_VERSION;
        header->param_count = static_cast<uint16_t>(actual_count);
        header->data_size = static_cast<uint32_t>(actual_count * sizeof(ParameterData));
        
        // Fill params after header and calculate checksum
        ParameterData* param_data = reinterpret_cast<ParameterData*>(write_buffer + sizeof(PresetHeader));
        uint32_t checksum = 0;
        
        for (size_t i = 0; i < actual_count; i++) {
            SerializeParameter(params[i], param_data[i]);
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&param_data[i]);
            for (size_t j = 0; j < sizeof(ParameterData); j++) {
                checksum += bytes[j];
            }
        }
        header->checksum = checksum;
        
        // Open file for writing
        FIL file;
        if (g_logger) g_logger->PrintLine("Save: Opening file...");
        FRESULT fr = f_open(&file, filepath, FA_CREATE_ALWAYS | FA_WRITE);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Save: Open failed: %d", (int)fr);
            return false;
        }
        
        // Write everything in one call (like official example)
        UINT bytes_written;
        if (g_logger) g_logger->PrintLine("Save: Writing %d bytes...", (int)total_size);
        fr = f_write(&file, write_buffer, total_size, &bytes_written);
        if (fr != FR_OK || bytes_written != total_size) {
            if (g_logger) g_logger->PrintLine("Save: Write failed, fr=%d, written=%u", (int)fr, bytes_written);
            f_close(&file);
            return false;
        }
        if (g_logger) g_logger->PrintLine("Save: Write done (%u bytes)", bytes_written);
        
        // Close file
        if (g_logger) g_logger->PrintLine("Save: Closing...");
        fr = f_close(&file);
        if (g_logger) g_logger->PrintLine("Save: Close result fr=%d", (int)fr);
        
        // Verify file was created
        FILINFO fno;
        FRESULT stat_fr = f_stat(filepath, &fno);
        if (g_logger) g_logger->PrintLine("Save: Verify stat fr=%d, size=%lu", (int)stat_fr, fno.fsize);
        
        return (stat_fr == FR_OK && fno.fsize > 0);
        
        return (fr == FR_OK && fno.fsize > 0);
    }
    
    // Load parameters from a preset file
    bool LoadPreset(const char* preset_name, Parameter* params, size_t param_count) {
        if (g_logger) g_logger->PrintLine("Load: Starting for '%s'", preset_name);
        
        if (!EnsureSDMounted() || !preset_name || !params || param_count == 0) {
            if (g_logger) g_logger->PrintLine("Load: Invalid args or SD not mounted");
            return false;
        }
        
        // Build file path
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "%s/%s.bin", preset_dir_, preset_name);
        if (g_logger) g_logger->PrintLine("Load: Path '%s'", filepath);
        
        // Check file exists and get size
        FILINFO fno;
        FRESULT fr = f_stat(filepath, &fno);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Load: File not found, fr=%d", (int)fr);
            return false;
        }
        if (g_logger) g_logger->PrintLine("Load: File size=%lu bytes", fno.fsize);
        
        // Open file for reading
        FIL file;
        fr = f_open(&file, filepath, FA_READ);
        if (fr != FR_OK) {
            if (g_logger) g_logger->PrintLine("Load: Open failed, fr=%d", (int)fr);
            return false;
        }
        if (g_logger) g_logger->PrintLine("Load: File opened, fptr=%lu, fsize=%lu", 
                                          (unsigned long)f_tell(&file), (unsigned long)f_size(&file));
        
        // Read header
        PresetHeader header;
        UINT bytes_read;
        fr = f_read(&file, &header, sizeof(PresetHeader), &bytes_read);
        if (fr != FR_OK || bytes_read != sizeof(PresetHeader)) {
            if (g_logger) g_logger->PrintLine("Load: Header read failed, fr=%d, bytes=%u", (int)fr, bytes_read);
            f_close(&file);
            return false;
        }
        if (g_logger) g_logger->PrintLine("Load: Header - magic=0x%08X, version=%u, params=%u, checksum=0x%08X", 
                                          header.magic, header.version, header.param_count, header.checksum);
        
        // Validate header
        if (header.magic != PRESET_MAGIC) {
            if (g_logger) g_logger->PrintLine("Load: Bad magic (expected 0x%08X)", PRESET_MAGIC);
            f_close(&file);
            return false;
        }
        
        // Version check (allow loading older versions)
        if (header.version > PRESET_VERSION) {
            if (g_logger) g_logger->PrintLine("Load: Version too new");
            f_close(&file);
            return false;
        }
        
        // Read parameters (up to what we have)
        uint32_t checksum = 0;
        size_t params_to_read = (header.param_count < param_count) ? header.param_count : param_count;
        if (g_logger) g_logger->PrintLine("Load: Reading %u params (file has %u, we need %u)", 
                                          (unsigned)params_to_read, header.param_count, (unsigned)param_count);
        
        for (size_t i = 0; i < params_to_read; i++) {
            ParameterData pd;
            fr = f_read(&file, &pd, sizeof(ParameterData), &bytes_read);
            if (fr != FR_OK || bytes_read != sizeof(ParameterData)) {
                if (g_logger) g_logger->PrintLine("Load: Param %u read failed", (unsigned)i);
                f_close(&file);
                return false;
            }
            
            // Add to checksum verification
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&pd);
            for (size_t j = 0; j < sizeof(ParameterData); j++) {
                checksum += bytes[j];
            }
            
            // Deserialize into parameter (preserve type and name)
            DeserializeParameter(pd, params[i]);
            if (g_logger && i < 3) {
                g_logger->PrintLine("Load: Param[%u] value=%.3f", (unsigned)i, pd.value);
            }
        }
        if (g_logger) g_logger->PrintLine("Load: Computed checksum=0x%08X (expected=0x%08X)", checksum, header.checksum);
        
        // Skip any extra parameters in file (newer preset with more params)
        for (size_t i = params_to_read; i < header.param_count; i++) {
            ParameterData pd;
            f_read(&file, &pd, sizeof(ParameterData), &bytes_read);
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&pd);
            for (size_t j = 0; j < sizeof(ParameterData); j++) {
                checksum += bytes[j];
            }
        }
        
        f_close(&file);
        if (g_logger) g_logger->PrintLine("Load: File closed");
        
        // Verify checksum
        if (checksum != header.checksum) {
            if (g_logger) g_logger->PrintLine("Load: CHECKSUM MISMATCH!");
            return false;
        }
        
        if (g_logger) g_logger->PrintLine("Load: SUCCESS!");
        return true;
    }
    
    // Scan directory for preset files, returns count
    int ScanPresets() {
        preset_count_ = 0;
        
        if (!EnsureSDMounted()) {
            return 0;
        }
        
        DIR dir;
        FILINFO fno;
        
        FRESULT fr = f_opendir(&dir, preset_dir_);
        if (fr != FR_OK) {
            return 0;
        }
        
        while (preset_count_ < MAX_PRESETS) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) {
                break;  // End of directory or error
            }
            
            // Skip directories
            if (fno.fattrib & AM_DIR) {
                continue;
            }
            
            // Check for .bin extension
            size_t len = strlen(fno.fname);
            if (len > 4 && strcmp(fno.fname + len - 4, ".bin") == 0) {
                // Copy name without extension
                size_t name_len = len - 4;
                if (name_len > MAX_PRESET_NAME) {
                    name_len = MAX_PRESET_NAME;
                }
                strncpy(presets_[preset_count_].name, fno.fname, name_len);
                presets_[preset_count_].name[name_len] = '\0';
                preset_count_++;
            }
        }
        
        f_closedir(&dir);
        return preset_count_;
    }
    
    // Get preset count (after ScanPresets)
    int GetPresetCount() const { return preset_count_; }
    
    // Get preset name by index
    const char* GetPresetName(int index) const {
        if (index < 0 || index >= preset_count_) {
            return nullptr;
        }
        return presets_[index].name;
    }
    
private:
    bool initialized_;
    bool sd_mounted_;
    bool sd_init_attempted_;
    daisy::SdmmcHandler* sdmmc_;
    daisy::FatFSInterface* fsi_;
    char module_name_[16];
    char preset_dir_[32];
    
    // Preset list cache
    PresetEntry presets_[MAX_PRESETS];
    int preset_count_;
    
    // Create directory structure if needed
    void EnsureDirectoryExists() {
        if (!sd_mounted_) return;
        
        // Create module directory
        char module_dir[24];
        snprintf(module_dir, sizeof(module_dir), "/%s", module_name_);
        f_mkdir(module_dir);  // Ignore error if exists
        
        // Create presets subdirectory
        f_mkdir(preset_dir_);  // Ignore error if exists
    }
    
    // Serialize parameter to binary format
    void SerializeParameter(const Parameter& param, ParameterData& data) {
        data.value = param.value;
        
        data.mapping.source = static_cast<uint8_t>(param.mapping.source);
        data.mapping.cc_number = static_cast<int8_t>(param.mapping.cc_number);
        data.mapping.plugged = param.mapping.plugged ? 1 : 0;
        data.mapping.trigger = static_cast<uint8_t>(param.mapping.trigger);
        data.mapping.action = static_cast<uint8_t>(param.mapping.action);
        data.mapping.offset = param.mapping.offset;
        data.mapping.attenuverter = param.mapping.attenuverter;
        data.mapping.velocity_amount = param.mapping.velocity_amount;
        
        // Clear padding
        data.mapping.padding[0] = 0;
        data.mapping.padding[1] = 0;
        data.mapping.padding[2] = 0;
    }
    
    // Deserialize binary format to parameter (preserves type, name, enum_labels)
    void DeserializeParameter(const ParameterData& data, Parameter& param) {
        param.value = data.value;
        
        // Clamp value to valid range
        if (param.value < param.min) param.value = param.min;
        if (param.value > param.max) param.value = param.max;
        
        param.mapping.source = static_cast<MappingSource>(data.mapping.source);
        param.mapping.cc_number = static_cast<int>(data.mapping.cc_number);
        param.mapping.plugged = data.mapping.plugged != 0;
        param.mapping.trigger = static_cast<TriggerMode>(data.mapping.trigger);
        param.mapping.action = static_cast<EnumAction>(data.mapping.action);
        param.mapping.offset = data.mapping.offset;
        param.mapping.attenuverter = data.mapping.attenuverter;
        param.mapping.velocity_amount = data.mapping.velocity_amount;
        
        // Reset runtime state that shouldn't be persisted
        param.mapping.last_gate_state = false;
        param.mapping.toggle_state = false;
    }
};

} // namespace mutables_ui
