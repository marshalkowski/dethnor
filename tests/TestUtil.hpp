#pragma once

// Minimal assert-and-count helpers shared by the headless test executables
// (same hand-rolled, framework-free style as Bengine's own EngineTests.cpp).

#include <cmath>
#include <cstdio>
#include <string>

namespace testutil {

inline int failures = 0;

inline void Fail(const std::string& message) {
    std::printf("FAIL: %s\n", message.c_str());
    ++failures;
}

inline void Check(bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

inline bool Near(float a, float b, float epsilon = 1.0e-4f) { return std::fabs(a - b) < epsilon; }

// Prints the summary line and returns the process exit code.
inline int Finish(const char* name) {
    std::printf(failures == 0 ? "%s passed\n" : "%s FAILED (%d)\n", name, failures);
    return failures == 0 ? 0 : 1;
}

} // namespace testutil
