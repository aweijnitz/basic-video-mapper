#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace simple_catch {

using TestFunc = std::function<void()>;

struct TestCase {
  std::string name;
  TestFunc func;
};

class TestRegistry {
 public:
  static TestRegistry& instance();

  void add(std::string name, TestFunc func);

  int runAll() const;

 private:
  std::vector<TestCase> tests;
};

class AutoReg {
 public:
  AutoReg(std::string name, TestFunc func);
};

}  // namespace simple_catch

#define SC_INTERNAL_CONCAT_IMPL(x, y) x##y
#define SC_INTERNAL_CONCAT(x, y) SC_INTERNAL_CONCAT_IMPL(x, y)
#define SC_INTERNAL_UNIQUE_NAME(base) SC_INTERNAL_CONCAT(base, __LINE__)

#define TEST_CASE(name, tags)                                                             \
  static void SC_INTERNAL_UNIQUE_NAME(test_case_func_)();                                 \
  static ::simple_catch::AutoReg SC_INTERNAL_UNIQUE_NAME(auto_reg_)(                      \
      name, SC_INTERNAL_UNIQUE_NAME(test_case_func_));                                    \
  static void SC_INTERNAL_UNIQUE_NAME(test_case_func_)()

#define REQUIRE(expr)                                                                      \
  do {                                                                                     \
    if (!(expr)) {                                                                         \
      throw std::runtime_error(std::string("REQUIRE failed: ") + #expr);                  \
    }                                                                                      \
  } while (false)

namespace simple_catch {

inline TestRegistry& TestRegistry::instance() {
  static TestRegistry registry;
  return registry;
}

inline void TestRegistry::add(std::string name, TestFunc func) {
  tests.push_back(TestCase{std::move(name), std::move(func)});
}

inline int TestRegistry::runAll() const {
  int failures = 0;
  for (const auto& test : tests) {
    try {
      test.func();
      std::cout << "[ ok ] " << test.name << "\n";
    } catch (const std::exception& ex) {
      ++failures;
      std::cout << "[fail] " << test.name << ": " << ex.what() << "\n";
    }
  }
  std::cout << "Ran " << tests.size() << " test(s)." << std::endl;
  return failures == 0 ? 0 : 1;
}

inline AutoReg::AutoReg(std::string name, TestFunc func) {
  TestRegistry::instance().add(std::move(name), std::move(func));
}

}  // namespace simple_catch

