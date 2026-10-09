#include <gtest/gtest.h>

#include <Modules/BootMenuLogic.h>
#include <Modules/StageStatusModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>

using Cluiche::AppFlow::BootMenuLogic;
using Cluiche::AppFlow::StageStatusModule;
using Dia::Core::StringCRC;
using Sev = StageStatusModule::Severity;

namespace {
StageStatusModule::StageStatus Make(const char* label, Sev sev, bool visited)
{
    StageStatusModule::StageStatus s;
    s.label = label;
    s.severity = sev;
    s.visited = visited;
    return s;
}

const StringCRC kStages[] = {
    StringCRC("StageA"), StringCRC("StageB"), StringCRC("StageC"), StringCRC("StageD"), StringCRC("StageE") };
const unsigned int kStageCount = 5;
}

TEST(BootMenuLogic, CountsBySeverityWithProvider)
{
    StageStatusModule m(StringCRC("Status"));
    m.Set(StringCRC("StageA"), Make("PASSED", Sev::kOk, false));
    m.Set(StringCRC("StageB"), Make("PASSED", Sev::kOk, false));
    m.Set(StringCRC("StageC"), Make("FAILED", Sev::kFail, false));
    m.Set(StringCRC("StageD"), Make("SLOW", Sev::kWarn, false));
    // StageE has no entry -> pending

    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.SetProvider(&m);

    EXPECT_TRUE(logic.HasStatus());
    const BootMenuLogic::Counts c = logic.CountBySeverity();
    EXPECT_EQ(c.passed, 2u);
    EXPECT_EQ(c.failed, 1u);
    EXPECT_EQ(c.pending, 2u);
}

TEST(BootMenuLogic, KNoneCountsAsPending)
{
    StageStatusModule m(StringCRC("Status"));
    m.Set(StringCRC("StageA"), Make("-", Sev::kNone, false));

    BootMenuLogic logic;
    logic.SetStages(kStages, 1);
    logic.SetProvider(&m);

    const BootMenuLogic::Counts c = logic.CountBySeverity();
    EXPECT_EQ(c.passed, 0u);
    EXPECT_EQ(c.failed, 0u);
    EXPECT_EQ(c.pending, 1u);
}

TEST(BootMenuLogic, NullProviderHasNoStatusAndNoBadges)
{
    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.SetProvider(nullptr);

    EXPECT_FALSE(logic.HasStatus());
    BootMenuLogic::Badge badge;
    EXPECT_FALSE(logic.GetBadge(StringCRC("StageA"), badge));
    const BootMenuLogic::Counts c = logic.CountBySeverity();
    EXPECT_EQ(c.passed, 0u);
    EXPECT_EQ(c.failed, 0u);
    EXPECT_EQ(c.pending, 0u);
}

TEST(BootMenuLogic, BadgeLookupWithAndWithoutEntry)
{
    StageStatusModule m(StringCRC("Status"));
    m.Set(StringCRC("StageA"), Make("PASSED", Sev::kOk, false));

    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.SetProvider(&m);

    BootMenuLogic::Badge badge;
    ASSERT_TRUE(logic.GetBadge(StringCRC("StageA"), badge));
    EXPECT_STREQ(badge.label, "PASSED");
    EXPECT_EQ(badge.severity, Sev::kOk);

    BootMenuLogic::Badge other;
    EXPECT_FALSE(logic.GetBadge(StringCRC("StageB"), other));
}

TEST(BootMenuLogic, RestoreVisitedFromProviderEntries)
{
    StageStatusModule m(StringCRC("Status"));
    m.Set(StringCRC("StageA"), Make("x", Sev::kOk, true));
    m.Set(StringCRC("StageB"), Make("x", Sev::kOk, false));
    m.Set(StringCRC("NotListed"), Make("x", Sev::kOk, true));
    // StageC has no entry

    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.SetProvider(&m);
    logic.RestoreVisited();

    EXPECT_TRUE(logic.IsVisited(StringCRC("StageA")));
    EXPECT_FALSE(logic.IsVisited(StringCRC("StageB")));
    EXPECT_FALSE(logic.IsVisited(StringCRC("StageC")));
    EXPECT_FALSE(logic.IsVisited(StringCRC("NotListed")));
}

TEST(BootMenuLogic, RestoreVisitedWithNullProviderMarksNothing)
{
    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.RestoreVisited();
    for (unsigned int i = 0; i < kStageCount; ++i)
        EXPECT_FALSE(logic.IsVisited(kStages[i])) << i;
}

TEST(BootMenuLogic, MarkVisitedOnLaunch)
{
    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    EXPECT_FALSE(logic.IsVisited(StringCRC("StageC")));
    logic.MarkVisited(StringCRC("StageC"));

    EXPECT_TRUE(logic.IsVisited(StringCRC("StageC")));
    EXPECT_FALSE(logic.IsVisited(StringCRC("StageA")));
}

TEST(BootMenuLogic, MarkVisitedIgnoresUnlistedStage)
{
    BootMenuLogic logic;
    logic.SetStages(kStages, kStageCount);
    logic.MarkVisited(StringCRC("NotListed"));
    EXPECT_FALSE(logic.IsVisited(StringCRC("NotListed")));
}

TEST(BootMenuLogic, TitleDefaultsAndOverrides)
{
    EXPECT_STREQ(BootMenuLogic::ResolveTitle(nullptr), "Boot Menu");
    EXPECT_STREQ(BootMenuLogic::ResolveTitle(""), "Boot Menu");
    EXPECT_STREQ(BootMenuLogic::ResolveTitle("CoW Boot"), "CoW Boot");
}

TEST(BootMenuLogic, ProjectDefaultsAndOverrides)
{
    EXPECT_STREQ(BootMenuLogic::ResolveProject(nullptr), "");
    EXPECT_STREQ(BootMenuLogic::ResolveProject(""), "");
    EXPECT_STREQ(BootMenuLogic::ResolveProject("CoW v0.1"), "CoW v0.1");
}
