// Minimal zero-dependency test harness (no doctest/gtest, offline-friendly).
//
// Usage: in exactly ONE .cpp, define PHONE2PAD_TEST_MAIN before including this
// header to emit main(). Register cases with TEST_CASE("name") { ... } and assert
// with CHECK(cond) / CHECK_EQ(a, b). main() runs all registered cases and returns
// non-zero on any failure.
#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace phone2pad::test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

// Per-run counters, accessed from CHECK macros and main().
inline int& failure_count() {
    static int n = 0;
    return n;
}
inline int& check_count() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(Case{name, std::move(fn)});
    }
};

inline void report_failure(const char* file, int line, const std::string& msg) {
    ++failure_count();
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg.c_str());
}

}  // namespace phone2pad::test

#define PP_CONCAT_(a, b) a##b
#define PP_CONCAT(a, b) PP_CONCAT_(a, b)

#define TEST_CASE(NAME)                                                            \
    static void PP_CONCAT(pp_test_fn_, __LINE__)();                                \
    static ::phone2pad::test::Registrar PP_CONCAT(pp_test_reg_, __LINE__)(        \
        NAME, &PP_CONCAT(pp_test_fn_, __LINE__));                                  \
    static void PP_CONCAT(pp_test_fn_, __LINE__)()

#define CHECK(COND)                                                                \
    do {                                                                           \
        ++::phone2pad::test::check_count();                                       \
        if (!(COND)) {                                                             \
            ::phone2pad::test::report_failure(__FILE__, __LINE__,                 \
                                               "CHECK(" #COND ") failed");         \
        }                                                                          \
    } while (0)

#define CHECK_EQ(A, B)                                                             \
    do {                                                                           \
        ++::phone2pad::test::check_count();                                       \
        auto&& _a = (A);                                                           \
        auto&& _b = (B);                                                           \
        if (!(_a == _b)) {                                                         \
            std::ostringstream _oss;                                               \
            _oss << "CHECK_EQ(" #A ", " #B ") failed";                             \
            ::phone2pad::test::report_failure(__FILE__, __LINE__, _oss.str());    \
        }                                                                          \
    } while (0)

#ifdef PHONE2PAD_TEST_MAIN
int main() {
    using namespace phone2pad::test;
    int cases_with_failures = 0;
    for (const Case& c : registry()) {
        const int before = failure_count();
        std::fprintf(stderr, "[ RUN  ] %s\n", c.name.c_str());
        c.fn();
        const int added = failure_count() - before;
        if (added == 0) {
            std::fprintf(stderr, "[  OK  ] %s\n", c.name.c_str());
        } else {
            ++cases_with_failures;
            std::fprintf(stderr, "[ FAIL ] %s (%d failures)\n", c.name.c_str(), added);
        }
    }
    std::fprintf(stderr, "\n%zu cases, %d checks, %d failures\n",
                 registry().size(), check_count(), failure_count());
    return cases_with_failures == 0 ? 0 : 1;
}
#endif
