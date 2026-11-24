// CellLayerGolden: per-layer hash goldens with a readable diff report (Testing/ helper shipped with DiaCellLayers).
#include <gtest/gtest.h>
#include <DiaCellLayers/Testing/CellLayerGolden.h>

#include <process.h>   // _getpid: shards run in separate processes and must not share a temp dir

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace Dia::CellLayers::Testing;

namespace
{
    namespace fs = std::filesystem;

    struct TempDir
    {
        fs::path path;
        TempDir()
        {
            static int counter = 0;
            path = fs::temp_directory_path() / ("CellLayerGoldenTest_" + std::to_string(_getpid()) + "_" + std::to_string(++counter));
            fs::remove_all(path);
            fs::create_directories(path);
        }
        ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    };

    std::string Join(const std::vector<std::string>& lines)
    {
        std::string s;
        for (const std::string& l : lines) { s += l; s += '\n'; }
        return s;
    }

    bool Contains(const std::vector<std::string>& lines, const std::string& needle)
    {
        return Join(lines).find(needle) != std::string::npos;
    }

    struct Layers
    {
        std::vector<float> elevation{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        std::vector<float> rain{ 0.5f, 0.5f, 0.25f, 0.0f, 1.0f, 0.75f };
        std::vector<GoldenLayerInput> Inputs() const
        {
            return { { "elevation", elevation.data(), static_cast<uint32_t>(elevation.size()) },
                     { "rain", rain.data(), static_cast<uint32_t>(rain.size()) } };
        }
    };
}

TEST(CellLayerGoldenHash, EqualDataEqualHashDifferentDataDifferentHash)
{
    const float a[] = { 1.0f, 2.0f, 3.0f };
    const float b[] = { 1.0f, 2.0f, 3.0f };
    const float c[] = { 1.0f, 2.0f, 3.5f };
    const float d[] = { 3.0f, 2.0f, 1.0f };
    EXPECT_EQ(HashLayer(a, 3), HashLayer(b, 3));
    EXPECT_NE(HashLayer(a, 3), HashLayer(c, 3));
    EXPECT_NE(HashLayer(a, 3), HashLayer(d, 3));   // order sensitive
    EXPECT_NE(HashLayer(a, 3), HashLayer(a, 2));   // length sensitive
}

TEST(CellLayerGoldenHash, AllNaNPayloadsHashTheSame)
{
    const uint32_t bitsA = 0x7FC00000u, bitsB = 0x7FC12345u;
    float a, b;
    std::memcpy(&a, &bitsA, 4);
    std::memcpy(&b, &bitsB, 4);
    ASSERT_TRUE(std::isnan(a) && std::isnan(b));
    EXPECT_EQ(HashLayer(&a, 1), HashLayer(&b, 1));
}

TEST(CellLayerGoldenHash, EmptyLayerHashesWithoutReading)
{
    EXPECT_EQ(HashLayer(nullptr, 0), HashLayer(nullptr, 0));
}

TEST(CellLayerGoldenSummary, MinMaxMean)
{
    const float v[] = { 1.0f, 3.0f, 2.0f };
    const LayerSummary s = Summarise(v, 3);
    EXPECT_EQ(s.count, 3u);
    EXPECT_DOUBLE_EQ(s.min, 1.0);
    EXPECT_DOUBLE_EQ(s.max, 3.0);
    EXPECT_DOUBLE_EQ(s.mean, 2.0);
}

TEST(CellLayerGoldenSummary, NaNCellsAreCountedNotSummarised)
{
    const float v[] = { 1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f };
    const LayerSummary s = Summarise(v, 3);
    EXPECT_EQ(s.count, 3u);
    EXPECT_EQ(s.nanCount, 1u);
    EXPECT_DOUBLE_EQ(s.min, 1.0);
    EXPECT_DOUBLE_EQ(s.max, 3.0);
    EXPECT_DOUBLE_EQ(s.mean, 2.0);
}

TEST(CellLayerGolden, MissingGoldenReportsMissingAndNamesTheCase)
{
    TempDir dir;
    Layers l;
    GoldenStore store(dir.path / "golden.json", dir.path / "dumps", /*update*/ false);
    const GoldenResult r = store.Check("default/seed1", l.Inputs());
    EXPECT_EQ(r.outcome, GoldenOutcome::Missing);
    EXPECT_TRUE(Contains(r.report, "default/seed1")) << Join(r.report);
}

TEST(CellLayerGolden, UpdateThenCheckMatches)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore store(dir.path / "golden.json", dir.path / "dumps", /*update*/ true);
        EXPECT_EQ(store.Check("c1", l.Inputs()).outcome, GoldenOutcome::Updated);
    }
    GoldenStore store(dir.path / "golden.json", dir.path / "dumps", /*update*/ false);
    const GoldenResult r = store.Check("c1", l.Inputs());
    EXPECT_EQ(r.outcome, GoldenOutcome::Match) << Join(r.report);
    EXPECT_TRUE(r.report.empty());
}

TEST(CellLayerGolden, UpdatePreservesOtherCases)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c2", l.Inputs());
    }
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    EXPECT_EQ(s.Check("c1", l.Inputs()).outcome, GoldenOutcome::Match);
    EXPECT_EQ(s.Check("c2", l.Inputs()).outcome, GoldenOutcome::Match);
}

TEST(CellLayerGolden, ChangedCellReportsLayerCountAndFirstIndexFromBaselineDump)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    l.rain[4] = 0.125f;
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    const GoldenResult r = s.Check("c1", l.Inputs());
    ASSERT_EQ(r.outcome, GoldenOutcome::Mismatch);
    const std::string text = Join(r.report);
    EXPECT_NE(text.find("rain"), std::string::npos) << text;
    EXPECT_NE(text.find("1 of 6 cells"), std::string::npos) << text;
    EXPECT_NE(text.find("first cell 4"), std::string::npos) << text;
    EXPECT_EQ(text.find("elevation"), std::string::npos) << "unchanged layer must not be reported: " << text;
}

TEST(CellLayerGolden, ChangedCellWithoutBaselineDumpFallsBackToSummary)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    fs::remove_all(dir.path / "dumps");
    l.elevation[0] = 100.0f;
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    const GoldenResult r = s.Check("c1", l.Inputs());
    ASSERT_EQ(r.outcome, GoldenOutcome::Mismatch);
    const std::string text = Join(r.report);
    EXPECT_NE(text.find("elevation"), std::string::npos) << text;
    EXPECT_NE(text.find("max"), std::string::npos) << "summary shows the max moving: " << text;
    EXPECT_EQ(text.find("first cell"), std::string::npos) << "no cell-level data without a baseline dump: " << text;
}

TEST(CellLayerGolden, MismatchWritesActualDump)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    l.rain[0] = 9.0f;
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    ASSERT_EQ(s.Check("c1", l.Inputs()).outcome, GoldenOutcome::Mismatch);
    EXPECT_TRUE(fs::exists(dir.path / "dumps" / "c1.actual.json"));
}

TEST(CellLayerGolden, AddedAndRemovedLayersAreReported)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    std::vector<GoldenLayerInput> inputs = { { "elevation", l.elevation.data(), 6 }, { "wind", l.rain.data(), 6 } };
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    const GoldenResult r = s.Check("c1", inputs);
    ASSERT_EQ(r.outcome, GoldenOutcome::Mismatch);
    EXPECT_TRUE(Contains(r.report, "wind")) << Join(r.report);
    EXPECT_TRUE(Contains(r.report, "rain")) << Join(r.report);
}

TEST(CellLayerGolden, CellCountChangeIsReported)
{
    TempDir dir;
    Layers l;
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c1", l.Inputs());
    }
    std::vector<GoldenLayerInput> inputs = { { "elevation", l.elevation.data(), 5 }, { "rain", l.rain.data(), 6 } };
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    const GoldenResult r = s.Check("c1", inputs);
    ASSERT_EQ(r.outcome, GoldenOutcome::Mismatch);
    EXPECT_TRUE(Contains(r.report, "elevation")) << Join(r.report);
    EXPECT_TRUE(Contains(r.report, "6")) << Join(r.report);
}

TEST(CellLayerGolden, CaseIdWithSlashesIsUsableAsDumpFileName)
{
    TempDir dir;
    Layers l;
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
    EXPECT_EQ(s.Check("highlands/small/seed 3", l.Inputs()).outcome, GoldenOutcome::Updated);
    EXPECT_TRUE(fs::exists(dir.path / "dumps" / "highlands_small_seed_3.json"));
}

TEST(CellLayerGolden, NaNCellsRoundTripThroughDump)
{
    TempDir dir;
    std::vector<float> v{ 1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f };
    std::vector<GoldenLayerInput> in = { { "x", v.data(), 3 } };
    {
        GoldenStore s(dir.path / "golden.json", dir.path / "dumps", true);
        s.Check("c", in);
    }
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    EXPECT_EQ(s.Check("c", in).outcome, GoldenOutcome::Match);
    v[2] = 4.0f;
    const GoldenResult r = s.Check("c", in);
    ASSERT_EQ(r.outcome, GoldenOutcome::Mismatch);
    EXPECT_TRUE(Contains(r.report, "1 of 3 cells")) << Join(r.report);
}

TEST(CellLayerGolden, CorruptGoldenFileIsAnErrorNotAMatch)
{
    TempDir dir;
    { std::ofstream(dir.path / "golden.json") << "{ not json"; }
    Layers l;
    GoldenStore s(dir.path / "golden.json", dir.path / "dumps", false);
    const GoldenResult r = s.Check("c1", l.Inputs());
    EXPECT_EQ(r.outcome, GoldenOutcome::Error);
    EXPECT_FALSE(r.report.empty());
}

TEST(CellLayerGolden, UpdateRequestedReadsEnvironment)
{
#ifdef _WIN32
    _putenv_s("DIA_UPDATE_GOLDEN", "1");
    EXPECT_TRUE(UpdateRequested());
    _putenv_s("DIA_UPDATE_GOLDEN", "0");
    EXPECT_FALSE(UpdateRequested());
    _putenv_s("DIA_UPDATE_GOLDEN", "");
    EXPECT_FALSE(UpdateRequested());
#endif
}
