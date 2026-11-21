////////////////////////////////////////////////////////////////////////////////
// Perf Inspector A4b (AA9): PerfSummaryAccumulator, serialiser, writer.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/PerfSummary.h>
#include <DiaObservation/Profile/UnitTimingTable.h>
#include <DiaCore/Json/external/json/json.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Dia::Observation::Profile;

namespace {

    PerfModuleRow MakeRow(const char* name, uint32_t crc, float p50 = 1.0f)
    {
        PerfModuleRow r = {};
        strncpy_s(r.name, sizeof(r.name), name, _TRUNCATE);
        r.crc = crc;
        r.entryCount = 10;
        r.p50Ms = p50;
        r.p95Ms = p50 * 2.0f;
        r.maxMs = p50 * 3.0f;
        r.ewmaMs = p50 * 1.1f;
        return r;
    }

    PerfSummary MakeSummary()
    {
        PerfSummary s;
        strncpy_s(s.buildConfig, sizeof(s.buildConfig), "Debug", _TRUNCATE);
        s.units.resize(2);
        for (int u = 0; u < 2; ++u)
        {
            PerfUnitSummary& unit = s.units[u];
            strncpy_s(unit.unit, sizeof(unit.unit), u == 0 ? "SimPU" : "RenderPU", _TRUNCATE);
            unit.budgetMs = 16.5f;
            unit.entryCount = 100;
            unit.unitP50Ms = 2.0f; unit.unitP95Ms = 4.0f; unit.unitMaxMs = 9.0f; unit.unitEwmaMs = 2.5f;
            unit.tripCount = 3; unit.suppressedCount = 4; unit.overBudgetFrames = 5;
            unit.modules.Upsert(MakeRow(u == 0 ? "simA" : "renA", 100 + u, 1.5f));
            unit.modules.Upsert(MakeRow(u == 0 ? "simB" : "renB", 200 + u, 0.5f));
        }
        return s;
    }

    std::filesystem::path TempDir(const char* leaf)
    {
        auto p = std::filesystem::temp_directory_path() / (std::string("perfsummary_") + leaf);
        std::filesystem::remove_all(p);
        std::filesystem::create_directories(p);
        return p;
    }
}

TEST(PerfSummaryAccumulator, UpsertReplacesSameCrc)
{
    PerfSummaryAccumulator acc;
    EXPECT_TRUE(acc.Upsert(MakeRow("a", 1, 1.0f)));
    EXPECT_TRUE(acc.Upsert(MakeRow("a", 1, 7.0f)));
    EXPECT_EQ(acc.GetCount(), 1u);
    EXPECT_FLOAT_EQ(acc.GetRow(0).p50Ms, 7.0f);
    EXPECT_EQ(acc.GetDropped(), 0u);
}

TEST(PerfSummaryAccumulator, OverflowDropsAndCounts)
{
    PerfSummaryAccumulator acc;
    for (uint32_t i = 0; i < PerfSummaryAccumulator::kCapacity; ++i)
        EXPECT_TRUE(acc.Upsert(MakeRow("m", i + 1)));
    EXPECT_EQ(acc.GetCount(), PerfSummaryAccumulator::kCapacity);

    EXPECT_FALSE(acc.Upsert(MakeRow("x", 9999)));
    EXPECT_FALSE(acc.Upsert(MakeRow("y", 10000)));
    EXPECT_EQ(acc.GetCount(), PerfSummaryAccumulator::kCapacity);
    EXPECT_EQ(acc.GetDropped(), 2u);

    // Updating an existing row at capacity still works and is not a drop.
    EXPECT_TRUE(acc.Upsert(MakeRow("m", 1, 9.0f)));
    EXPECT_EQ(acc.GetDropped(), 2u);
    EXPECT_FLOAT_EQ(acc.GetRow(0).p50Ms, 9.0f);
}

TEST(PerfSummaryAccumulator, ResetClears)
{
    PerfSummaryAccumulator acc;
    acc.Upsert(MakeRow("a", 1));
    acc.Reset();
    EXPECT_EQ(acc.GetCount(), 0u);
    EXPECT_EQ(acc.GetDropped(), 0u);
}

TEST(PerfSummaryRow, BuildFromTable)
{
    UnitTimingTable t;
    for (int i = 1; i <= 10; ++i)
    {
        t.RecordModule(2, 0xABCD, static_cast<float>(i));
        t.CommitEntry();
    }
    PerfModuleRow r;
    BuildPerfModuleRow(r, t, 2, "mod", 0xABCD);
    EXPECT_STREQ(r.name, "mod");
    EXPECT_EQ(r.crc, 0xABCDu);
    EXPECT_EQ(r.entryCount, 10u);
    EXPECT_FLOAT_EQ(r.maxMs, 10.0f);
    EXPECT_GT(r.p95Ms, r.p50Ms);

    BuildPerfModuleRow(r, t, 5, "never", 77);   // no samples: row still produced, zeroed
    EXPECT_STREQ(r.name, "never");
    EXPECT_EQ(r.crc, 77u);
    EXPECT_EQ(r.entryCount, 0u);
    EXPECT_FLOAT_EQ(r.maxMs, 0.0f);
}

TEST(PerfSummary, SerialiserRoundTripsAllFields)
{
    const PerfSummary s = MakeSummary();
    const std::string text = SerializePerfSummary(s);

    Json::Value root;
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream in(text);
    ASSERT_TRUE(Json::parseFromStream(b, in, &root, &errs)) << errs;

    EXPECT_EQ(root["build_config"].asString(), "Debug");
    ASSERT_EQ(root["units"].size(), 2u);
    for (unsigned u = 0; u < 2; ++u)
    {
        const Json::Value& unit = root["units"][u];
        EXPECT_EQ(unit["unit"].asString(), u == 0 ? "SimPU" : "RenderPU");
        EXPECT_DOUBLE_EQ(unit["budget_ms"].asDouble(), 16.5);
        EXPECT_EQ(unit["entry_count"].asUInt(), 100u);
        EXPECT_DOUBLE_EQ(unit["unit_p50_ms"].asDouble(), 2.0);
        EXPECT_DOUBLE_EQ(unit["unit_p95_ms"].asDouble(), 4.0);
        EXPECT_DOUBLE_EQ(unit["unit_max_ms"].asDouble(), 9.0);
        EXPECT_DOUBLE_EQ(unit["unit_ewma_ms"].asDouble(), 2.5);
        EXPECT_EQ(unit["trip_count"].asUInt(), 3u);
        EXPECT_EQ(unit["suppressed_count"].asUInt(), 4u);
        EXPECT_EQ(unit["over_budget_frames"].asUInt(), 5u);
        EXPECT_EQ(unit["modules_dropped"].asUInt(), 0u);
        ASSERT_EQ(unit["modules"].size(), 2u);
        const Json::Value& m = unit["modules"][0];
        EXPECT_EQ(m["module"].asString(), u == 0 ? "simA" : "renA");
        EXPECT_EQ(m["entry_count"].asUInt(), 10u);
        EXPECT_DOUBLE_EQ(m["p50_ms"].asDouble(), 1.5);
        EXPECT_DOUBLE_EQ(m["p95_ms"].asDouble(), 3.0);
        EXPECT_DOUBLE_EQ(m["max_ms"].asDouble(), 4.5);
        EXPECT_TRUE(m.isMember("ewma_ms"));
    }
}

TEST(PerfSummary, NoFileWhenPerfOff)
{
    const auto dir = TempDir("off");
    EXPECT_FALSE(WritePerfSummaryIfEnabled(false, MakeSummary(), dir.string().c_str()));
    EXPECT_FALSE(std::filesystem::exists(dir / "perf-summary.json"));
    std::filesystem::remove_all(dir);
}

TEST(PerfSummary, FileWrittenWhenPerfOn)
{
    const auto dir = TempDir("on");
    EXPECT_TRUE(WritePerfSummaryIfEnabled(true, MakeSummary(), dir.string().c_str()));
    ASSERT_TRUE(std::filesystem::exists(dir / "perf-summary.json"));

    std::stringstream ss;
    {
        std::ifstream f(dir / "perf-summary.json");
        ss << f.rdbuf();
    }
    Json::Value root;
    Json::CharReaderBuilder b; std::string errs;
    std::istringstream in(ss.str());
    ASSERT_TRUE(Json::parseFromStream(b, in, &root, &errs)) << errs;
    EXPECT_EQ(root["units"].size(), 2u);
    std::filesystem::remove_all(dir);
}

TEST(PerfSummary, WriteFailsOnEmptyOrMissingDir)
{
    EXPECT_FALSE(WritePerfSummary(MakeSummary(), ""));
    EXPECT_FALSE(WritePerfSummary(MakeSummary(), nullptr));
    EXPECT_FALSE(WritePerfSummary(MakeSummary(), "Z:/definitely/not/a/dir"));
}
