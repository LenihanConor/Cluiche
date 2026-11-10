#include <gtest/gtest.h>

#include <Modules/StageStatusModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <string>

using Cluiche::AppFlow::StageStatusModule;
using Dia::Core::StringCRC;

namespace {
StageStatusModule::StageStatus Make(const char* label, StageStatusModule::Severity sev, bool visited)
{
    StageStatusModule::StageStatus s;
    s.label = label;
    s.severity = sev;
    s.visited = visited;
    return s;
}
}

TEST(StageStatusModule, SetThenGetReturnsStoredValues)
{
    StageStatusModule m(StringCRC("TestStageStatus"));
    EXPECT_TRUE(m.Set(StringCRC("StageA"), Make("Alpha", StageStatusModule::Severity::kWarn, true)));
    const auto* s = m.Get(StringCRC("StageA"));
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s->label, "Alpha");
    EXPECT_EQ(s->severity, StageStatusModule::Severity::kWarn);
    EXPECT_TRUE(s->visited);
}

TEST(StageStatusModule, UnknownStageReturnsNull)
{
    StageStatusModule m(StringCRC("TestStageStatus"));
    EXPECT_EQ(m.Get(StringCRC("Missing")), nullptr);
    m.Set(StringCRC("StageA"), Make("Alpha", StageStatusModule::Severity::kOk, false));
    EXPECT_EQ(m.Get(StringCRC("Missing")), nullptr);
}

TEST(StageStatusModule, OverwriteReplacesExisting)
{
    StageStatusModule m(StringCRC("TestStageStatus"));
    EXPECT_TRUE(m.Set(StringCRC("StageA"), Make("Old", StageStatusModule::Severity::kOk, false)));
    EXPECT_TRUE(m.Set(StringCRC("StageA"), Make("New", StageStatusModule::Severity::kFail, true)));
    const auto* s = m.Get(StringCRC("StageA"));
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s->label, "New");
    EXPECT_EQ(s->severity, StageStatusModule::Severity::kFail);
    EXPECT_TRUE(s->visited);
}

TEST(StageStatusModule, CapacityLimitRejectsNewStageButAllowsOverwrite)
{
    StageStatusModule m(StringCRC("TestStageStatus"));
    for (int i = 0; i < StageStatusModule::kMaxStages; ++i)
    {
        const std::string name = "Stage" + std::to_string(i);
        EXPECT_TRUE(m.Set(StringCRC(name.c_str()), Make("x", StageStatusModule::Severity::kOk, false))) << i;
    }
    EXPECT_FALSE(m.Set(StringCRC("OneTooMany"), Make("y", StageStatusModule::Severity::kOk, false)));
    EXPECT_EQ(m.Get(StringCRC("OneTooMany")), nullptr);

    EXPECT_TRUE(m.Set(StringCRC("Stage0"), Make("z", StageStatusModule::Severity::kFail, true)));
    const auto* s = m.Get(StringCRC("Stage0"));
    ASSERT_NE(s, nullptr);
    EXPECT_STREQ(s->label, "z");
    EXPECT_EQ(s->severity, StageStatusModule::Severity::kFail);
}

TEST(StageStatusModule, AllSeverityValuesRoundTrip)
{
    StageStatusModule m(StringCRC("TestStageStatus"));
    const StageStatusModule::Severity all[] = {
        StageStatusModule::Severity::kNone, StageStatusModule::Severity::kOk,
        StageStatusModule::Severity::kWarn, StageStatusModule::Severity::kFail };
    for (int i = 0; i < 4; ++i)
    {
        const std::string name = "Sev" + std::to_string(i);
        EXPECT_TRUE(m.Set(StringCRC(name.c_str()), Make("l", all[i], false)));
    }
    for (int i = 0; i < 4; ++i)
    {
        const std::string name = "Sev" + std::to_string(i);
        const auto* s = m.Get(StringCRC(name.c_str()));
        ASSERT_NE(s, nullptr) << i;
        EXPECT_EQ(s->severity, all[i]);
    }
}
