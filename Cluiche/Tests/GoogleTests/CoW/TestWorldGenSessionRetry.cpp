// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/retry-mechanism.md (AC7, Telemetry, SD-S1-008)
// WorldGenSession and the retry loop: Regenerate retries, live preview never does, the report and its queries.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/SessionDrive.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{
    Json::Value Obj() { return Json::Value(Json::objectValue); }

    size_t ReplySize(const Json::Value& v) { return Json::FastWriter().write(v).size(); }

    // Session wired to a registry, driven with one Regenerate.
    struct Gen
    {
        WorldGenSession s;
        Dia::Maths::RandomStream rng{ 1 };
        explicit Gen(const SiteRegistry* reg = nullptr) { if (reg) { s.SetSiteRegistry(reg); } }
        bool Regenerate(const Json::Value& args = Json::Value(Json::objectValue))
        {
            const Json::Value r = s.Regenerate(args);
            EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
            return TickUntilIdle(s, rng);
        }
    };
}

TEST(WorldGenSessionRetry, NoReportBeforeTheFirstRegenerate)
{
    Gen g;
    EXPECT_EQ(g.s.LastReport(), nullptr);
    EXPECT_EQ(g.s.ReportRevision(), 0u);
    const Json::Value r = g.s.Report(Obj());
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_FALSE(r["hasReport"].asBool());
    EXPECT_TRUE(g.s.Status()["generation"].isNull());
}

TEST(WorldGenSessionRetry, FirstGenerationAndLivePreviewNeverRetryOrReport)
{
    Gen g;
    ASSERT_TRUE(g.s.Tick(g.rng));   // first generation: terrain only
    EXPECT_EQ(g.s.LastReport(), nullptr);
    Json::Value a = Obj(); a["rivers"] = 0.9;
    g.s.Set(a);
    ASSERT_TRUE(g.s.Tick(g.rng));   // live preview
    EXPECT_EQ(g.s.LastReport(), nullptr);
    EXPECT_EQ(g.s.ReportRevision(), 0u);
}

TEST(WorldGenSessionRetry, RegenerateSuccessLeavesAReportMatchingTheWorld)
{
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    const GenerationReport* rep = g.s.LastReport();
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->outcome, GenerationOutcome::Success);
    EXPECT_EQ(rep->attempts.Size(), 1u);
    EXPECT_EQ(rep->worldRevision, g.s.Revision());
    EXPECT_EQ(g.s.ReportRevision(), 1u);
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_TRUE(g.s.RegionsFresh());
    EXPECT_GT(g.s.World().regions.regions.Size(), 0u);
    EXPECT_EQ(rep->outerSeed, g.s.Seed());
}

TEST(WorldGenSessionRetry, RetryableFailuresRetryAndTheAcceptedAttemptIsTheOneKept)
{
    AttemptGatedPlacer placer("Test.Gated", 2);
    SiteRegistry reg;
    ASSERT_TRUE(reg.Register(&placer));
    Gen g(&reg);
    ASSERT_TRUE(g.Regenerate());
    const GenerationReport* rep = g.s.LastReport();
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->attempts.Size(), 3u);
    EXPECT_EQ(rep->outcome, GenerationOutcome::Success);
    EXPECT_EQ(rep->worldRevision, g.s.Revision());
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_EQ(g.s.World().sites.Of(StringCRC("Test.Gated").Value()).size(), 1u);   // the accepted attempt's site
}

TEST(WorldGenSessionRetry, UnsatisfiableGenerationClearsRegionsAndFailsTheState)
{
    FixedGuarantee never("Test.Never", false);
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&never));
    Gen g(&reg);
    Json::Value a = Obj(); a["maxRetryAttempts"] = 3;
    ASSERT_TRUE(g.Regenerate(a));
    const GenerationReport* rep = g.s.LastReport();
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(rep->attempts.Size(), 3u);
    EXPECT_EQ(rep->worldRevision, 0u);
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    EXPECT_FALSE(g.s.RegionsFresh());
    EXPECT_EQ(g.s.World().regions.regions.Size(), 0u);
    EXPECT_EQ(g.s.World().sites.Count(), 0);
    EXPECT_EQ(g.s.ReportRevision(), 1u);
    EXPECT_TRUE(g.s.Status()["layout"].isNull());
}

TEST(WorldGenSessionRetry, ReportRevisionIncrementsPerRegenerateOnly)
{
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    EXPECT_EQ(g.s.ReportRevision(), 1u);
    ASSERT_TRUE(g.Regenerate());
    EXPECT_EQ(g.s.ReportRevision(), 2u);
    Json::Value a = Obj(); a["rivers"] = 0.9;
    g.s.Set(a);
    ASSERT_TRUE(g.s.Tick(g.rng));
    EXPECT_EQ(g.s.ReportRevision(), 2u);   // live preview does not touch the report
}

TEST(WorldGenSessionRetry, ReportBecomesStaleAfterLivePreview)
{
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    EXPECT_FALSE(g.s.Report(Obj())["stale"].asBool());
    Json::Value a = Obj(); a["rivers"] = 0.9;
    g.s.Set(a);
    ASSERT_TRUE(g.s.Tick(g.rng));
    const Json::Value r = g.s.Report(Obj());
    EXPECT_TRUE(r["hasReport"].asBool());
    EXPECT_TRUE(r["stale"].asBool());   // its world revision no longer matches the current world
}

TEST(WorldGenSessionRetry, RegenerateAcceptsAndClampsMaxRetryAttempts)
{
    Gen g;
    Json::Value a = Obj(); a["maxRetryAttempts"] = 0;
    Json::Value r = g.s.Regenerate(a);
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["maxRetryAttempts"].asUInt(), 1u);
    a["maxRetryAttempts"] = 100000;
    r = g.s.Regenerate(a);
    EXPECT_EQ(r["maxRetryAttempts"].asUInt(), kMaxRetryAttemptsLimit);
    EXPECT_EQ(g.s.MaxRetryAttempts(), kMaxRetryAttemptsLimit);
}

TEST(WorldGenSessionRetry, RegenerateRejectsBadMaxRetryAttempts)
{
    Gen g;
    for (const Json::Value& bad : { Json::Value("ten"), Json::Value(-1), Json::Value(2.5), Json::Value(true) })
    {
        Json::Value a = Obj(); a["maxRetryAttempts"] = bad;
        const Json::Value r = g.s.Regenerate(a);
        EXPECT_FALSE(r["success"].asBool()) << bad.toStyledString();
        EXPECT_FALSE(r["error"].asString().empty());
    }
    EXPECT_EQ(g.s.MaxRetryAttempts(), kDefaultMaxRetryAttempts);   // rejected values change nothing
}

TEST(WorldGenSessionRetry, MaxRetryAttemptsIsStickyAcrossRegenerates)
{
    Gen g;
    Json::Value a = Obj(); a["maxRetryAttempts"] = 4;
    g.s.Regenerate(a);
    g.s.Tick(g.rng);
    g.s.Regenerate(Obj());
    EXPECT_EQ(g.s.MaxRetryAttempts(), 4u);
    EXPECT_EQ(g.s.Params()["maxRetryAttempts"].asUInt(), 4u);
}

TEST(WorldGenSessionRetry, SameSeedGivesIdenticalWorldAndReportAcrossSessions)
{
    AttemptGatedPlacer p1("Test.Gated", 2), p2("Test.Gated", 2);
    SiteRegistry r1, r2;
    ASSERT_TRUE(r1.Register(&p1));
    ASSERT_TRUE(r2.Register(&p2));
    Gen a(&r1), b(&r2);
    Json::Value s = Obj(); s["seed"] = 9;
    ASSERT_TRUE(a.Regenerate(s));
    ASSERT_TRUE(b.Regenerate(s));
    EXPECT_TRUE(SameApartFromTiming(*a.s.LastReport(), *b.s.LastReport()));
    EXPECT_TRUE(a.s.World().regions == b.s.World().regions);
    EXPECT_EQ(a.s.World().sites.Count(), b.s.World().sites.Count());
}

TEST(WorldGenSessionRetry, StatusCarriesAOneLineGenerationSummary)
{
    AttemptGatedPlacer placer("Test.Gated", 1);
    SiteRegistry reg;
    ASSERT_TRUE(reg.Register(&placer));
    Gen g(&reg);
    ASSERT_TRUE(g.Regenerate());
    const Json::Value gen = g.s.Status()["generation"];
    ASSERT_TRUE(gen.isObject());
    EXPECT_EQ(gen["outcome"].asString(), "Success");
    EXPECT_EQ(gen["attempts"].asUInt(), 2u);
    EXPECT_EQ(gen["maxAttempts"].asUInt(), kDefaultMaxRetryAttempts);
    EXPECT_EQ(gen["worldRevision"].asUInt(), g.s.Revision());
    EXPECT_EQ(gen["reportRevision"].asUInt(), 1u);
}

TEST(WorldGenSessionRetry, ReportQueryListsEveryAttempt)
{
    FailFirstNGuarantee flaky("Test.Flaky", 2);
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&flaky));
    Gen g(&reg);
    ASSERT_TRUE(g.Regenerate());
    const Json::Value r = g.s.Report(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["hasReport"].asBool());
    EXPECT_EQ(r["outcome"].asString(), "Success");
    EXPECT_EQ(r["attemptCount"].asUInt(), 3u);
    EXPECT_EQ(r["count"].asUInt(), 3u);
    EXPECT_EQ(r["worldRevision"].asUInt(), g.s.Revision());
    EXPECT_EQ(r["reportRevision"].asUInt(), 1u);
    EXPECT_FALSE(r["stale"].asBool());
    const Json::Value& attempts = r["attempts"];
    ASSERT_EQ(attempts.size(), 3u);
    EXPECT_EQ(attempts[0]["attempt"].asUInt(), 0u);
    EXPECT_FALSE(attempts[0]["passed"].asBool());
    EXPECT_TRUE(attempts[2]["passed"].asBool());
    ASSERT_EQ(attempts[0]["failedGuarantees"].size(), 1u);
    EXPECT_EQ(attempts[0]["failedGuarantees"][0]["id"].asUInt(), StringCRC("Test.Flaky").Value());
    ASSERT_GE(attempts[0]["diagnostics"].size(), 1u);
    EXPECT_NE(attempts[0]["diagnostics"][0].asString().find("Test.Flaky fails"), std::string::npos);
    EXPECT_EQ(attempts[0]["subSeed"].asUInt64(), g.s.LastReport()->attempts[0].subSeed);
    const Json::Value& fin = r["finalValidation"];
    EXPECT_TRUE(fin["passed"].asBool());
    EXPECT_GE(fin["guarantees"].size(), 2u);
}

TEST(WorldGenSessionRetry, ReportQueryPagesAndStaysUnderTheReplyCap)
{
    FixedGuarantee never("Test.Never", false);
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&never));
    Gen g(&reg);
    Json::Value a = Obj(); a["maxRetryAttempts"] = static_cast<Json::UInt>(kMaxRetryAttemptsLimit);
    ASSERT_TRUE(g.Regenerate(a));
    ASSERT_EQ(g.s.LastReport()->attempts.Size(), kMaxRetryAttemptsLimit);

    unsigned seen = 0;
    Json::Value offset(0);
    while (!offset.isNull())
    {
        Json::Value q = Obj(); q["offset"] = offset;
        const Json::Value r = g.s.Report(q);
        ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
        EXPECT_LT(ReplySize(r), 16u * 1024u);
        EXPECT_EQ(r["attemptCount"].asUInt(), kMaxRetryAttemptsLimit);
        for (const Json::Value& at : r["attempts"]) { EXPECT_EQ(at["attempt"].asUInt(), seen); ++seen; }
        offset = r["nextOffset"];
    }
    EXPECT_EQ(seen, kMaxRetryAttemptsLimit);
}

TEST(WorldGenSessionRetry, ReportQueryRejectsBadArguments)
{
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    Json::Value q = Obj(); q["offset"] = -1;
    EXPECT_FALSE(g.s.Report(q)["success"].asBool());
    q = Obj(); q["limit"] = "x";
    EXPECT_FALSE(g.s.Report(q)["success"].asBool());
    EXPECT_FALSE(g.s.Report(Json::Value(5))["success"].asBool());
    EXPECT_TRUE(g.s.Report(Json::Value())["success"].asBool());   // null args = defaults
}

TEST(WorldGenSessionRetry, ReportQueryPastTheEndIsEmptyNotAnError)
{
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    Json::Value q = Obj(); q["offset"] = 50;
    const Json::Value r = g.s.Report(q);
    ASSERT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["count"].asUInt(), 0u);
    EXPECT_TRUE(r["nextOffset"].isNull());
}

TEST(WorldGenSessionRetry, AcceptedAttemptKeepsThePreviousBehaviourWhenAttemptZeroPasses)
{
    // Attempt 0 uses DeriveSubSeed(seed, 0): the world is the same as the pre-retry session produced.
    Gen g;
    ASSERT_TRUE(g.Regenerate());
    ASSERT_EQ(g.s.LastReport()->attempts.Size(), 1u);
    EXPECT_TRUE(g.s.Status()["layout"].isObject());
    EXPECT_TRUE(g.s.Status()["layout"]["ok"].asBool());
}

TEST(WorldGenSessionRetry, UnsatisfiableReportIsNotStaleAndALaterRegenerateRecovers)
{
    FailFirstNGuarantee flaky("Test.Flaky", 3);   // fails the first three validations, passes afterwards
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&flaky));
    Gen g(&reg);
    Json::Value a = Obj(); a["maxRetryAttempts"] = 3;
    ASSERT_TRUE(g.Regenerate(a));
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    const Json::Value failed = g.s.Report(Obj());
    EXPECT_EQ(failed["outcome"].asString(), "Unsatisfiable");
    EXPECT_FALSE(failed["stale"].asBool());          // worldRevision 0 means "no world", not "an old world"
    EXPECT_EQ(failed["worldRevision"].asUInt(), 0u);

    ASSERT_TRUE(g.Regenerate());                      // the guarantee passes now
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_TRUE(g.s.RegionsFresh());
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Success);
    EXPECT_EQ(g.s.ReportRevision(), 2u);
    EXPECT_GT(g.s.World().regions.regions.Size(), 0u);
}
