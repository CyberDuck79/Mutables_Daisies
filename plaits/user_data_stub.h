// Stub for user_data.h - Plaits user data functionality disabled for Daisy port
// Original user_data is for loading samples from flash on original Plaits hardware

#pragma once

#include <cstdint>
#include <cstring>

namespace plaits {

class UserData {
 public:
  UserData() {}
  
  // Stub implementation - no user data support
  // Returns nullptr indicating no user samples loaded
  const uint8_t* ptr(int engine_index) const {
    return nullptr;
  }
  
  static void Load() {}
  static const int16_t* samples() { return nullptr; }
  static size_t sample_count() { return 0; }
};

}  // namespace plaits
