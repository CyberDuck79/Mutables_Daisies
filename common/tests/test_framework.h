#pragma once

#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Simple Test Framework
struct Test {
  std::string suite;
  std::string name;
  std::function<void()> func;
};

// Global registry accessor
std::vector<Test> &GetTests();

struct TestRegistrar {
  TestRegistrar(const char *suite, const char *name,
                std::function<void()> func);
};

#define TEST(suite, name)                                                      \
  void suite##_##name();                                                       \
  static TestRegistrar suite##_##name##_reg(#suite, #name, suite##_##name);    \
  void suite##_##name()

// Assertions
#define EXPECT_EQ(a, b)                                                        \
  if ((a) != (b)) {                                                            \
    std::cerr << "FAILED: " << #a << " == " << #b << "\n";                     \
    std::cerr << "  Expected: " << (b) << "\n";                                \
    std::cerr << "  Actual:   " << (a) << "\n";                                \
    std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << "\n";            \
    throw std::runtime_error("Assertion failed");                              \
  }

#define EXPECT_FLOAT_EQ(a, b)                                                  \
  if (std::abs((a) - (b)) > 0.0001f) {                                         \
    std::cerr << "FAILED: " << #a << " near " << #b << "\n";                   \
    std::cerr << "  Expected: " << (b) << "\n";                                \
    std::cerr << "  Actual:   " << (a) << "\n";                                \
    std::cerr << "  Diff:     " << std::abs((a) - (b)) << "\n";                \
    std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << "\n";            \
    throw std::runtime_error("Assertion failed");                              \
  }

#define EXPECT_TRUE(x)                                                         \
  if (!(x)) {                                                                  \
    std::cerr << "FAILED: " << #x << " is true" << "\n";                       \
    std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << "\n";            \
    throw std::runtime_error("Assertion failed");                              \
  }

#define EXPECT_FALSE(x)                                                        \
  if (x) {                                                                     \
    std::cerr << "FAILED: " << #x << " is false" << "\n";                      \
    std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << "\n";            \
    throw std::runtime_error("Assertion failed");                              \
  }

#define EXPECT_STREQ(a, b)                                                     \
  if (std::strcmp((a), (b)) != 0) {                                            \
    std::cerr << "FAILED: " << #a << " == " << #b << "\n";                     \
    std::cerr << "  Expected: \"" << (b) << "\"\n";                            \
    std::cerr << "  Actual:   \"" << (a) << "\"\n";                            \
    std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << "\n";            \
    throw std::runtime_error("Assertion failed");                              \
  }
