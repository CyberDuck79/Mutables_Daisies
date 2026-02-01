#pragma once

#include "daisy_patch.h"
#include "fatfs.h"
#include "../eurorack/plaits/user_data.h"
#include "../common/sd_dma_buffer.h"  // Shared DMA buffer for SD operations
#include "../common/user_data_manager_base.h"  // Base interface for ApplicationContext
#include <cstring>
#include <cstdint>

// Debug logging - uses shared logger from preset_manager.h
extern daisy::Logger<daisy::LOGGER_INTERNAL>* g_logger;

namespace mutables_plaits {

/**
 * UserDataManager - Manages user data for Plaits engines
 * 
 * Implements:
 * - plaits::UserDataProvider: Integration with Plaits Voice class
 * - mutables_ui::UserDataManagerBase: Integration with ApplicationContext
 * 
 * User data targets (engine slots):
 *   0-1: Not used
 *   2: Six-op FM bank 1 (engine index 2)
 *   3: Six-op FM bank 2 (engine index 3)
 *   4: Six-op FM bank 3 (engine index 4)
 *   5: Wave terrain (engine index 5)
 *   13: Wavetable (engine index 13)
 * 
 * Each target stores 4096 bytes of user data.
 * Data is loaded from SD card at boot and can be reloaded on demand.
 * 
 * SD card structure:
 *   /plaits/user_data/six_op_bank_1/default.bin (or named .bin files)
 *   /plaits/user_data/six_op_bank_2/default.bin
 *   /plaits/user_data/six_op_bank_3/default.bin
 *   /plaits/user_data/wavetable/default.bin
 *   /plaits/user_data/wave_terrain/default.bin
 * 
 * IMPORTANT: SD card DMA requires buffers in AXI SRAM (not DTCMRAM).
 * Uses the shared DMA buffer from sd_dma_buffer.h.
 */
class UserDataManager : public plaits::UserDataProvider, 
                        public mutables_ui::UserDataManagerBase {
public:
    // User data size (matches original Plaits)
    static constexpr size_t DATA_SIZE = 4096;
    
    // Number of user data targets
    static constexpr size_t NUM_TARGETS = 5;
    
    // Target indices
    enum Target {
        TARGET_SIX_OP_1 = 0,      // Engine slot 2
        TARGET_SIX_OP_2 = 1,      // Engine slot 3
        TARGET_SIX_OP_3 = 2,      // Engine slot 4
        TARGET_WAVE_TERRAIN = 3,  // Engine slot 5
        TARGET_WAVETABLE = 4      // Engine slot 13
    };
    
    // Engine slot to target mapping
    static Target SlotToTarget(int slot) {
        switch (slot) {
            case 2: return TARGET_SIX_OP_1;
            case 3: return TARGET_SIX_OP_2;
            case 4: return TARGET_SIX_OP_3;
            case 5: return TARGET_WAVE_TERRAIN;
            case 13: return TARGET_WAVETABLE;
            default: return TARGET_SIX_OP_1;  // Invalid slot
        }
    }
    
    // Check if engine slot uses user data
    static bool SlotHasUserData(int slot) {
        return slot == 2 || slot == 3 || slot == 4 || slot == 5 || slot == 13;
    }
    
    UserDataManager()
        : initialized_(false)
        , fsi_(nullptr) {
        // Initialize all data buffers to zero and mark as not loaded
        for (size_t i = 0; i < NUM_TARGETS; ++i) {
            memset(data_[i], 0, DATA_SIZE);
            loaded_[i] = false;
            current_file_[i][0] = '\0';
        }
    }
    
    /**
     * Initialize the manager
     * Call after SD card is mounted
     */
    bool Init(daisy::FatFSInterface& fsi, const char* module_name) {
        fsi_ = &fsi;
        
        // Build base path: /<module>/user_data/
        snprintf(base_path_, sizeof(base_path_), "/%s/user_data", module_name);
        
        initialized_ = true;
        return true;
    }
    
    /**
     * Check if manager is initialized and ready
     */
    bool IsInitialized() const {
        return initialized_;
    }
    
    /**
     * Load default user data for all targets
     * Call this at startup after Init()
     * If default.bin doesn't exist for a target, firmware built-in data is used
     * Returns number of targets successfully loaded from SD
     */
    int LoadDefaults() {
        if (!initialized_) return 0;
        
        int loaded_count = 0;
        
        // Try to load each target's default file
        // If file doesn't exist, ptr() returns nullptr and voice.cc uses built-in data
        if (LoadTarget(TARGET_SIX_OP_1, "default.bin")) loaded_count++;
        if (LoadTarget(TARGET_SIX_OP_2, "default.bin")) loaded_count++;
        if (LoadTarget(TARGET_SIX_OP_3, "default.bin")) loaded_count++;
        if (LoadTarget(TARGET_WAVE_TERRAIN, "default.bin")) loaded_count++;
        if (LoadTarget(TARGET_WAVETABLE, "default.bin")) loaded_count++;
        
        if (g_logger) {
            g_logger->PrintLine("UserData: %d/%d from SD", 
                               loaded_count, (int)NUM_TARGETS);
        }
        
        return loaded_count;
    }
    
    /**
     * Load user data for a specific target from a file
     * @param target The target to load
     * @param filename The filename (within the target's directory)
     * @return true if loaded successfully
     * 
     * Uses DMA-compatible buffer for SD card reads, then copies to final storage.
     */
    bool LoadTarget(Target target, const char* filename) {
        if (!initialized_ || !fsi_ || target >= NUM_TARGETS) {
            return false;
        }
        
        // Build full path
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s/%s/%s",
                 base_path_, GetTargetDirName(target), filename);
        
        // Open file
        FIL file;
        FRESULT fr = f_open(&file, filepath, FA_READ);
        if (fr != FR_OK) {
            if (g_logger && strcmp(filename, "default.bin") != 0) {
                g_logger->PrintLine("UserData: Cannot open %s (%d)", filepath, (int)fr);
            }
            return false;
        }
        
        // Check file size
        FSIZE_t file_size = f_size(&file);
        if (file_size != DATA_SIZE) {
            if (g_logger) {
                g_logger->PrintLine("UserData: Wrong size %lu (expect %lu)", 
                                   (unsigned long)file_size, (unsigned long)DATA_SIZE);
            }
            f_close(&file);
            return false;
        }
        
        // Read data using shared DMA-compatible buffer (in AXI SRAM)
        auto& dma_buffer = sd_utils::GetSharedDmaBuffer();
        UINT bytes_read;
        fr = f_read(&file, dma_buffer.data, DATA_SIZE, &bytes_read);
        f_close(&file);
        
        if (fr != FR_OK || bytes_read != DATA_SIZE) {
            if (g_logger) {
                g_logger->PrintLine("UserData: Read error %s (%d)", filepath, (int)fr);
            }
            loaded_[target] = false;
            return false;
        }
        
        // Copy from DMA buffer to final storage
        memcpy(data_[target], dma_buffer.data, DATA_SIZE);
        
        // Mark as loaded and store filename
        loaded_[target] = true;
        strncpy(current_file_[target], filename, sizeof(current_file_[target]) - 1);
        current_file_[target][sizeof(current_file_[target]) - 1] = '\0';
        
        if (g_logger) {
            g_logger->PrintLine("UserData: Loaded %s/%s", 
                               GetTargetDirName(target), filename);
        }
        
        return true;
    }
    
    /**
     * Load user data by engine slot number
     * @param slot Engine slot (2-4 for FM, 5 for terrain, 13 for wavetable)
     * @param filename The filename to load
     * @return true if loaded successfully
     */
    bool LoadForSlot(int slot, const char* filename) {
        if (!SlotHasUserData(slot)) return false;
        return LoadTarget(SlotToTarget(slot), filename);
    }
    
    /**
     * Clear loaded data for a target, reverting to firmware defaults
     * @param target The target to clear
     * @return true (always succeeds)
     */
    bool LoadDefaultForTarget(Target target) {
        if (target >= NUM_TARGETS) return false;
        
        // Clear the loaded flag so ptr() returns nullptr
        // This causes voice.cc to use built-in firmware data
        loaded_[target] = false;
        current_file_[target][0] = '\0';
        
        if (g_logger) {
            g_logger->PrintLine("UserData: Cleared %s (using firmware default)", 
                               GetTargetDirName(target));
        }
        
        return true;
    }
    
    /**
     * Get pointer to user data for an engine slot
     * Implements plaits::UserDataProvider interface
     * This is the main interface used by Voice::LoadUserData()
     * NOTE: This is called from the audio callback - NO LOGGING HERE!
     * @param slot Engine slot number
     * @return Pointer to data if loaded, nullptr otherwise
     */
    const uint8_t* ptr(int slot) const override {
        if (!SlotHasUserData(slot)) {
            return nullptr;
        }
        
        Target target = SlotToTarget(slot);
        if (!loaded_[target]) {
            return nullptr;
        }
        
        return data_[target];
    }
    
    /**
     * Check if user data is loaded for an engine slot
     */
    bool IsLoaded(int slot) const {
        if (!SlotHasUserData(slot)) return false;
        return loaded_[SlotToTarget(slot)];
    }
    
    /**
     * Get current filename for a target
     */
    const char* GetCurrentFile(Target target) const {
        if (target >= NUM_TARGETS) return "";
        return current_file_[target];
    }
    
    /**
     * List available files for a target
     * Fills the provided array with filenames
     * @param target The target to list files for
     * @param files Array to fill with filenames
     * @param max_files Maximum number of files to return
     * @param max_name_len Maximum length of each filename
     * @return Number of files found
     */
    int ListFiles(Target target, char files[][32], size_t max_files, size_t max_name_len = 32) {
        if (!initialized_ || !fsi_ || target >= NUM_TARGETS) {
            return 0;
        }
        
        // Build directory path
        char dirpath[128];
        snprintf(dirpath, sizeof(dirpath), "%s/%s", base_path_, GetTargetDirName(target));
        
        DIR dir;
        FRESULT fr = f_opendir(&dir, dirpath);
        if (fr != FR_OK) {
            return 0;
        }
        
        size_t count = 0;
        FILINFO fno;
        
        while (count < max_files) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == '\0') {
                break;  // End of directory or error
            }
            
            // Skip directories
            if (fno.fattrib & AM_DIR) continue;
            
            // Skip hidden files (starting with '.')
            if (fno.fname[0] == '.') continue;
            
            // Check for .bin extension
            size_t len = strlen(fno.fname);
            if (len < 5) continue;
            if (strcasecmp(fno.fname + len - 4, ".bin") != 0) continue;
            
            // Copy filename
            strncpy(files[count], fno.fname, max_name_len - 1);
            files[count][max_name_len - 1] = '\0';
            count++;
        }
        
        f_closedir(&dir);
        return count;
    }
    
    //=========================================================================
    // UserDataManagerBase Interface Implementation
    //=========================================================================
    // These methods allow ApplicationContext to work with this manager
    // without knowing Plaits-specific details. They convert int -> Target.
    //=========================================================================
    
    /**
     * List files for a target (UserDataManagerBase interface)
     * @param target Target index (cast from Target enum)
     * @param files Output array of filenames
     * @param max_files Maximum number of files
     * @return Number of files found
     */
    int ListFiles(int target, char files[][32], int max_files) override {
        if (target < 0 || target >= static_cast<int>(NUM_TARGETS)) return 0;
        return ListFiles(static_cast<Target>(target), files, static_cast<size_t>(max_files), 32);
    }
    
    /**
     * Load a specific file (UserDataManagerBase interface)
     * @param target Target index
     * @param filename Filename to load
     * @return true if successful
     */
    bool LoadTarget(int target, const char* filename) override {
        if (target < 0 || target >= static_cast<int>(NUM_TARGETS)) return false;
        return LoadTarget(static_cast<Target>(target), filename);
    }
    
    /**
     * Load default for target (UserDataManagerBase interface)
     * @param target Target index
     * @return true if successful
     */
    bool LoadDefaultForTarget(int target) override {
        if (target < 0 || target >= static_cast<int>(NUM_TARGETS)) return false;
        return LoadDefaultForTarget(static_cast<Target>(target));
    }
    
    /**
     * Get current file for target (UserDataManagerBase interface)
     * @param target Target index
     * @return Current filename (empty if using default)
     */
    const char* GetCurrentFile(int target) const override {
        if (target < 0 || target >= static_cast<int>(NUM_TARGETS)) return "";
        return GetCurrentFile(static_cast<Target>(target));
    }
    
    /**
     * Create the directory structure on SD card
     */
    bool CreateDirectories() {
        if (!initialized_ || !fsi_) return false;
        
        // Create base directory
        f_mkdir(base_path_);
        
        // Create target directories
        char dirpath[128];
        for (size_t i = 0; i < NUM_TARGETS; ++i) {
            snprintf(dirpath, sizeof(dirpath), "%s/%s", base_path_, GetTargetDirName((Target)i));
            f_mkdir(dirpath);
        }
        
        return true;
    }
    
private:
    /**
     * Get directory name for a target
     */
    static const char* GetTargetDirName(Target target) {
        switch (target) {
            case TARGET_SIX_OP_1: return "six_op_bank_1";
            case TARGET_SIX_OP_2: return "six_op_bank_2";
            case TARGET_SIX_OP_3: return "six_op_bank_3";
            case TARGET_WAVE_TERRAIN: return "wave_terrain";
            case TARGET_WAVETABLE: return "wavetable";
            default: return "";
        }
    }
    
    // Initialization state
    bool initialized_;
    daisy::FatFSInterface* fsi_;
    
    // Base path for user data
    char base_path_[64];
    
    // Data buffers for each target (final storage, can be in any RAM)
    // Must be aligned for int16_t access since wavetable data is accessed as int16_t
    alignas(4) uint8_t data_[NUM_TARGETS][DATA_SIZE];
    
    // Loaded state for each target
    bool loaded_[NUM_TARGETS];
    
    // Current filename for each target
    char current_file_[NUM_TARGETS][32];
};

}  // namespace mutables_plaits
