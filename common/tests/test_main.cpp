#include "test_framework.h"

std::vector<Test> &GetTests() {
  static std::vector<Test> tests;
  return tests;
}

TestRegistrar::TestRegistrar(const char *suite, const char *name,
                             std::function<void()> func) {
  GetTests().push_back({suite, name, func});
}

int main() {
  std::cout << "Running " << GetTests().size() << " tests...\n";
  int passed = 0;
  int failed = 0;

  for (const auto &test : GetTests()) {
    try {
      test.func();
      // std::cout << "[PASS] " << test.suite << "." << test.name << "\n";
      passed++;
    } catch (...) {
      std::cout << "[FAIL] " << test.suite << "." << test.name << "\n";
      failed++;
    }
  }

  std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
  return failed > 0 ? 1 : 0;
}
