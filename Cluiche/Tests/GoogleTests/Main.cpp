// Main.cpp - Google Test entry point for Cluiche GoogleTests
//
// This file provides the main entry point for all Google Test unit tests.
// Google Test will automatically discover and run all tests defined with
// TEST(), TEST_F(), TEST_P(), etc. macros throughout the test project.

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    // Initialize Google Test framework
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    // Returns 0 if all tests pass, non-zero if any test fails
    return RUN_ALL_TESTS();
}
