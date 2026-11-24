// S-1 per-layer cell-data goldens (decision 2026-11-13): every WorldLayerSource layer of each shipped preset at Small,
// seeds 1..3, hashed against Tests/GoogleTests/CoW/Golden/worldgen_layers.golden.json.
// A failure prints which layers moved, how many cells and the first cell (cell-level when the local baseline dump in
// out/CoW/golden-dumps/ matches the golden). Re-baseline after an intended change:
//   $env:DIA_UPDATE_GOLDEN = "1"; dia run googletest --filter="WorldGenLayerGoldens*"; Remove-Item Env:DIA_UPDATE_GOLDEN
#include <gtest/gtest.h>
#include <DiaMaths/Core/RandomStream.h>   // before module headers: windows.h defines min/max macros
#include <DiaCellLayers/Testing/CellLayerGolden.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>
#include <DraoiHexWorldTerrain/WorldLayerSource.h>
#include <DraoiHexWorldTerrain/Testing/SessionDrive.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaCore/Json/external/json/json.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
namespace Golden = Dia::CellLayers::Testing;

namespace
{
    namespace fs = std::filesystem;

    // Walks up from this file to the repo root (the directory that contains Cluiche/Assets).
    fs::path FindRepoRoot()
    {
        std::error_code ec;
        fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            if (fs::exists(dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets", ec)) { return dir; }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    bool GenerateAndCollect(const fs::path& root, const char* preset, uint64_t seed, std::vector<std::vector<float>>& storage,
                            std::vector<Golden::GoldenLayerInput>& inputs, std::string& error)
    {
        TestSession s;
        Dia::Maths::RandomStream rng{ 1 };
        s.SetPresetDirectories((root / "Cluiche/Assets/CoW/Global/WorldGen/Presets").string(), "");
        Json::Value sz(Json::objectValue); sz["size"] = "Small";
        if (!s.SetSize(sz)["success"].asBool()) { error = "SetSize failed"; return false; }
        if (std::string(preset) != "default")
        {
            Json::Value p(Json::objectValue); p["id"] = preset;
            if (!s.PresetLoad(p)["success"].asBool()) { error = std::string("PresetLoad failed: ") + preset; return false; }
        }
        Json::Value a(Json::objectValue); a["seed"] = static_cast<Json::UInt64>(seed);
        if (!s.Regenerate(a)["success"].asBool()) { error = "Regenerate refused"; return false; }
        TickUntilIdle(s, rng);
        if (s.State() != WorldGenState::Complete) { error = "generation did not complete"; return false; }

        const GridDimensions d = s.Dimensions();
        const uint32_t count = static_cast<uint32_t>(d.width * d.height);
        storage.assign(WorldLayerSource::Count(), std::vector<float>(count, 0.0f));
        inputs.clear();
        for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
        {
            const WorldLayerDesc& desc = WorldLayerSource::At(i);
            desc.read(s, storage[i].data(), count);
            inputs.push_back({ desc.id, storage[i].data(), count });
        }
        return true;
    }
}

TEST(WorldGenLayerGoldens, EveryLayerOfEveryPresetMatchesTheGolden)
{
    const fs::path root = FindRepoRoot();
    ASSERT_FALSE(root.empty()) << "repo root not found from " << __FILE__;
    Golden::GoldenStore store(root / "Cluiche/Tests/GoogleTests/CoW/Golden/worldgen_layers.golden.json",
                              root / "Cluiche/out/CoW/golden-dumps");

    const char* const presets[] = { "default", "continental", "archipelago", "highlands" };
    for (const char* preset : presets)
    {
        for (uint64_t seed = 1; seed <= 3; ++seed)
        {
            const std::string caseId = std::string(preset) + "/small/seed" + std::to_string(seed);
            SCOPED_TRACE(caseId);
            std::vector<std::vector<float>>        storage;
            std::vector<Golden::GoldenLayerInput>  inputs;
            std::string                            error;
            ASSERT_TRUE(GenerateAndCollect(root, preset, seed, storage, inputs, error)) << error;
            ASSERT_FALSE(inputs.empty());

            const Golden::GoldenResult r = store.Check(caseId, inputs);
            std::string text;
            for (const std::string& line : r.report) { text += line + "\n"; }
            EXPECT_TRUE(r.outcome == Golden::GoldenOutcome::Match || r.outcome == Golden::GoldenOutcome::Updated) << text;
        }
    }
}

TEST(WorldGenLayerGoldens, SameSeedTwiceGivesIdenticalLayers)   // guards the goldens themselves: a flaky layer can never be baselined
{
    const fs::path root = FindRepoRoot();
    ASSERT_FALSE(root.empty());
    std::vector<std::vector<float>> sa, sb;
    std::vector<Golden::GoldenLayerInput> ia, ib;
    std::string error;
    ASSERT_TRUE(GenerateAndCollect(root, "default", 2, sa, ia, error)) << error;
    ASSERT_TRUE(GenerateAndCollect(root, "default", 2, sb, ib, error)) << error;
    ASSERT_EQ(ia.size(), ib.size());
    for (size_t i = 0; i < ia.size(); ++i)
    {
        EXPECT_EQ(Golden::HashLayer(ia[i].values, ia[i].count), Golden::HashLayer(ib[i].values, ib[i].count)) << "layer " << ia[i].name;
    }
}
