#pragma once

#ifndef TESTS_TESTAPPS_H
#define TESTS_TESTAPPS_H

#include "BaseFrameworkTest.h"

#include <array>

namespace mc::tests {

struct TestEntry {
    const char *name;
    App *(*create)();
};

inline constexpr std::array kTestApps{
    TestEntry{"base", [] -> App * { return new BaseFrameworkTest(); }},
};

}  // namespace mc::tests

#endif
