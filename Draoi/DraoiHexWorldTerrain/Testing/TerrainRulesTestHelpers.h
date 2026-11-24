// Neutral test helper: locates a data file by walking up from the calling header and the working directory, and aborts
// with a message when it cannot (a broken checkout, not a test result). Nothing here names a game, so a game-neutral
// package can ship it. The shipped sidecar, TestRules() and TestSession live with the
// application that owns those assets, seed names and guarantees.
// Header-only.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Draoi {
namespace HexWorld {

namespace TerrainRulesTestDetail
{
    inline void Fail(const char* what, const char* detail)
    {
        std::fprintf(stderr, "TerrainRulesTestHelpers: %s%s\n", what, detail);
        std::fflush(stderr);
        std::abort();
    }

    // The text of 'relativePath' found by walking up from 'anchorFile' (pass __FILE__) and then from the working directory;
    // empty when not found.
    inline std::string ReadFileFromSourceTree(const char* relativePath, const char* anchorFile)
    {
        std::error_code ec;
        const std::filesystem::path starts[] = { std::filesystem::absolute(std::filesystem::path(anchorFile), ec).parent_path(),
                                                 std::filesystem::current_path(ec) };
        for (std::filesystem::path dir : starts)
        {
            while (!dir.empty())
            {
                const std::filesystem::path candidate = dir / relativePath;
                if (std::filesystem::exists(candidate, ec))
                {
                    std::ifstream     in(candidate, std::ios::binary);
                    std::stringstream ss;
                    ss << in.rdbuf();
                    return ss.str();
                }
                if (dir == dir.parent_path())
                {
                    break;
                }
                dir = dir.parent_path();
            }
        }
        return {};
    }
}

} // namespace HexWorld
} // namespace Draoi
