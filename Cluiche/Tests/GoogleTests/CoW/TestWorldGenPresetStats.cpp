// S-1 world quality from cell data (decision 2026-11-13): per shipped preset, mean stats over seeds 1..8 at Small.
// Prints a table; asserts only intent-level orderings between presets (see docs/BACKLOG.md 'World-sim cell-data verification').
#include <gtest/gtest.h>
#include <DiaMaths/Core/RandomStream.h>   // before module headers: windows.h defines min/max macros
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/WorldHexGrid.h>
#include <Shared/WorldGeneration/Testing/SessionDrive.h>

#include <DiaCore/Json/external/json/json.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;

namespace
{
    namespace fs = std::filesystem;

    std::string FindShippedPresetDir()   // same lookup as TestWorldGenSizes.cpp (its helpers are file-local)
    {
        std::error_code ec;
        fs::path        dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const fs::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (fs::exists(candidate, ec)) { return candidate.string(); }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    struct Stats { double landFraction = 0, landmasses = 0, largestShare = 0, riverCells = 0, lakeCells = 0, mountainFraction = 0; };

    Stats Measure(const WorldStructure& w)
    {
        const int n = w.CellCount();
        WorldHexGrid grid(w.dimensions);
        std::vector<int> comp(n, -1);
        int land = 0, rivers = 0, lakes = 0, mountain = 0, comps = 0, largest = 0;
        for (int i = 0; i < n; ++i)
        {
            const Cell& c = w.Cells()[i];
            if (c.waterSource == WaterSource::Ocean) { continue; }
            ++land;
            if (c.waterSource == WaterSource::River) { ++rivers; }
            if (c.waterSource == WaterSource::Lake) { ++lakes; }
            if (c.terrain == TerrainType::Mountain || c.terrain == TerrainType::Peak || c.terrain == TerrainType::Cliff) { ++mountain; }   // steep ground classifies as Cliff first
        }
        for (int i = 0; i < n; ++i)
        {
            if (w.Cells()[i].waterSource == WaterSource::Ocean || comp[i] >= 0) { continue; }
            int size = 0;
            std::vector<int> stack{ i };
            comp[i] = comps;
            while (!stack.empty())
            {
                const int c = stack.back(); stack.pop_back(); ++size;
                for (uint32_t d = 0; d < 6; ++d)
                {
                    const int nb = grid.Neighbour(static_cast<uint32_t>(c), d);
                    if (nb < 0 || comp[nb] >= 0 || w.Cells()[nb].waterSource == WaterSource::Ocean) { continue; }
                    comp[nb] = comps;
                    stack.push_back(nb);
                }
            }
            ++comps;
            largest = std::max(largest, size);
        }
        Stats s;
        s.landFraction = double(land) / n;
        s.landmasses = comps;
        s.largestShare = land ? double(largest) / land : 0.0;
        s.riverCells = rivers;
        s.lakeCells = lakes;
        s.mountainFraction = double(mountain) / n;
        return s;
    }
}

TEST(WorldGenPresetStats, PresetIntentOrderings)
{
    const char* const presets[] = { "default", "continental", "archipelago", "highlands" };
    std::map<std::string, Stats> mean;
    for (const char* preset : presets)
    {
        Stats sum;
        const int seeds = 8;
        for (uint64_t seed = 1; seed <= seeds; ++seed)
        {
            WorldGenSession s;
            Dia::Maths::RandomStream rng{ 1 };
            s.SetPresetDirectories(FindShippedPresetDir(), "");
            Json::Value sz(Json::objectValue); sz["size"] = "Small";
            ASSERT_TRUE(s.SetSize(sz)["success"].asBool());
            if (std::string(preset) != "default")
            {
                Json::Value p(Json::objectValue); p["id"] = preset;
                ASSERT_TRUE(s.PresetLoad(p)["success"].asBool()) << preset;
            }
            Json::Value a(Json::objectValue); a["seed"] = static_cast<Json::UInt64>(seed);
            ASSERT_TRUE(s.Regenerate(a)["success"].asBool());
            TickUntilIdle(s, rng);
            ASSERT_EQ(s.State(), WorldGenState::Complete) << preset << " seed " << seed;
            const Stats m = Measure(s.World());
            sum.landFraction += m.landFraction; sum.landmasses += m.landmasses; sum.largestShare += m.largestShare;
            sum.riverCells += m.riverCells; sum.lakeCells += m.lakeCells; sum.mountainFraction += m.mountainFraction;
        }
        Stats& r = mean[preset];
        r.landFraction = sum.landFraction / seeds; r.landmasses = sum.landmasses / seeds; r.largestShare = sum.largestShare / seeds;
        r.riverCells = sum.riverCells / seeds; r.lakeCells = sum.lakeCells / seeds; r.mountainFraction = sum.mountainFraction / seeds;
    }

    std::printf("[ PresetStats ] Small, mean over seeds 1..8\n");
    std::printf("[ PresetStats ] %-12s %8s %10s %12s %8s %8s %10s\n", "preset", "landFrac", "landmasses", "largestShare", "riverCl", "lakeCl", "mountFrac");
    for (const char* preset : presets)
    {
        const Stats& r = mean[preset];
        std::printf("[ PresetStats ] %-12s %8.3f %10.2f %12.3f %8.1f %8.1f %10.3f\n", preset, r.landFraction, r.landmasses, r.largestShare, r.riverCells, r.lakeCells, r.mountainFraction);
    }

    const Stats& con = mean["continental"];
    const Stats& arc = mean["archipelago"];
    const Stats& hig = mean["highlands"];
    EXPECT_GT(arc.landmasses, con.landmasses) << "archipelago should have more landmasses than continental";
    EXPECT_GT(con.largestShare, arc.largestShare) << "continental largest-landmass share should exceed archipelago";
    EXPECT_GT(hig.mountainFraction, con.mountainFraction) << "highlands should be more mountainous than continental";
    EXPECT_GT(arc.riverCells, 0.0) << "archipelago should have rivers";
    EXPECT_GT(con.landFraction, arc.landFraction) << "continental should have more land than archipelago";
}
