// User data provider global - defined for Daisy platform
// This file provides the global pointer that the plaits::UserData class uses

#include "../eurorack/plaits/user_data.h"

namespace plaits {

// Global pointer to user data provider
// Set this to your UserDataManager instance in main.cpp
UserDataProvider* g_user_data_provider = nullptr;

}  // namespace plaits
