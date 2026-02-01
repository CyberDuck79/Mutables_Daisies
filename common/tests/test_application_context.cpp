// =============================================================================
// test_application_context.cpp
// Tests for UserDataManagerBase interface patterns
// 
// NOTE: ModuleBase and ApplicationContext tests require hardware mocks
// (DaisyPatch) which are not available in this test environment.
// These tests focus on the UserDataManagerBase interface that can be
// tested independently.
// =============================================================================

#include "test_framework.h"
#include "../user_data_manager_base.h"
#include "../parameter.h"
#include <cstring>

using namespace mutables_ui;

namespace {

//=============================================================================
// Mock UserDataManager implementing UserDataManagerBase interface
// This simulates how Plaits (or any module with user data) would implement it
//=============================================================================

class MockUserDataManager : public UserDataManagerBase {
public:
    // Track calls for testing
    int list_files_target_ = -1;
    int load_target_target_ = -1;
    const char* load_target_filename_ = nullptr;
    int load_default_target_ = -1;
    mutable int get_current_target_ = -1;
    
    // Simulated data
    static constexpr int kNumTargets = 3;
    char current_files_[kNumTargets][32] = {"", "", ""};
    
    int ListFiles(int target, char files[][32], int max_files) override {
        list_files_target_ = target;
        
        // Return mock file list
        if (max_files >= 2 && target >= 0 && target < kNumTargets) {
            strcpy(files[0], "file1.bin");
            strcpy(files[1], "file2.bin");
            return 2;
        }
        return 0;
    }
    
    bool LoadTarget(int target, const char* filename) override {
        load_target_target_ = target;
        load_target_filename_ = filename;
        
        if (target >= 0 && target < kNumTargets) {
            strncpy(current_files_[target], filename, 31);
            current_files_[target][31] = '\0';
            return true;
        }
        return false;
    }
    
    bool LoadDefaultForTarget(int target) override {
        load_default_target_ = target;
        
        if (target >= 0 && target < kNumTargets) {
            current_files_[target][0] = '\0';  // Clear = default
            return true;
        }
        return false;
    }
    
    const char* GetCurrentFile(int target) const override {
        get_current_target_ = target;
        
        if (target >= 0 && target < kNumTargets) {
            return current_files_[target];
        }
        return "";
    }
    
    void Reset() {
        list_files_target_ = -1;
        load_target_target_ = -1;
        load_target_filename_ = nullptr;
        load_default_target_ = -1;
        get_current_target_ = -1;
        for (int i = 0; i < kNumTargets; i++) {
            current_files_[i][0] = '\0';
        }
    }
};

} // namespace

//=============================================================================
// UserDataManagerBase Interface Tests
// These test the abstract interface that all user data managers implement
//=============================================================================

TEST(UserDataManager, ListFilesReturnsCorrectCount) {
    MockUserDataManager udm;
    char files[10][32];
    
    int count = udm.ListFiles(1, files, 10);
    
    EXPECT_EQ(count, 2);
    EXPECT_EQ(udm.list_files_target_, 1);
}

TEST(UserDataManager, ListFilesPopulatesArray) {
    MockUserDataManager udm;
    char files[10][32];
    
    udm.ListFiles(0, files, 10);
    
    EXPECT_EQ(strcmp(files[0], "file1.bin"), 0);
    EXPECT_EQ(strcmp(files[1], "file2.bin"), 0);
}

TEST(UserDataManager, LoadTargetSucceeds) {
    MockUserDataManager udm;
    
    bool success = udm.LoadTarget(2, "custom.bin");
    
    EXPECT_TRUE(success);
    EXPECT_EQ(udm.load_target_target_, 2);
    EXPECT_EQ(strcmp(udm.load_target_filename_, "custom.bin"), 0);
}

TEST(UserDataManager, LoadTargetUpdatesCurrentFile) {
    MockUserDataManager udm;
    
    udm.LoadTarget(2, "custom.bin");
    
    EXPECT_EQ(strcmp(udm.GetCurrentFile(2), "custom.bin"), 0);
}

TEST(UserDataManager, LoadDefaultClearsCurrentFile) {
    MockUserDataManager udm;
    
    // First load a file
    udm.LoadTarget(1, "test.bin");
    EXPECT_EQ(strcmp(udm.GetCurrentFile(1), "test.bin"), 0);
    
    // Then load default
    bool success = udm.LoadDefaultForTarget(1);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(udm.load_default_target_, 1);
    EXPECT_EQ(strcmp(udm.GetCurrentFile(1), ""), 0);
}

TEST(UserDataManager, LoadTargetFailsForInvalidTarget) {
    MockUserDataManager udm;
    
    bool success = udm.LoadTarget(99, "test.bin");
    
    EXPECT_FALSE(success);
}

TEST(UserDataManager, LoadDefaultFailsForNegativeTarget) {
    MockUserDataManager udm;
    
    bool success = udm.LoadDefaultForTarget(-1);
    
    EXPECT_FALSE(success);
}

TEST(UserDataManager, GetCurrentFileReturnsEmptyForDefault) {
    MockUserDataManager udm;
    
    // Default state - no file loaded
    const char* file = udm.GetCurrentFile(0);
    
    EXPECT_EQ(strcmp(file, ""), 0);
}

TEST(UserDataManager, MultipleTargetsIndependent) {
    MockUserDataManager udm;
    
    // Load different files for different targets
    udm.LoadTarget(0, "file_a.bin");
    udm.LoadTarget(1, "file_b.bin");
    udm.LoadTarget(2, "file_c.bin");
    
    // Each should retain its own file
    EXPECT_EQ(strcmp(udm.GetCurrentFile(0), "file_a.bin"), 0);
    EXPECT_EQ(strcmp(udm.GetCurrentFile(1), "file_b.bin"), 0);
    EXPECT_EQ(strcmp(udm.GetCurrentFile(2), "file_c.bin"), 0);
}

TEST(UserDataManager, PolymorphicAccess) {
    MockUserDataManager udm;
    
    // Access through base pointer (how ApplicationContext uses it)
    UserDataManagerBase* base_ptr = &udm;
    
    bool success = base_ptr->LoadTarget(1, "poly.bin");
    EXPECT_TRUE(success);
    
    const char* file = base_ptr->GetCurrentFile(1);
    EXPECT_EQ(strcmp(file, "poly.bin"), 0);
}

//=============================================================================
// Parameter USER_DATA Type Tests
// These test the Parameter struct's support for user data
//=============================================================================

TEST(UserDataParam, TypeIsUserData) {
    Parameter param;
    param.type = ParamType::USER_DATA;
    param.name = "Wavetable";
    param.user_data_target = 2;
    param.user_data_filename[0] = '\0';
    
    EXPECT_TRUE(param.type == ParamType::USER_DATA);
    EXPECT_EQ(param.user_data_target, 2);
}

TEST(UserDataParam, SetFilename) {
    Parameter param;
    param.type = ParamType::USER_DATA;
    param.user_data_filename[0] = '\0';
    
    param.SetUserDataFile("custom_wave.bin");
    
    EXPECT_EQ(strcmp(param.user_data_filename, "custom_wave.bin"), 0);
}

TEST(UserDataParam, ClearFilename) {
    Parameter param;
    param.type = ParamType::USER_DATA;
    param.SetUserDataFile("some_file.bin");
    
    // Setting empty string = use default
    param.SetUserDataFile("");
    
    EXPECT_EQ(param.user_data_filename[0], '\0');
}

TEST(UserDataParam, DefaultFilenameIsEmpty) {
    Parameter param;
    param.type = ParamType::USER_DATA;
    param.user_data_filename[0] = '\0';
    
    // Empty means "use built-in default"
    EXPECT_EQ(strlen(param.user_data_filename), 0u);
}

TEST(UserDataParam, TargetIndexStored) {
    Parameter param;
    param.type = ParamType::USER_DATA;
    
    param.user_data_target = 0;  // e.g., FM_BANK
    EXPECT_EQ(param.user_data_target, 0);
    
    param.user_data_target = 1;  // e.g., WAVETABLE
    EXPECT_EQ(param.user_data_target, 1);
    
    param.user_data_target = 2;  // e.g., WAVE_TERRAIN
    EXPECT_EQ(param.user_data_target, 2);
}
