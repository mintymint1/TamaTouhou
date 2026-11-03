#pragma once
// Minimal test framework (no dependencies): TEST(name) { CHECK(...); CHECK_EQ(a, b); }
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

struct TestCase {
  const char *name;
  std::function<void()> fn;
};

inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> r;
  return r;
}

struct Registrar {
  Registrar(const char *n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

inline int &failures() {
  static int f = 0;
  return f;
}

#define TEST(name)                                         \
  static void test_##name();                               \
  static Registrar reg_##name(#name, test_##name);         \
  static void test_##name()

#define CHECK(cond)                                                                   \
  do {                                                                                \
    if (!(cond)) {                                                                    \
      std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
      failures()++;                                                                   \
    }                                                                                 \
  } while (0)

#define CHECK_EQ(a, b)                                                                \
  do {                                                                                \
    auto va_ = (a);                                                                   \
    auto vb_ = (b);                                                                   \
    if (!(va_ == vb_)) {                                                              \
      std::printf("    FAIL %s:%d: %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, \
                  (long long)va_, (long long)vb_);                                    \
      failures()++;                                                                   \
    }                                                                                 \
  } while (0)
