// Tests for CoW::BuildF2StageResult: the all-pass vs any-fail decision behind the F2 StageResult event.
#include <gtest/gtest.h>
#include <Modules/F2StageResult.h>
#include <cstring>

using namespace CoW;
using Cluiche::AppFlow::StageResultEvent;
using Cluiche::AppFlow::StageResultSeverity;

TEST(F2StageResult, AllPassIsOkPass)
{
    StageResultEvent ev;
    BuildF2StageResult(kF2CheckCount, 0, 1234, ev);
    EXPECT_EQ(ev.result.severity, StageResultSeverity::kOk);
    EXPECT_STREQ(ev.result.label.AsCStr(), "PASS");
    EXPECT_EQ(ev.stage, Dia::Core::StringCRC(kF2StageName));
    EXPECT_EQ(ev.result.utcMs, 1234u);
    EXPECT_EQ(ev.result.sessionId.Length(), 0u);
    EXPECT_EQ(ev.result.logPath.Length(), 0u);
}

TEST(F2StageResult, OneFailIsFailFail)
{
    StageResultEvent ev;
    BuildF2StageResult(kF2CheckCount - 1, 1, 99, ev);
    EXPECT_EQ(ev.result.severity, StageResultSeverity::kFail);
    EXPECT_STREQ(ev.result.label.AsCStr(), "FAIL");
    EXPECT_EQ(ev.stage, Dia::Core::StringCRC("F2CoreRuntime"));
}

TEST(F2StageResult, IncompleteChecksAreFail)
{
    StageResultEvent ev;
    BuildF2StageResult(kF2CheckCount - 1, 0, 1, ev);   // one check never resolved
    EXPECT_EQ(ev.result.severity, StageResultSeverity::kFail);
}
