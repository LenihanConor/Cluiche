// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/timeout-failure-handling.md (Goal 5a, per-frame state machine),
//       retry-mechanism.md (AC7)
// WorldGenSession: a Regenerate steps one attempt per Tick, Cancel, timeout, error, and the previous world surviving
// every failed generation.
#include <gtest/gtest.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/Testing/SessionDrive.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace
{
    Json::Value Obj() { return Json::Value(Json::objectValue); }

    bool StartsWith(const char* text, const char* prefix) { return std::strncmp(text, prefix, std::strlen(prefix)) == 0; }

    Json::Value ElevationPage(const WorldGenSession& s)
    {
        Json::Value a(Json::objectValue);
        a["layer"] = "elevation";
        a["count"] = 400;
        return s.Layers(a);
    }

    struct FakeClock : IGenerationClock
    {
        double now = 50.0;
        double NowSeconds() const override { return now; }
    };

    // Passes or fails on demand; optionally advances the fake clock and/or throws.
    class SwitchGuarantee : public IWorldGuarantee
    {
    public:
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.Switch"); }
        GuaranteeResult Check(const ValidationContext&) const override
        {
            ++calls;
            if (clock != nullptr) { clock->now += step; }
            if (throws) { throw std::runtime_error("Test error"); }
            GuaranteeResult r;
            r.idCrc = GetId().Value();
            r.satisfied = pass;
            if (!pass) { r.AddDiagnostic("switched off"); }
            return r;
        }
        bool pass = true;
        bool throws = false;
        FakeClock* clock = nullptr;
        double step = 0.0;
        mutable int calls = 0;
    };

    // A copy of everything a reader sees of the accepted world.
    struct Snapshot
    {
        explicit Snapshot(const WorldGenSession& s)
            : world(s.World()), revision(s.Revision()), state(s.State()), seed(s.Seed()), regionsFresh(s.RegionsFresh())
            , elevation(ElevationPage(s).toStyledString()), status(s.Status()) {}
        WorldStructure world;
        uint32_t revision;
        WorldGenState state;
        uint64_t seed;
        bool regionsFresh;
        std::string elevation;
        Json::Value status;
    };

    bool SameWorld(const WorldStructure& a, const WorldStructure& b)
    {
        if (a.CellCount() != b.CellCount() || !(a.regions == b.regions) || a.sites.Count() != b.sites.Count()) { return false; }
        if (a.CellCount() > 0 && std::memcmp(a.Cells(), b.Cells(), sizeof(Cell) * static_cast<size_t>(a.CellCount())) != 0) { return false; }
        for (int i = 0; i < a.sites.Count(); ++i)
        {
            if (!(a.sites.All()[i] == b.sites.All()[i])) { return false; }
        }
        return true;
    }

    // Session + registry + switchable guarantee, first generated with a successful Regenerate (regions and sites present).
    struct Gen
    {
        TestSession s;
        Dia::Maths::RandomStream rng{ 1 };
        CoWSiteRegistry registry;
        SwitchGuarantee sw;
        FakeClock clock;

        Gen()
        {
            s.SetSiteRegistry(&registry);
            s.SetGenerationClock(&clock);
            registry.RegisterGuarantee(&sw);
            EXPECT_TRUE(s.Regenerate(Obj())["success"].asBool());
            EXPECT_TRUE(TickUntilIdle(s, rng));
            EXPECT_EQ(s.State(), WorldGenState::Complete);
            EXPECT_TRUE(s.RegionsFresh());
        }
        Json::Value Regen(uint64_t seed, uint32_t maxRetry = 3, const char* extra = nullptr)
        {
            Json::Value a = Obj();
            a["seed"] = static_cast<Json::UInt64>(seed);
            a["maxRetryAttempts"] = maxRetry;
            if (extra != nullptr) { a[extra] = true; }
            return s.Regenerate(a);
        }
    };

    void ExpectUntouched(const Snapshot& before, const WorldGenSession& s, const char* what, bool requestReverted = true)
    {
        EXPECT_TRUE(SameWorld(before.world, s.World())) << what;
        EXPECT_EQ(s.Revision(), before.revision) << what;
        EXPECT_EQ(s.RegionsFresh(), before.regionsFresh) << what;
        if (requestReverted) { EXPECT_EQ(s.Seed(), before.seed) << what; }   // mid-run the session shows the pending request
        EXPECT_EQ(ElevationPage(s).toStyledString(), before.elevation) << what;
        const Json::Value now = s.Status();
        for (const char* key : { "seed", "revision", "size", "locked", "source", "regionsFresh", "dimensions" })
        {
            if (!requestReverted && std::strcmp(key, "seed") == 0) { continue; }
            EXPECT_EQ(now[key].toStyledString(), before.status[key].toStyledString()) << what << " status." << key;
        }
    }
}

// ---- stepping ------------------------------------------------------------------------------------------------------

TEST(WorldGenSessionTimeout, ASuccessOnTheFirstAttemptFinishesInTheSameTick)
{
    Gen g;
    const uint32_t revision = g.s.Revision();
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    EXPECT_TRUE(g.s.Tick(g.rng));
    EXPECT_FALSE(g.s.IsGenerating());
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_EQ(g.s.Revision(), revision + 1);
    EXPECT_EQ(g.s.LastReport()->attempts.Size(), 1u);
    EXPECT_EQ(g.s.LastReport()->worldRevision, g.s.Revision());
}

TEST(WorldGenSessionTimeout, ARetryingRegenerateRunsOneAttemptPerTickAndKeepsThePreviousWorldMeanwhile)
{
    Gen g;
    const Snapshot before(g.s);
    AttemptGatedPlacer gated("Test.Gated", 2);
    ASSERT_TRUE(g.registry.Register(&gated));
    ASSERT_TRUE(g.Regen(7)["success"].asBool());

    EXPECT_FALSE(g.s.Tick(g.rng));                    // attempt 1 failed; not finished
    EXPECT_TRUE(g.s.IsGenerating());
    EXPECT_EQ(g.s.State(), WorldGenState::Generating);
    ExpectUntouched(before, g.s, "after attempt 1", false);
    EXPECT_EQ(g.s.Seed(), 7u);                        // the pending request, until the run ends (accepted seed was 1)

    EXPECT_FALSE(g.s.Tick(g.rng));                    // attempt 2 failed
    EXPECT_TRUE(g.s.IsGenerating());
    EXPECT_TRUE(SameWorld(before.world, g.s.World()));

    EXPECT_TRUE(g.s.Tick(g.rng));                     // attempt 3 passes: the candidate replaces the world
    EXPECT_FALSE(g.s.IsGenerating());
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_EQ(g.s.Revision(), before.revision + 1);
    EXPECT_EQ(g.s.LastReport()->attempts.Size(), 3u);
    EXPECT_FALSE(SameWorld(before.world, g.s.World()));
    EXPECT_EQ(g.s.Seed(), 7u);
}

TEST(WorldGenSessionTimeout, StatusReportsProgressWhileGenerating)
{
    Gen g;
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 5)["success"].asBool());
    EXPECT_FALSE(g.s.Status()["generating"].asBool());
    g.s.Tick(g.rng);
    const Json::Value st = g.s.Status();
    EXPECT_TRUE(st["generating"].asBool());
    EXPECT_EQ(st["state"].asString(), "Generating");
    EXPECT_EQ(st["progress"]["attempts"].asUInt(), 1u);
    EXPECT_EQ(st["progress"]["maxAttempts"].asUInt(), 5u);
    EXPECT_FLOAT_EQ(st["progress"]["timeoutSeconds"].asFloat(), kDefaultTimeoutSeconds);
    TickUntilIdle(g.s, g.rng);
    EXPECT_FALSE(g.s.Status()["generating"].asBool());
    EXPECT_TRUE(g.s.Status()["progress"].isNull());
}

TEST(WorldGenSessionTimeout, CoalescedRegeneratesBeforeATickMakeOneRun)
{
    Gen g;
    const uint32_t reports = g.s.ReportRevision();
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    ASSERT_TRUE(g.Regen(8)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    EXPECT_EQ(g.s.ReportRevision(), reports + 1);
    EXPECT_EQ(g.s.Seed(), 8u);
}

// ---- the previous world survives every failure ---------------------------------------------------------------------

TEST(WorldGenSessionTimeout, AC7_AnUnsatisfiableRegenerateKeepsThePreviousWorld)
{
    Gen g;
    const Snapshot before(g.s);
    const uint32_t reportsBefore = g.s.ReportRevision();
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 3)["success"].asBool());
    EXPECT_FALSE(TickUntilIdle(g.s, g.rng));          // no world change

    ExpectUntouched(before, g.s, "unsatisfiable");
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    EXPECT_EQ(g.s.ReportRevision(), reportsBefore + 1);
    const GenerationReport* rep = g.s.LastReport();
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(rep->attempts.Size(), 3u);
    EXPECT_EQ(rep->worldRevision, 0u);
    EXPECT_TRUE(StartsWith(rep->failureMessage, "FAILURE"));
    EXPECT_TRUE(g.s.Status()["layout"].isObject());   // the previous world's layout summary still answers
    EXPECT_GT(g.s.World().regions.regions.Size(), 0u);
}

TEST(WorldGenSessionTimeout, ATimedOutRegenerateKeepsThePreviousWorld)
{
    Gen g;
    const Snapshot before(g.s);
    g.sw.pass = false;
    g.sw.clock = &g.clock;
    g.sw.step = 4.0;                                  // each failing attempt "takes" 4 s against a 5 s budget
    Json::Value a = Obj(); a["seed"] = 7; a["timeoutSeconds"] = 5;
    ASSERT_TRUE(g.s.Regenerate(a)["success"].asBool());
    EXPECT_FALSE(TickUntilIdle(g.s, g.rng));

    ExpectUntouched(before, g.s, "timeout");
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Timeout);
    EXPECT_STREQ(g.s.LastReport()->failureMessage, "TIMEOUT: World generation exceeded 5.0 seconds");
    EXPECT_EQ(g.s.LastReport()->attempts.Size(), 2u);   // elapsed 0 and 4 pass the boundary; 8 > 5 stops
    EXPECT_EQ(g.s.Report(Obj())["message"].asString(), "TIMEOUT: World generation exceeded 5.0 seconds");
}

TEST(WorldGenSessionTimeout, AGenerationErrorKeepsThePreviousWorld)
{
    Gen g;
    const Snapshot before(g.s);
    g.sw.throws = true;
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    EXPECT_FALSE(TickUntilIdle(g.s, g.rng));

    ExpectUntouched(before, g.s, "error");
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Error);
    EXPECT_STREQ(g.s.LastReport()->failureMessage, "FAILURE: World generation error: Test error");
}

TEST(WorldGenSessionTimeout, AFailedRegenerateRevertsThePendingRequestToTheAcceptedOne)
{
    Gen g;
    Json::Value shape = Obj(); shape["rivers"] = 0.7;
    g.s.Set(shape);
    g.s.Tick(g.rng);                                  // live preview accepted
    const Snapshot before(g.s);
    const Json::Value paramsBefore = g.s.Params();
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(99, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);

    EXPECT_EQ(g.s.Seed(), before.seed);
    EXPECT_EQ(g.s.Params()["params"].toStyledString(), paramsBefore["params"].toStyledString());
    EXPECT_EQ(g.s.Params()["seed"].toStyledString(), paramsBefore["seed"].toStyledString());
    EXPECT_FALSE(g.s.IsDirty());
    ExpectUntouched(before, g.s, "reverted");
}

TEST(WorldGenSessionTimeout, TheSessionRecoversAfterAFailureAndLivePreviewStillWorks)
{
    Gen g;
    const uint32_t revision = g.s.Revision();
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    ASSERT_EQ(g.s.State(), WorldGenState::Failed);

    Json::Value shape = Obj(); shape["rivers"] = 0.9;
    ASSERT_TRUE(g.s.Set(shape)["success"].asBool());   // not refused
    EXPECT_TRUE(g.s.Tick(g.rng));
    EXPECT_EQ(g.s.Revision(), revision + 1);
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);   // a live preview is a successful generation

    g.sw.pass = true;
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    EXPECT_TRUE(TickUntilIdle(g.s, g.rng));
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_TRUE(g.s.RegionsFresh());
}

TEST(WorldGenSessionTimeout, WithNoPreviousWorldAFailedFirstRegenerateAdoptsTheCandidateTerrain)
{
    TestSession s;
    Dia::Maths::RandomStream rng{ 1 };
    CoWSiteRegistry registry;
    SwitchGuarantee sw;
    sw.pass = false;
    registry.RegisterGuarantee(&sw);
    s.SetSiteRegistry(&registry);
    Json::Value a = Obj(); a["maxRetryAttempts"] = 2;
    ASSERT_TRUE(s.Regenerate(a)["success"].asBool());
    EXPECT_TRUE(TickUntilIdle(s, rng));                // nothing to keep: the candidate terrain is shown
    EXPECT_EQ(s.State(), WorldGenState::Failed);
    EXPECT_EQ(s.Revision(), 1u);
    EXPECT_FALSE(s.RegionsFresh());
    EXPECT_EQ(s.World().regions.regions.Size(), 0u);
    EXPECT_EQ(s.World().sites.Count(), 0);
    EXPECT_GT(s.World().CellCount(), 0);
    EXPECT_EQ(s.LastReport()->outcome, GenerationOutcome::Unsatisfiable);
}

// ---- cancel ------------------------------------------------------------------------------------------------------------

TEST(WorldGenSessionTimeout, AC5_CancelMidRunKeepsThePreviousWorldAndReportsCancelled)
{
    Gen g;
    const Snapshot before(g.s);
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 10)["success"].asBool());
    g.s.Tick(g.rng);                                  // attempt 1 ran
    ASSERT_TRUE(g.s.IsGenerating());

    const Json::Value r = g.s.Cancel(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["cancelled"].asBool());
    EXPECT_EQ(r["outcome"].asString(), "Cancelled");
    EXPECT_EQ(r["attempts"].asUInt(), 1u);
    EXPECT_STREQ(r["message"].asCString(), "CANCEL: World generation cancelled by user");

    EXPECT_FALSE(g.s.IsGenerating());
    EXPECT_FALSE(g.s.Tick(g.rng));                    // nothing left to do
    ExpectUntouched(before, g.s, "cancelled");
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);   // a user action is not a failure: back to the previous state
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Cancelled);
    EXPECT_EQ(g.s.LastReport()->worldRevision, 0u);
}

TEST(WorldGenSessionTimeout, CancelBeforeTheRunStartedDropsThePendingRegenerate)
{
    Gen g;
    const Snapshot before(g.s);
    const uint32_t reports = g.s.ReportRevision();
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    ASSERT_TRUE(g.s.IsDirty());

    const Json::Value r = g.s.Cancel(Obj());
    EXPECT_TRUE(r["cancelled"].asBool());
    EXPECT_EQ(r["attempts"].asUInt(), 0u);
    EXPECT_FALSE(g.s.IsDirty());
    EXPECT_FALSE(g.s.Tick(g.rng));
    ExpectUntouched(before, g.s, "pending cancelled");
    EXPECT_EQ(g.s.ReportRevision(), reports + 1);
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Cancelled);
    EXPECT_EQ(g.s.LastReport()->attempts.Size(), 0u);
}

TEST(WorldGenSessionTimeout, CancelWithNothingToCancelSucceedsAndChangesNothing)
{
    Gen g;
    const Snapshot before(g.s);
    const uint32_t reports = g.s.ReportRevision();
    const Json::Value r = g.s.Cancel(Obj());
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_FALSE(r["cancelled"].asBool());
    ExpectUntouched(before, g.s, "idle cancel");
    EXPECT_EQ(g.s.ReportRevision(), reports);
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_FALSE(g.s.Cancel(Json::Value(Json::arrayValue))["success"].asBool());   // bad args rejected
}

TEST(WorldGenSessionTimeout, CancelAfterTheRunCompletedIsANoOp)
{
    Gen g;
    ASSERT_TRUE(g.Regen(7)["success"].asBool());
    ASSERT_TRUE(g.s.Tick(g.rng));
    const Snapshot done(g.s);
    EXPECT_FALSE(g.s.Cancel(Obj())["cancelled"].asBool());
    ExpectUntouched(done, g.s, "cancel after success");
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Success);
}

TEST(WorldGenSessionTimeout, CancelBeforeTheFirstGenerationStillGeneratesTerrainOnly)
{
    TestSession s;
    Dia::Maths::RandomStream rng{ 1 };
    ASSERT_TRUE(s.Regenerate(Obj())["success"].asBool());
    EXPECT_TRUE(s.Cancel(Obj())["cancelled"].asBool());
    EXPECT_TRUE(s.Tick(rng));                          // the first generation is not a Regenerate any more
    EXPECT_EQ(s.Revision(), 1u);
    EXPECT_EQ(s.State(), WorldGenState::Complete);
    EXPECT_FALSE(s.RegionsFresh());
    EXPECT_EQ(s.LastReport()->outcome, GenerationOutcome::Cancelled);
}

TEST(WorldGenSessionTimeout, CancelDuringLivePreviewIsHarmless)
{
    Gen g;
    Json::Value shape = Obj(); shape["rivers"] = 0.9;
    g.s.Set(shape);                                    // live preview pending, no Regenerate
    const Json::Value r = g.s.Cancel(Obj());
    EXPECT_FALSE(r["cancelled"].asBool());
    EXPECT_TRUE(g.s.Tick(g.rng));                      // the preview still applies
}

// ---- busy ------------------------------------------------------------------------------------------------------------------

TEST(WorldGenSessionTimeout, ActionsAreRefusedWhileARunIsActiveAndQueriesStillWork)
{
    Gen g;
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 10)["success"].asBool());
    g.s.Tick(g.rng);
    ASSERT_TRUE(g.s.IsGenerating());

    auto expectBusy = [](const Json::Value& r, const char* what)
    {
        EXPECT_FALSE(r["success"].asBool()) << what;
        EXPECT_EQ(r["error"].asString(), "busy") << what;
        EXPECT_TRUE(r["busy"].asBool()) << what;
        EXPECT_TRUE(r["generating"].asBool()) << what;
    };
    Json::Value set = Obj(); set["rivers"] = 0.5;
    expectBusy(g.s.Set(set), "set");
    Json::Value size = Obj(); size["size"] = "Large";
    expectBusy(g.s.SetSize(size), "set_size");
    Json::Value preset = Obj(); preset["id"] = "continental";
    expectBusy(g.s.PresetLoad(preset), "preset.load");
    expectBusy(g.Regen(8), "regenerate");
    Json::Value imp = Obj(); imp["name"] = "x";
    expectBusy(g.s.ImportWorld(imp), "import");
    expectBusy(g.s.ExportWorld(Obj()), "export");

    EXPECT_TRUE(g.s.Params()["seed"].isNumeric());
    EXPECT_TRUE(g.s.Status()["success"].isNull() || g.s.Status()["state"].isString());
    EXPECT_TRUE(g.s.Regions()["success"].asBool());
    Json::Value cellArgs = Obj(); cellArgs["index"] = 0;
    EXPECT_TRUE(g.s.Cell(cellArgs)["success"].asBool());
    EXPECT_TRUE(g.s.Report(Obj())["success"].asBool());

    ASSERT_TRUE(g.s.Cancel(Obj())["cancelled"].asBool());
    EXPECT_TRUE(g.s.Set(set)["success"].asBool());      // free again
}

// ---- parameters ---------------------------------------------------------------------------------------------------------------

TEST(WorldGenSessionTimeout, RegenerateAcceptsAndClampsTimeoutSecondsAndItSticks)
{
    TestSession s;
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), kDefaultTimeoutSeconds);
    Json::Value a = Obj(); a["timeoutSeconds"] = 12.5;
    Json::Value r = s.Regenerate(a);
    ASSERT_TRUE(r["success"].asBool());
    EXPECT_FLOAT_EQ(r["timeoutSeconds"].asFloat(), 12.5f);
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), 12.5f);

    ASSERT_TRUE(s.Regenerate(Obj())["success"].asBool());            // sticky
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), 12.5f);

    Json::Value huge = Obj(); huge["timeoutSeconds"] = 1.0e12;
    ASSERT_TRUE(s.Regenerate(huge)["success"].asBool());
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), kMaxTimeoutSeconds);

    Json::Value zero = Obj(); zero["timeoutSeconds"] = 0;
    ASSERT_TRUE(s.Regenerate(zero)["success"].asBool());
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), 0.0f);

    for (const Json::Value bad : { Json::Value(-1.0), Json::Value("5"), Json::Value(true), Json::Value(Json::nullValue) })
    {
        Json::Value b = Obj(); b["timeoutSeconds"] = bad;
        const Json::Value e = s.Regenerate(b);
        EXPECT_FALSE(e["success"].asBool()) << bad.toStyledString();
        EXPECT_FLOAT_EQ(s.TimeoutSeconds(), 0.0f);                    // unchanged by a rejected request
    }
    s.SetTimeoutSeconds(7.0f);
    EXPECT_FLOAT_EQ(s.TimeoutSeconds(), 7.0f);
}

TEST(WorldGenSessionTimeout, StatusAndReportCarryTheFailureMessage)
{
    Gen g;
    EXPECT_STREQ(g.s.Status()["generation"]["message"].asCString(), "");   // success: empty
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    const std::string expected = "FAILURE: World generation failed after 2 attempts (unsatisfiable seed or parameters)";
    EXPECT_EQ(g.s.Status()["generation"]["message"].asString(), expected);
    EXPECT_EQ(g.s.Report(Obj())["message"].asString(), expected);
    EXPECT_EQ(g.s.Status()["generation"]["outcome"].asString(), "Unsatisfiable");
}

// ---- locked imports -------------------------------------------------------------------------------------------------------------

namespace
{
    struct TempDir
    {
        TempDir()
        {
            static std::atomic<int> counter{ 0 };
            path = fs::temp_directory_path() / ("cow_timeout_test_" + std::to_string(counter.fetch_add(1)) + "_" +
                                                 std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFFFF));
            std::error_code ec;
            fs::remove_all(path, ec);
            fs::create_directories(path, ec);
        }
        ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
        fs::path path;
    };
}

TEST(WorldGenSessionTimeout, AFailedUnlockRegenerateKeepsTheLockedImportedWorld)
{
    TempDir dir;
    Gen g;
    g.s.SetWorldDirectories("", dir.path.string());
    Json::Value ex = Obj(); ex["name"] = "w1";
    ASSERT_TRUE(g.s.ExportWorld(ex)["success"].asBool());
    Json::Value im = Obj(); im["name"] = "w1";
    ASSERT_TRUE(g.s.ImportWorld(im)["success"].asBool());
    ASSERT_TRUE(g.s.IsLocked());
    const Snapshot before(g.s);

    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 2, "unlock")["success"].asBool());     // unlock drops the lock for the run...
    EXPECT_FALSE(g.s.IsLocked());
    EXPECT_FALSE(TickUntilIdle(g.s, g.rng));                       // ...which fails

    EXPECT_TRUE(g.s.IsLocked());                                   // the hand-edited world and its lock are back
    EXPECT_TRUE(g.s.IsImported());
    ExpectUntouched(before, g.s, "locked import kept");
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);

    // A refused action on the still-locked world asks for confirmation as before.
    Json::Value set = Obj(); set["rivers"] = 0.5;
    EXPECT_TRUE(g.s.Set(set)["needsConfirm"].asBool());

    // A successful unlock Regenerate replaces the import.
    g.sw.pass = true;
    ASSERT_TRUE(g.Regen(7, 2, "unlock")["success"].asBool());
    EXPECT_TRUE(TickUntilIdle(g.s, g.rng));
    EXPECT_FALSE(g.s.IsLocked());
    EXPECT_FALSE(g.s.IsImported());
}

TEST(WorldGenSessionTimeout, CancelDuringAnUnlockRegenerateRelocksTheImport)
{
    TempDir dir;
    Gen g;
    g.s.SetWorldDirectories("", dir.path.string());
    Json::Value ex = Obj(); ex["name"] = "w1";
    ASSERT_TRUE(g.s.ExportWorld(ex)["success"].asBool());
    Json::Value im = Obj(); im["name"] = "w1";
    ASSERT_TRUE(g.s.ImportWorld(im)["success"].asBool());
    const Snapshot before(g.s);

    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 10, "unlock")["success"].asBool());
    g.s.Tick(g.rng);
    ASSERT_TRUE(g.s.IsGenerating());
    ASSERT_TRUE(g.s.Cancel(Obj())["cancelled"].asBool());
    EXPECT_TRUE(g.s.IsLocked());
    ExpectUntouched(before, g.s, "cancelled unlock");
}

// ---- edge cases (Timeout plan task 5) -------------------------------------------------------------------------------------------

TEST(WorldGenSessionTimeoutEdge, ATimeoutExactlyAtTheBudgetDoesNotFireUntilItIsExceeded)
{
    Gen g;
    const Snapshot before(g.s);
    g.sw.pass = false;
    g.sw.clock = &g.clock;
    g.sw.step = 5.0;                                   // each failing attempt takes exactly the 5 s budget
    Json::Value a = Obj(); a["seed"] = 7; a["timeoutSeconds"] = 5;
    ASSERT_TRUE(g.s.Regenerate(a)["success"].asBool());
    EXPECT_FALSE(TickUntilIdle(g.s, g.rng));
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Timeout);
    EXPECT_EQ(g.s.LastReport()->attempts.Size(), 2u);   // elapsed 0 and exactly 5 are not > 5; 10 is
    ExpectUntouched(before, g.s, "boundary");
}

TEST(WorldGenSessionTimeoutEdge, AZeroBudgetOnTheRealClockTimesOutBeforeTheFirstAttemptAndKeepsTheWorld)
{
    TestSession s;                                  // no injected clock: the steady clock
    Dia::Maths::RandomStream rng{ 1 };
    ASSERT_TRUE(s.Regenerate(Obj())["success"].asBool());
    ASSERT_TRUE(TickUntilIdle(s, rng));
    ASSERT_EQ(s.State(), WorldGenState::Complete);
    const Snapshot before(s);

    Json::Value a = Obj(); a["seed"] = 7; a["timeoutSeconds"] = 0;
    ASSERT_TRUE(s.Regenerate(a)["success"].asBool());
    EXPECT_FALSE(TickUntilIdle(s, rng));
    EXPECT_EQ(s.LastReport()->outcome, GenerationOutcome::Timeout);
    EXPECT_EQ(s.LastReport()->attempts.Size(), 0u);     // the candidate terrain took real time, so the budget was gone
    ExpectUntouched(before, s, "zero budget");
}

TEST(WorldGenSessionTimeoutEdge, AHugeBudgetIsClampedAndNeverGetsInTheWay)
{
    Gen g;
    Json::Value a = Obj(); a["seed"] = 7; a["timeoutSeconds"] = 1.0e30;
    ASSERT_TRUE(g.s.Regenerate(a)["success"].asBool());
    EXPECT_TRUE(TickUntilIdle(g.s, g.rng));
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Success);
    EXPECT_FLOAT_EQ(g.s.TimeoutSeconds(), kMaxTimeoutSeconds);
}

TEST(WorldGenSessionTimeoutEdge, CancelThenRegenerateStraightAwayRunsCleanly)
{
    Gen g;
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 10)["success"].asBool());
    g.s.Tick(g.rng);
    ASSERT_TRUE(g.s.Cancel(Obj())["cancelled"].asBool());
    EXPECT_FALSE(g.s.Cancel(Obj())["cancelled"].asBool());   // a second cancel has nothing to cancel

    g.sw.pass = true;
    ASSERT_TRUE(g.Regen(8)["success"].asBool());
    EXPECT_TRUE(TickUntilIdle(g.s, g.rng));
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Success);
    EXPECT_EQ(g.s.Seed(), 8u);
}

TEST(WorldGenSessionTimeoutEdge, TicksAfterAFailureDoNothingAndCancelAfterAFailureIsANoOp)
{
    Gen g;
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    ASSERT_EQ(g.s.State(), WorldGenState::Failed);
    const Snapshot after(g.s);
    const uint32_t reports = g.s.ReportRevision();
    for (int i = 0; i < 5; ++i) { EXPECT_FALSE(g.s.Tick(g.rng)); }
    EXPECT_FALSE(g.s.Cancel(Obj())["cancelled"].asBool());
    ExpectUntouched(after, g.s, "idle after failure");
    EXPECT_EQ(g.s.ReportRevision(), reports);
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
}

TEST(WorldGenSessionTimeoutEdge, ExportAfterAFailedRegenerateWritesThePreviousWorldWithItsOwnSeed)
{
    TempDir dir;
    Gen g;                                              // accepted seed is the default (1)
    g.s.SetWorldDirectories("", dir.path.string());
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(99, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    ASSERT_EQ(g.s.State(), WorldGenState::Failed);

    Json::Value ex = Obj(); ex["name"] = "afterfail";
    const Json::Value r = g.s.ExportWorld(ex);
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    Json::Value im = Obj(); im["name"] = "afterfail";
    TestSession other;
    other.SetWorldDirectories("", dir.path.string());
    ASSERT_TRUE(other.ImportWorld(im)["success"].asBool());
    EXPECT_EQ(other.Seed(), 1u);                        // the provenance names the seed that produced the world on show
}

TEST(WorldGenSessionTimeoutEdge, ARunThatAlreadyFailedLeavesNoCandidateStateBehind)
{
    Gen g;
    g.sw.pass = false;
    ASSERT_TRUE(g.Regen(7, 2)["success"].asBool());
    TickUntilIdle(g.s, g.rng);
    EXPECT_FALSE(g.s.IsGenerating());
    EXPECT_FALSE(g.s.IsDirty());
    Json::Value shape = Obj(); shape["rivers"] = 0.2;
    EXPECT_TRUE(g.s.Set(shape)["success"].asBool());    // actions are accepted again straight away
}
