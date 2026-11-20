#pragma once
#include "gtest/gtest.h"

// Death tests rely on DIA_ASSERT, which compiles out in Release. DIA_EXPECT_DEATH / DIA_ASSERT_DEATH
// run the real gtest death check in Debug (DIA_DEBUG) and compile to nothing in Release, so the
// functional assertions around them still run in both configurations.
#ifdef DIA_DEBUG
#define DIA_EXPECT_DEATH(statement, regex) EXPECT_DEATH(statement, regex)
#define DIA_ASSERT_DEATH(statement, regex) ASSERT_DEATH(statement, regex)
#else
#define DIA_EXPECT_DEATH(statement, regex) ((void)0)
#define DIA_ASSERT_DEATH(statement, regex) ((void)0)
#endif
