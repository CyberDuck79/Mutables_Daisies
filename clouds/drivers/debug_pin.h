#pragma once

#include "stmlib/stmlib.h"

namespace clouds {

class DebugPin {
 public:
  DebugPin() { }
  ~DebugPin() { }
  static void Init() { }
  static void High() { }
  static void Low() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugPin);
};

#define TIC
#define TOC

}  // namespace clouds
