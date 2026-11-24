// S-1 Biome Registry: no biome id literals in production code (AC 5).
// Biome identity lives in Assets/CoW/Global/World/biomes.json; production code under Cluiche/CoW must not name a
// biome as a "Biome.X" string literal. Assets/ (data) and Testing/ (shipped test helpers) are exempt.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path FindCoWRoot()
    {
        std::error_code           ec;
        std::filesystem::path     dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche" / "CoW";
            if (std::filesystem::is_directory(candidate, ec))
            {
                return candidate;
            }
            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) { break; }
            dir = parent;
        }
        return {};
    }

    bool IsExemptPath(const std::filesystem::path& root, const std::filesystem::path& file)
    {
        const std::filesystem::path rel = file.lexically_relative(root);
        for (const auto& part : rel)
        {
            if (part == "Assets" || part == "Testing") { return true; }
        }
        return false;
    }
}

TEST(BiomeLiterals, NoBiomeIdLiteralsInProductionCode)
{
    const std::filesystem::path root = FindCoWRoot();
    ASSERT_FALSE(root.empty()) << "could not locate Cluiche/CoW";

    const std::regex literal("\"Biome\\.[A-Z]");
    std::vector<std::string> offenders;

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec))
    {
        if (ec || !it->is_regular_file()) { continue; }
        const std::filesystem::path& p = it->path();
        const std::string ext = p.extension().string();
        if (ext != ".h" && ext != ".cpp" && ext != ".inl" && ext != ".hpp") { continue; }
        if (IsExemptPath(root, p)) { continue; }

        std::ifstream in(p, std::ios::binary);
        std::string   line;
        int           lineNo = 0;
        while (std::getline(in, line))
        {
            ++lineNo;
            if (std::regex_search(line, literal))
            {
                offenders.push_back(p.lexically_relative(root).generic_string() + ":" + std::to_string(lineNo));
            }
        }
    }

    EXPECT_TRUE(offenders.empty()) << offenders.size() << " biome id literal(s) under Cluiche/CoW:\n" << [&] {
        std::string s;
        for (const std::string& o : offenders) { s += "  " + o + "\n"; }
        return s;
    }();
}
