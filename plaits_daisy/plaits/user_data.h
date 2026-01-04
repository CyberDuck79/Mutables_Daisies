/**
 * Daisy-compatible user_data stub
 * 
 * This file provides a stub implementation of the Plaits UserData class
 * for the Daisy platform. It replaces the STM32F3-specific flash access
 * with a simple null implementation.
 * 
 * In the future, this could be implemented to use Daisy's QSPI flash
 * for storing custom wavetables.
 */

#ifndef PLAITS_DAISY_USER_DATA_H_
#define PLAITS_DAISY_USER_DATA_H_

#include "stmlib/stmlib.h"

namespace plaits {

class UserData {
 public:
  enum {
    ADDRESS = 0x08007000,  // Not used on Daisy
    SIZE = 0x1000
  };

  UserData() { }
  ~UserData() { }

  // Stub implementation - always returns NULL (no user data)
  inline const uint8_t* ptr(int slot) const {
    return NULL;
  }
  
  // Stub implementation - saving not supported yet
  inline bool Save(uint8_t* rx_buffer, int slot) {
    return false;
  }
};

}  // namespace plaits

#endif  // PLAITS_DAISY_USER_DATA_H_
