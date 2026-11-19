#include <gtest/gtest.h>

#include <Modules/StageCatalogueModule.h>
#include <Modules/StageCatalogueState.h>
#include <Testing/StageCatalogueFixture.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Testing/MockSink.h>
#include <DiaObservation/Testing/HealthFixture.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#define DIA_TEST_PID _getpid()
#else
#include <unistd.h>
#define DIA_TEST_PID getpid()
#endif

using namespace Cluiche::AppFlow;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static int counter = 0;
        std::ostringstream n;
        n << "StageCatModuleTest_" << DIA_TEST_PID << "_" << std::chrono::steady_clock::now().time_since_epoch().count() << "_" << counter++;
        path = fs::temp_directory_path() / n.str();
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    std::string File(const char* name) const { return (path / name).string(); }
};

std::string DiagamePath(const char* fixture)
{
    return (Cluiche::AppFlow::Testing::FindStageCatalogueFixtures(__FILE__) / fixture / "game.diagame").string();
}

StageResultEvent MakeEvent(const char* stage, StageResultSeverity sev, uint64_t utcMs, const char* sessionId = "", const char* logPath = "")
{
    StageResultEvent e;
    e.stage = StringCRC(stage);
    e.result.severity = sev;
    e.result.SetLabel(sev == StageResultSeverity::kOk ? "PASS" : "FAIL");
    e.result.utcMs = utcMs;
    e.result.SetSessionId(sessionId);
    e.result.SetLogPath(logPath);
    return e;
}

// Exposes the protected seams and fakes the observation session.
class TestableModule : public StageCatalogueModule {
public:
    TestableModule() : StageCatalogueModule(StringCRC("TestStageCatalogue")) {}

    void Configure(const std::string& diagame, const std::string& resultsPath)
    {
        Json::Value cfg(Json::objectValue);
        if (!diagame.empty()) { cfg["diagame_path"] = diagame; }
        if (!resultsPath.empty()) { cfg["results_path"] = resultsPath; }
        Json::FastWriter w;
        OnConfigure(w.write(cfg).c_str());
    }
    bool Start() { return DoStart() == Dia::ApplicationFlow::StartResult::kReady; }
    void Stop() { DoStop(); }
    void Frame(const StageResultEvent* ev, unsigned int n) { ProcessEvents(ev, n); EndFrame(); }

    bool hasSession = false;
    std::string sessionId;
    std::string sessionDir;

protected:
    bool GetSessionInfo(std::string& id, std::string& dir) const override
    {
        if (!hasSession) { return false; }
        id = sessionId;
        dir = sessionDir;
        return true;
    }
};

// Captures log entries on the STAGES channel.
struct StagesLogCapture {
    Dia::Observation::Log::MockSink sink;
    StagesLogCapture() { Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer(); Dia::Observation::Log::Logger::Instance().RegisterSink(&sink); }
    ~StagesLogCapture() { Dia::Observation::Log::Logger::Instance().UnregisterSink(&sink); Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer(); }
    unsigned int Count(Dia::Observation::Log::LogLevel level)
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        unsigned int n = 0;
        for (unsigned int i = 0; i < sink.Count(); ++i)
        {
            const auto& e = sink.entries[i].entry;
            if (e.level == level && e.channel == StringCRC("STAGES")) { ++n; }
        }
        return n;
    }
};

} // namespace

TEST(StageCatalogueSnapshot, FitsSizeBudget)
{
    EXPECT_LE(sizeof(StageCatalogueSnapshot), kMaxStageCatalogueSnapshotBytes);
}

TEST(StageCatalogueSnapshot, HasNoLogPathField)
{
    // The entry carries the session id but no log path: the entry must be much smaller than a String256 + the rest.
    EXPECT_LT(sizeof(StageCatalogueSnapshotEntry), 600u);
}

TEST(StageCatalogueState, DeriveResultsPathFromSessionDir)
{
    const std::string p = DeriveResultsPathFromSessionDir("C:/g/out/App/sessions/20261118_1/");
    EXPECT_EQ(fs::path(p), fs::path("C:/g/out/App/stage_results.json"));
    EXPECT_EQ(fs::path(DeriveResultsPathFromSessionDir("C:/g/out/App/sessions/20261118_1")), fs::path("C:/g/out/App/stage_results.json"));
    EXPECT_TRUE(DeriveResultsPathFromSessionDir("C:/g/out/App/other/x/").empty());
    EXPECT_TRUE(DeriveResultsPathFromSessionDir("").empty());
    EXPECT_TRUE(DeriveResultsPathFromSessionDir(nullptr).empty());
}

TEST(StageCatalogueState, TwoEventsOneFrameOneWrite)
{
    TempDir tmp;
    StageCatalogueState state;
    state.SetResultsPath(tmp.File("r.json").c_str());
    EXPECT_TRUE(state.ApplyEvent(MakeEvent("A", StageResultSeverity::kOk, 10), "", ""));
    EXPECT_TRUE(state.ApplyEvent(MakeEvent("B", StageResultSeverity::kFail, 11), "", ""));
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kWritten);
    EXPECT_EQ(state.WriteCount(), 1u);
    EXPECT_EQ(state.Revision(), 2u);
    // No new events: no write.
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kNothingToWrite);
    EXPECT_EQ(state.WriteCount(), 1u);

    StageResultsTable read;
    ASSERT_EQ(StageResultsFile::Read(tmp.File("r.json").c_str(), read), StageResultsReadResult::kOk);
    EXPECT_EQ(read.Count(), 2u);
}

TEST(StageCatalogueState, NoPersistenceNeverWritesButStillBumpsRevision)
{
    StageCatalogueState state;
    EXPECT_TRUE(state.ApplyEvent(MakeEvent("A", StageResultSeverity::kOk, 10), "", ""));
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kNothingToWrite);
    EXPECT_EQ(state.WriteCount(), 0u);
    EXPECT_EQ(state.Revision(), 1u);
    EXPECT_FALSE(state.LastWriteFailed());
}

TEST(StageCatalogueState, FillsSessionIdAndLogPathOnlyWhenEmpty)
{
    StageCatalogueState state;
    state.ApplyEvent(MakeEvent("Empty", StageResultSeverity::kOk, 1), "sess-1", "C:/s/log.jsonl");
    state.ApplyEvent(MakeEvent("Given", StageResultSeverity::kOk, 1, "mine", "C:/mine.log"), "sess-1", "C:/s/log.jsonl");
    const auto& t = state.Results();
    const int e = t.Find("Empty");
    const int g = t.Find("Given");
    ASSERT_GE(e, 0);
    ASSERT_GE(g, 0);
    EXPECT_STREQ(t[e].result.sessionId.AsCStr(), "sess-1");
    EXPECT_STREQ(t[e].result.logPath.AsCStr(), "C:/s/log.jsonl");
    EXPECT_STREQ(t[g].result.sessionId.AsCStr(), "mine");
    EXPECT_STREQ(t[g].result.logPath.AsCStr(), "C:/mine.log");
}

TEST(StageCatalogueState, RejectedEventDoesNotBumpRevision)
{
    StageCatalogueState state;
    StageResultEvent e = MakeEvent("", StageResultSeverity::kOk, 1);
    e.stage = StringCRC();
    EXPECT_FALSE(state.ApplyEvent(e, "", ""));
    EXPECT_EQ(state.Revision(), 0u);
}

TEST(StageCatalogueState, SnapshotHasEntriesAndResultWithoutLogPath)
{
    StageCatalogueState state;
    ASSERT_EQ(state.LoadCatalogue(DiagamePath("basic").c_str()), StageCatalogueResult::kOk);
    ASSERT_GT(state.Catalogue().Size(), 0u);
    const StageCatalogueEntry& first = state.Catalogue()[0];
    state.ApplyEvent(MakeEvent(first.name.AsChar(), StageResultSeverity::kFail, 77, "sess", "C:/secret/log.jsonl"), "", "");

    std::unique_ptr<StageCatalogueSnapshot> snap(new StageCatalogueSnapshot());
    state.BuildSnapshot(*snap);
    EXPECT_EQ(snap->entryCount, state.Catalogue().Size());
    EXPECT_EQ(snap->revision, 1u);
    EXPECT_EQ(snap->omittedCount, state.Catalogue().OmittedCount());
    EXPECT_EQ(snap->droppedCount, state.Catalogue().DroppedCount());
    EXPECT_EQ(snap->entries[0].name, first.name);
    EXPECT_EQ(snap->entries[0].kinds, first.kinds);
    EXPECT_STREQ(snap->entries[0].description, first.description.AsCStr());
    EXPECT_EQ(snap->entries[0].severity, StageResultSeverity::kFail);
    EXPECT_STREQ(snap->entries[0].label, "FAIL");
    EXPECT_EQ(snap->entries[0].utcMs, 77u);
    EXPECT_STREQ(snap->entries[0].sessionId, "sess");
    // A stage with no result reports kNone.
    for (unsigned int i = 1; i < snap->entryCount; ++i) { EXPECT_EQ(snap->entries[i].severity, StageResultSeverity::kNone); }
    // The log path never reaches the snapshot bytes.
    const char* bytes = reinterpret_cast<const char*>(snap.get());
    const std::string haystack(bytes, sizeof(StageCatalogueSnapshot));
    EXPECT_EQ(haystack.find("secret"), std::string::npos);
}

TEST(StageCatalogueState, PublishOnlyWhenRevisionChanges)
{
    StageCatalogueState state;
    EXPECT_TRUE(state.NeedsPublish());   // once at start
    state.MarkPublished();
    EXPECT_FALSE(state.NeedsPublish());
    state.ApplyEvent(MakeEvent("A", StageResultSeverity::kOk, 1), "", "");
    EXPECT_TRUE(state.NeedsPublish());
    state.MarkPublished();
    EXPECT_FALSE(state.NeedsPublish());
}

TEST(StageCatalogueState, WriteFailureThenRecovers)
{
    TempDir tmp;
    // A regular file used as the parent directory makes the write fail.
    { std::ofstream f(tmp.File("blocker")); f << "x"; }
    StageCatalogueState state;
    state.SetResultsPath((tmp.path / "blocker" / "r.json").string().c_str());
    state.ApplyEvent(MakeEvent("A", StageResultSeverity::kOk, 1), "", "");
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kFailed);
    EXPECT_TRUE(state.LastWriteFailed());
    // A failed write is not retried every frame.
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kNothingToWrite);
    EXPECT_TRUE(state.LastWriteFailed());

    state.SetResultsPath(tmp.File("ok.json").c_str());
    state.ApplyEvent(MakeEvent("B", StageResultSeverity::kOk, 2), "", "");
    EXPECT_EQ(state.FlushFrame(), StageCatalogueState::FlushResult::kWritten);
    EXPECT_FALSE(state.LastWriteFailed());
}

TEST(StageCatalogueModule, StartLoadsCatalogueAndResultsAndPublishesOnce)
{
    TempDir tmp;
    {
        StageResultsTable t;
        t.Set("TestRig", MakeEvent("TestRig", StageResultSeverity::kOk, 5).result);
        ASSERT_TRUE(StageResultsFile::Write(tmp.File("r.json").c_str(), t));
    }
    TestableModule m;
    m.Configure(DiagamePath("basic"), tmp.File("r.json"));
    ASSERT_TRUE(m.Start());
    EXPECT_GT(m.State().Catalogue().Size(), 0u);
    EXPECT_EQ(m.State().Results().Count(), 1u);
    EXPECT_EQ(m.PublishCount(), 1u);
    m.Stop();
}

TEST(StageCatalogueModule, MissingDiagameRunsWithEmptyCatalogue)
{
    TempDir tmp;
    TestableModule m;
    m.Configure((tmp.path / "nope.diagame").string(), tmp.File("r.json"));
    EXPECT_TRUE(m.Start());   // never fails the app
    EXPECT_EQ(m.State().Catalogue().Size(), 0u);
    m.Stop();
}

TEST(StageCatalogueModule, TwoEventsOneFrameOneWriteAndOnePublish)
{
    TempDir tmp;
    TestableModule m;
    m.Configure(DiagamePath("basic"), tmp.File("r.json"));
    ASSERT_TRUE(m.Start());
    const StageResultEvent ev[2] = { MakeEvent("A", StageResultSeverity::kOk, 1), MakeEvent("B", StageResultSeverity::kFail, 2) };
    m.Frame(ev, 2);
    EXPECT_EQ(m.State().WriteCount(), 1u);
    EXPECT_EQ(m.PublishCount(), 2u);   // start + one revision change
    m.Frame(nullptr, 0);
    EXPECT_EQ(m.State().WriteCount(), 1u);
    EXPECT_EQ(m.PublishCount(), 2u);   // unchanged revision: no publish
    m.Stop();
}

TEST(StageCatalogueModule, DefaultResultsPathFromSessionAndFillIn)
{
    TempDir tmp;
    const fs::path sessionDir = tmp.path / "out" / "App" / "sessions" / "sess-9";
    fs::create_directories(sessionDir);
    TestableModule m;
    m.hasSession = true;
    m.sessionId = "sess-9";
    m.sessionDir = sessionDir.string() + "/";
    m.Configure(DiagamePath("basic"), "");
    ASSERT_TRUE(m.Start());
    EXPECT_EQ(fs::path(m.State().ResultsPath()), tmp.path / "out" / "App" / "stage_results.json");

    const StageResultEvent ev = MakeEvent("A", StageResultSeverity::kOk, 1);
    m.Frame(&ev, 1);
    const int i = m.State().Results().Find("A");
    ASSERT_GE(i, 0);
    EXPECT_STREQ(m.State().Results()[i].result.sessionId.AsCStr(), "sess-9");
    EXPECT_NE(std::string(m.State().Results()[i].result.logPath.AsCStr()).find("sess-9"), std::string::npos);
    EXPECT_TRUE(fs::exists(tmp.path / "out" / "App" / "stage_results.json"));
    m.Stop();
}

TEST(StageCatalogueModule, ConfiguredResultsPathWinsOverSession)
{
    TempDir tmp;
    TestableModule m;
    m.hasSession = true;
    m.sessionDir = (tmp.path / "out" / "App" / "sessions" / "s").string() + "/";
    m.Configure(DiagamePath("basic"), tmp.File("mine.json"));
    ASSERT_TRUE(m.Start());
    EXPECT_EQ(fs::path(m.State().ResultsPath()), fs::path(tmp.File("mine.json")));
    m.Stop();
}

TEST(StageCatalogueModule, NoPersistenceLogsExactlyOneWarning)
{
    StagesLogCapture log;
    TestableModule m;
    m.Configure(DiagamePath("basic"), "");
    ASSERT_TRUE(m.Start());
    const StageResultEvent ev = MakeEvent("A", StageResultSeverity::kOk, 1);
    m.Frame(&ev, 1);
    m.Frame(&ev, 1);
    EXPECT_EQ(log.Count(Dia::Observation::Log::LogLevel::kWarning), 1u);
    EXPECT_EQ(m.State().WriteCount(), 0u);
    m.Stop();
}

TEST(StageCatalogueModule, MalformedResultsFileLogsWarningAndStartsEmpty)
{
    TempDir tmp;
    {
        std::ofstream out(tmp.File("bad.json"));
        out << "{ not json";
    }
    StagesLogCapture log;
    TestableModule m;
    m.Configure(DiagamePath("basic"), tmp.File("bad.json"));
    ASSERT_TRUE(m.Start());
    EXPECT_EQ(m.State().Results().Count(), 0u);
    EXPECT_EQ(log.Count(Dia::Observation::Log::LogLevel::kWarning), 1u);
    m.Stop();
}

TEST(StageCatalogueModule, HealthDegradedOnWriteFailureThenRecovers)
{
    Dia::Observation::Testing::HealthFixture healthFixture;
    TempDir tmp;
    { std::ofstream f(tmp.File("blocker")); f << "x"; }
    TestableModule m;
    m.Configure(DiagamePath("basic"), (tmp.path / "blocker" / "r.json").string());
    ASSERT_TRUE(m.Start());
    EXPECT_EQ(m.HealthNow().status, Dia::Observation::Health::HealthStatus::kOK);

    const StageResultEvent ev = MakeEvent("A", StageResultSeverity::kOk, 1);
    m.Frame(&ev, 1);
    EXPECT_EQ(m.HealthNow().status, Dia::Observation::Health::HealthStatus::kDegraded);

    // Remove the blocker so the next event's write succeeds.
    fs::remove(tmp.File("blocker"));
    m.Frame(&ev, 1);
    EXPECT_EQ(m.HealthNow().status, Dia::Observation::Health::HealthStatus::kOK);
    m.Stop();
}
