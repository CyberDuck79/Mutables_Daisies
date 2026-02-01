#pragma once

#include <cstdint>

namespace mutables_ui {

//=============================================================================
// UserDataManagerBase - Abstract interface for module-specific user data
//=============================================================================
//
// PURPOSE:
// Some Mutable Instruments modules support loading custom user data from SD:
//   - Plaits: FM banks, wavetables, wave terrains
//   - Braids: User wavetables (if ported)
//   - Others: May have similar needs
//
// This interface allows the common ApplicationContext to interact with
// module-specific user data without knowing the details.
//
// IMPLEMENTATION GUIDE FOR NEW PORTS:
// 1. If your module has NO user data: Don't implement this. Return nullptr
//    from ModuleBase::GetUserDataManager().
//
// 2. If your module HAS user data:
//    a) Create a class inheriting from UserDataManagerBase
//    b) Define your Target enum for different data slots
//    c) Implement all virtual methods
//    d) Return a pointer from ModuleBase::GetUserDataManager()
//
// EXAMPLE (Plaits):
//    class UserDataManager : public UserDataManagerBase {
//        enum Target { TARGET_SIX_OP_1, TARGET_WAVETABLE, ... };
//        int ListFiles(int target, ...) override { ... }
//    };
//
//=============================================================================

class UserDataManagerBase {
public:
    virtual ~UserDataManagerBase() = default;
    
    //=========================================================================
    // File Listing
    //=========================================================================
    // List available files for a given target (data slot).
    // 
    // @param target      Module-specific target ID (cast from your enum)
    // @param files       Output array of filenames (32 chars each max)
    // @param max_files   Maximum number of files to return
    // @return            Number of files found
    //
    // The files array should be populated with filenames (without path).
    // First entry (index 0) will be shown as "Default" by the UI.
    //=========================================================================
    virtual int ListFiles(int target, char files[][32], int max_files) = 0;
    
    //=========================================================================
    // Load Specific File
    //=========================================================================
    // Load a named file for the given target.
    //
    // @param target    Module-specific target ID
    // @param filename  Filename to load (without path)
    // @return          true if successful
    //
    // The implementation should:
    // 1. Construct the full path: /<module>/user_data/<target_dir>/<filename>
    // 2. Read the file into the appropriate buffer
    // 3. Mark the target as loaded
    //=========================================================================
    virtual bool LoadTarget(int target, const char* filename) = 0;
    
    //=========================================================================
    // Load Default
    //=========================================================================
    // Load the default/firmware data for the given target.
    //
    // @param target  Module-specific target ID
    // @return        true if successful
    //
    // This should either:
    // - Load "default.bin" from SD if it exists
    // - Fall back to firmware-embedded defaults
    //=========================================================================
    virtual bool LoadDefaultForTarget(int target) = 0;
    
    //=========================================================================
    // Get Current File
    //=========================================================================
    // Get the currently loaded filename for a target.
    //
    // @param target  Module-specific target ID
    // @return        Filename (empty string if using default)
    //=========================================================================
    virtual const char* GetCurrentFile(int target) const = 0;
};

} // namespace mutables_ui
