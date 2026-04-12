// Main.cpp - Google Test entry point for Cluiche GoogleTests
//
// This file provides the main entry point for all Google Test unit tests.
// Google Test will automatically discover and run all tests defined with
// TEST(), TEST_F(), TEST_P(), etc. macros throughout the test project.
//
// DIRTY TEST TRACKING:
// If .dirty/dirty_tests.json exists, automatically runs only dirty tests.
// Otherwise, runs all tests. Override with --gtest_filter flag.

#include <gtest/gtest.h>
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>

bool LoadDirtyTestFilter(std::string& outFilter)
{
    // Check for environment variable first (for Visual Studio debugging)
    const char* envPath = std::getenv("DIRTY_TESTS_JSON");
    if (envPath != nullptr)
    {
        std::ifstream file(envPath);
        if (file.is_open())
        {
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errs;

            if (Json::parseFromStream(builder, file, &root, &errs) &&
                root.isMember("gtest_filter") && root["gtest_filter"].isString())
            {
                outFilter = root["gtest_filter"].asString();
                return true;
            }
        }
    }

    // Path to dirty_tests.json (relative to exe location)
    // Exe is at: Cluiche/bin/exe/Debug/GoogleTests.exe
    // JSON is at: Cluiche/Tests/GoogleTests/.dirty/dirty_tests.json
    const char* dirtyJsonPath = "../../../Tests/GoogleTests/.dirty/dirty_tests.json";

    std::ifstream file(dirtyJsonPath);
    if (!file.is_open())
    {
        // Try alternate path if running from different directory
        dirtyJsonPath = "../../Tests/GoogleTests/.dirty/dirty_tests.json";
        file.open(dirtyJsonPath);
        if (!file.is_open())
        {
            // Try from project directory
            dirtyJsonPath = ".dirty/dirty_tests.json";
            file.open(dirtyJsonPath);
            if (!file.is_open())
            {
                std::cerr << "[DirtyTests] Could not find dirty_tests.json at:" << std::endl;
                std::cerr << "  - ../../../Tests/GoogleTests/.dirty/dirty_tests.json" << std::endl;
                std::cerr << "  - ../../Tests/GoogleTests/.dirty/dirty_tests.json" << std::endl;
                std::cerr << "  - .dirty/dirty_tests.json" << std::endl;
                return false;
            }
        }
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;

    if (!Json::parseFromStream(builder, file, &root, &errs))
    {
        std::cerr << "[DirtyTests] Warning: Could not parse dirty_tests.json: " << errs << std::endl;
        return false;
    }

    if (root.isMember("gtest_filter") && root["gtest_filter"].isString())
    {
        outFilter = root["gtest_filter"].asString();
        return true;
    }

    return false;
}

int main(int argc, char** argv)
{
    // Initialize Google Test framework
    ::testing::InitGoogleTest(&argc, argv);

    // If no explicit --gtest_filter was provided, try to load dirty test filter
    std::string currentFilter = ::testing::GTEST_FLAG(filter);

    if (currentFilter == "*" || currentFilter.empty())
    {
        std::string dirtyFilter;
        if (LoadDirtyTestFilter(dirtyFilter))
        {
            if (!dirtyFilter.empty() && dirtyFilter != "*")
            {
                ::testing::GTEST_FLAG(filter) = dirtyFilter;

                std::cout << "=======================================" << std::endl;
                std::cout << "[DirtyTests] Running DIRTY tests only" << std::endl;
                std::cout << "=======================================" << std::endl;
                std::cout << "To run all tests: GoogleTests.exe --gtest_filter=*" << std::endl;
                std::cout << "To reset baseline: git rev-parse --short HEAD > .dirty/baseline.txt" << std::endl;
                std::cout << std::endl;
            }
            else
            {
                std::cout << "[DirtyTests] No dirty tests found, running all tests" << std::endl;
            }
        }
        else
        {
            std::cout << "[DirtyTests] No dirty_tests.json found, running all tests" << std::endl;
            std::cout << "  (Dirty tracking activates after building the project)" << std::endl;
        }
    }
    else
    {
        std::cout << "[DirtyTests] Using explicit filter: " << currentFilter << std::endl;
    }

    // Run all tests (with dirty filter applied if loaded)
    // Returns 0 if all tests pass, non-zero if any test fails
    return RUN_ALL_TESTS();
}
