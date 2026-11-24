#include <gtest/gtest.h>

#include <string.h>
#include <string>
#include <vector>

#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>
#include <Shared/WorldStep/Testing/FakeWorldStage.h>

#include <DiaAttribute/ParameterFile.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveResult.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <direct.h>
#include <cstdio>

#include <Shared/WorldStep/WorldSimSaveCodec.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

using Draoi::HexWorld::SavedField;
using Draoi::HexWorld::SavedKind;
using Draoi::HexWorld::SavedScope;
using Draoi::HexWorld::SaveError;

namespace {

// One dia.value_schema document with one float field entry and an inclusive [min, max].
std::string SchemaText(const char* forName, const char* key, double mn, double mx)
{
    Json::Value entry(Json::objectValue);
    entry["key"] = key;
    entry["kind"] = "field";
    entry["label"] = "Label";
    entry["description"] = "Desc";
    entry["type"] = "float";
    entry["min"] = mn;
    entry["max"] = mx;
    Json::Value doc(Json::objectValue);
    doc["format"] = "dia.value_schema";
    doc["version"] = 1;
    doc["for"] = forName;
    doc["entries"] = Json::Value(Json::arrayValue);
    doc["entries"].append(entry);
    Json::FastWriter w;
    return w.write(doc);
}

bool AddDoc(Dia::ValueSchema::ValueSchemaRegistry& reg, const std::string& text)
{
    Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "fixture.json");
    if (!r.mOk) { return false; }
    for (unsigned i = 0; i < r.mEntries.Size(); ++i)
    {
        if (reg.Register(r.mEntries[i]) != Dia::ValueSchema::RegisterResult::Ok) { return false; }
    }
    return true;
}

// A ready record of 2x2 cells with one per-cell array.
Json::Value Record(const char* arrayName, const float (&values)[4], uint64_t paramsHash = 7)
{
    Json::Value root(Json::objectValue);
    Draoi::HexWorld::SaveHeader h;
    h.width = 2; h.height = 2; h.cellCount = 4; h.stepDay = 3; h.paramsHash = paramsHash; h.basinCount = 0; h.worldHash = 9;
    Draoi::HexWorld::WriteHeader(root, h);
    Draoi::HexWorld::WriteArray(root, arrayName, values, 4);
    return root;
}

bool ValidateWith(const SavedField& f, const Json::Value& record, SaveError& err)
{
    Draoi::HexWorld::Caps caps;
    return Draoi::HexWorld::Validate(record, &f, 1, caps, err);
}

class CountingSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "FakeWorld") == nullptr) { return; }   // only this stage's channel text
        if (e.level == Dia::Observation::Log::LogLevel::kWarning) { ++warnings; }
        if (e.level == Dia::Observation::Log::LogLevel::kInfo) { ++infos; }
        if (strstr(e.message, "discard") != nullptr) { ++discardLogs; }
    }
    const char* GetName() const override { return "WorldSimSaveCodecSink"; }
    unsigned warnings = 0, infos = 0, discardLogs = 0;
};

}  // namespace

class WorldSimSaveCodecTest : public ::testing::Test
{
protected:
    WorldSimSaveCodecTest() : stage(hooks, &hooks.log) {}
    void SetUp() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&sink);
        stage.Start();
    }
    void TearDown() override
    {
        stage.Stop();
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&sink);
        l.UnregisterThreadBuffer();
    }
    CountingSink& Sink() { Dia::Observation::Log::Logger::Instance().FlushSync(); return sink; }

    static WorldView MakeWorld(uint32_t rev, uint32_t w = 2, uint32_t h = 2)
    {
        WorldView v;
        v.revision = rev;
        v.present = true;
        v.width = w;
        v.height = h;
        return v;
    }
    // A ready record: 2x2 cells by default, sim_value is what the fake RestoreCommitted loads.
    static Json::Value ReadyRecord(int simValue = 42, uint64_t hash = 7, uint32_t w = 2, uint32_t h = 2, uint32_t basins = 0)
    {
        Json::Value root(Json::objectValue);
        Draoi::HexWorld::SaveHeader hd;
        hd.width = w; hd.height = h; hd.cellCount = w * h; hd.stepDay = 3; hd.paramsHash = hash;
        hd.basinCount = basins; hd.worldHash = 9;
        Draoi::HexWorld::WriteHeader(root, hd);
        root["sim_value"] = simValue;
        return root;
    }
    void Announce(const WorldView& w) { stage.OnWorldGenerationComplete(w); }
    bool Called(const char* name) const
    {
        for (uint32_t i = 0; i < hooks.log.Count(); ++i) { if (strcmp(hooks.log.At(i), name) == 0) { return true; } }
        return false;
    }
    uint32_t IndexOf(const char* name) const
    {
        for (uint32_t i = 0; i < hooks.log.Count(); ++i) { if (strcmp(hooks.log.At(i), name) == 0) { return i; } }
        return 0xFFFFFFFFu;
    }

    FakeHooks hooks;
    FakeStage stage;
    CountingSink sink;
    Dia::ParallelExecutor::SerialExecutor exec;
};

// ---- Task 15b: ranges from sidecars ----------------------------------------------------------------

TEST_F(WorldSimSaveCodecTest, LayerSidecarMaxDrivesValidateWithNoCodeChange)
{
    const FieldDecl decl = { "cloud", SavedKind::F32, SavedScope::PerCell };
    const float ok[4] = { 0.f, 0.5f, 1.f, 1.f };
    const float high[4] = { 0.f, 0.5f, 1.f, 2.f };

    {
        Dia::ValueSchema::ValueSchemaRegistry reg;
        ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
        reg.Publish();
        SavedField f;
        SaveError err;
        ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
        EXPECT_EQ(1.0, f.max);
        EXPECT_TRUE(ValidateWith(f, Record("cloud", ok), err));
        EXPECT_FALSE(ValidateWith(f, Record("cloud", high), err));
        EXPECT_STREQ("cloud", err.field.AsCStr());
    }
    {
        // Same code, sidecar max changed to 3: the value 2 is now accepted.
        Dia::ValueSchema::ValueSchemaRegistry reg;
        ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 3.0)));
        reg.Publish();
        SavedField f;
        SaveError err;
        ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
        EXPECT_TRUE(ValidateWith(f, Record("cloud", high), err));
    }
}

TEST_F(WorldSimSaveCodecTest, ResolvesThroughTheServiceToo)
{
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.hydro.layers", "flow", -2.0, 5.0)));
    reg.Publish();
    Dia::ValueSchema::ValueSchemaService svc(reg);
    const FieldDecl decl = { "flow", SavedKind::F32, SavedScope::PerCell };
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(svc, "hydro", &decl, 1, &f, err));
    EXPECT_EQ(-2.0, f.min);
    EXPECT_EQ(5.0, f.max);
}

TEST_F(WorldSimSaveCodecTest, StateFieldWithoutLayerEntryReadsTheStateSchema)
{
    const FieldDecl decl = { "snow_depth", SavedKind::F32, SavedScope::PerCell };
    const float v[4] = { 0.f, 1.f, 4.f, 4.f };
    const float tooBig[4] = { 0.f, 1.f, 4.f, 5.f };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather_state", "snow_depth", 0.0, 4.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
    EXPECT_EQ(4.0, f.max);
    EXPECT_TRUE(ValidateWith(f, Record("snow_depth", v), err));
    EXPECT_FALSE(ValidateWith(f, Record("snow_depth", tooBig), err));
}

TEST_F(WorldSimSaveCodecTest, LayerEntryWinsOverStateEntry)
{
    const FieldDecl decl = { "cloud", SavedKind::F32, SavedScope::PerCell };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather_state", "cloud", 0.0, 9.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    ASSERT_TRUE(ResolveSavedFields(reg, "weather", &decl, 1, &f, err));
    EXPECT_EQ(1.0, f.max);
}

TEST_F(WorldSimSaveCodecTest, FieldWithNoEntryOrNoRangeIsRefusedNamingTheField)
{
    const FieldDecl missing = { "ghost", SavedKind::F32, SavedScope::PerCell };
    Dia::ValueSchema::ValueSchemaRegistry reg;
    ASSERT_TRUE(AddDoc(reg, SchemaText("cow.weather.layers", "cloud", 0.0, 1.0)));
    reg.Publish();
    SavedField f;
    SaveError err;
    EXPECT_FALSE(ResolveSavedFields(reg, "weather", &missing, 1, &f, err));
    EXPECT_STREQ("ghost", err.field.AsCStr());

    // Unknown system token: no key can be built.
    const FieldDecl cloud = { "cloud", SavedKind::F32, SavedScope::PerCell };
    EXPECT_FALSE(ResolveSavedFields(reg, "nope", &cloud, 1, &f, err));
    EXPECT_STREQ("cloud", err.field.AsCStr());
}

// ---- Task 17: params_hash and versioning -----------------------------------------------------------

namespace {

const char* kParamsSidecar =
    "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":["
    "{\"key\":\"a\",\"kind\":\"parameter\",\"label\":\"A\",\"description\":\"d\",\"type\":\"float\",\"min\":0,\"max\":10,\"default\":1}"
    "]}";

}  // namespace

TEST_F(WorldSimSaveCodecTest, ParamsHashFromParameterFileMatchesAndDetectsAChange)
{
    Dia::Attribute::ParameterFile file;
    Dia::Attribute::ParamError perr;
    ASSERT_TRUE(file.Load(kParamsSidecar, "params.json", nullptr, perr));
    const uint64_t hash = file.Hash();

    const float v[4] = { 0.f, 0.f, 0.f, 0.f };
    const Json::Value record = Record("cloud", v, hash);
    EXPECT_TRUE(ParamsHashMatches(record, hash));

    Json::Value value(2.5);
    ASSERT_TRUE(file.Set(Dia::Core::StringCRC("a"), value, perr));
    EXPECT_NE(hash, file.Hash());
    EXPECT_FALSE(ParamsHashMatches(record, file.Hash()));
}

TEST_F(WorldSimSaveCodecTest, ParamsHashMatchesRefusesRecordsWithoutAHash)
{
    Json::Value notReady(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(notReady);
    EXPECT_FALSE(ParamsHashMatches(notReady, 0));
    EXPECT_FALSE(ParamsHashMatches(Json::Value(Json::nullValue), 0));
}

TEST_F(WorldSimSaveCodecTest, SaveIdIsCowDotSystem)
{
    EXPECT_STREQ("cow.weather", World::SaveId("weather").c_str());
    EXPECT_STREQ("cow.hydro", World::SaveId("hydro").c_str());
}

namespace {

// A tiny participant: v2 expects a `layout` int that v1 records do not have; the migration adds it.
struct FakeRecordSaveable : Dia::SaveGame::ISaveable
{
    uint32_t version = 1;
    int32_t layout = 0;
    bool sawVersionKey = false;
    void Serialize(Dia::SaveGame::SaveContext& ctx) const override
    {
        ctx.Write(Dia::Core::StringCRC("ready"), true);
        ctx.Write(Dia::Core::StringCRC("step_day"), static_cast<int32_t>(5));
        if (version >= 2) { ctx.Write(Dia::Core::StringCRC("layout"), static_cast<int32_t>(2)); }
    }
    void Deserialize(Dia::SaveGame::LoadContext& ctx) override
    {
        sawVersionKey = ctx.CurrentNode().isMember("version");
        ctx.Read(Dia::Core::StringCRC("layout"), layout);
    }
    uint32_t GetVersion() const override { return version; }
};

}  // namespace

TEST_F(WorldSimSaveCodecTest, V1RecordLoadsAsV2ThroughARegisteredMigration)
{
    _mkdir("temp");
    _mkdir("temp/worldsim_codec_tests");
    const Dia::Core::StringCRC id(World::SaveId("weather").c_str());
    Dia::SaveGame::SaveConfig cfg{ "temp/worldsim_codec_tests/", "slot_{id}.sav", 0, Dia::SaveGame::SaveFormat::Json };

    FakeRecordSaveable v1;
    v1.version = 1;
    Dia::SaveGame::SaveRegistry saveReg;
    saveReg.Register(id, &v1);
    Dia::SaveGame::SaveManager saveMgr;
    saveMgr.Init(cfg, saveReg);
    ASSERT_TRUE(saveMgr.Save(Dia::Core::StringCRC("wsc_v1")).Ok());

    FakeRecordSaveable v2;
    v2.version = 2;
    Dia::SaveGame::SaveRegistry loadReg;
    loadReg.Register(id, &v2);
    bool migrated = false;
    loadReg.RegisterMigration(id, 1, [&migrated](Dia::SaveGame::LoadContext& ctx) {
        migrated = true;
        ctx.Write(Dia::Core::StringCRC("layout"), static_cast<int32_t>(2));
    });
    Dia::SaveGame::SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);
    const Dia::SaveGame::LoadResult lr = loadMgr.Load(Dia::Core::StringCRC("wsc_v1"));
    ASSERT_TRUE(lr.Ok());
    EXPECT_TRUE(migrated);
    EXPECT_EQ(2, v2.layout);
    EXPECT_FALSE(v2.sawVersionKey) << "no in-record version field";
    EXPECT_EQ(2u, v2.GetVersion());
    std::remove("temp/worldsim_codec_tests/slot_wsc_v1.sav");
}

// ---- Task 18: the 7-step restore ---------------------------------------------------------------------

typedef WorldStep::RestoreResult Rr;

// Step 1: cancel the job, drop pending, ignore a stale spin-up completion.
TEST_F(WorldSimSaveCodecTest, RestoreStep1CancelsTheJobAndDropsPending)
{
    const WorldView w = MakeWorld(1);
    stage.SpinUpNow(1);                       // pending sim present
    ASSERT_TRUE(stage.HasPending());
    Announce(MakeWorld(2));                   // a new job is running for revision 2
    ASSERT_TRUE(stage.SpinUpRunning());
    EXPECT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    EXPECT_FALSE(stage.HasPending());
    EXPECT_FALSE(stage.SpinUpRunning()) << "the running job was cancelled";
    (void)w;
}

TEST_F(WorldSimSaveCodecTest, RestoreStep1LateCompletionOfTheCancelledJobIsIgnored)
{
    const WorldView w = MakeWorld(1);
    Announce(w);
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    // The worker finishes after the restore: the cancelled job must not produce a sim.
    stage.RunJobNow(1, stage.VisitGeneration(), w);
    stage.Update();
    EXPECT_FALSE(stage.HasPending());
    EXPECT_EQ(1, FakeSim::sLive.load()) << "only the restored active sim is alive";
    stage.Capture(5);
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(42 * 2 + 5, hooks.publishedValue) << "the restored sim, not a spun-up one, was stepped";
}

// Step 2: ready:false or no world -> stop.
TEST_F(WorldSimSaveCodecTest, RestoreStep2NotReadyRecordStops)
{
    Announce(MakeWorld(1));
    Json::Value rec(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(rec);
    hooks.log.Clear();
    EXPECT_EQ(Rr::kNotReady, stage.Restore(rec));
    EXPECT_FALSE(Called("InitialiseSim"));
    EXPECT_FALSE(Called("RestoreCommitted"));
    EXPECT_FALSE(stage.IsReady());
    EXPECT_TRUE(stage.SpinUpRunning()) << "normal spin-up continues for the announced world";
}

TEST_F(WorldSimSaveCodecTest, RestoreStep2NoWorldStops)
{
    hooks.log.Clear();
    EXPECT_NE(Rr::kApplied, stage.Restore(ReadyRecord()));
    EXPECT_FALSE(Called("InitialiseSim"));
    EXPECT_FALSE(Called("RestoreCommitted"));
    EXPECT_FALSE(stage.IsReady());
}

// Step 3: dimension or extra-count mismatch -> Warning, discard, spin up.
struct MismatchCase { const char* name; uint32_t w, h, basins; bool extraOk; };

TEST_F(WorldSimSaveCodecTest, RestoreStep3MismatchWarnsDiscardsAndSpinsUp)
{
    static const MismatchCase kCases[] = {
        { "width",  3, 2, 0, true  },
        { "height", 2, 3, 0, true  },
        { "both",   4, 4, 0, true  },
        { "extra",  2, 2, 5, false },
    };
    for (const MismatchCase& c : kCases)
    {
        SCOPED_TRACE(c.name);
        Announce(MakeWorld(1));
        hooks.log.Clear();
        hooks.extraCountOk = c.extraOk;
        const unsigned before = Sink().warnings;
        EXPECT_EQ(Rr::kDiscarded, stage.Restore(ReadyRecord(42, 7, c.w, c.h, c.basins)));
        EXPECT_EQ(before + 1, Sink().warnings) << "exactly one Warning";
        EXPECT_FALSE(Called("RestoreCommitted"));
        EXPECT_FALSE(stage.IsReady());
        EXPECT_FALSE(stage.HasActive());
        EXPECT_TRUE(stage.SpinUpRunning()) << "the discard falls back to a normal spin-up";
        hooks.extraCountOk = true;
    }
}

// Step 4: InitialiseSim, then RestoreCommitted; a failure of either discards.
TEST_F(WorldSimSaveCodecTest, RestoreStep4InitialisesThenRestoresInOrder)
{
    Announce(MakeWorld(1));
    hooks.log.Clear();
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    ASSERT_NE(0xFFFFFFFFu, IndexOf("InitialiseSim"));
    ASSERT_NE(0xFFFFFFFFu, IndexOf("RestoreCommitted"));
    EXPECT_LT(IndexOf("InitialiseSim"), IndexOf("RestoreCommitted"));
}

TEST_F(WorldSimSaveCodecTest, RestoreStep4InitFailureDiscards)
{
    Announce(MakeWorld(1));
    hooks.log.Clear();
    hooks.initFails = true;
    EXPECT_EQ(Rr::kDiscarded, stage.Restore(ReadyRecord(42)));
    EXPECT_TRUE(Called("InitialiseSim"));
    EXPECT_FALSE(Called("RestoreCommitted"));
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.HasActive());
    EXPECT_EQ(0, FakeSim::sLive.load()) << "the half-built sim was freed";
}

TEST_F(WorldSimSaveCodecTest, RestoreStep4RestoreCommittedFailureDiscards)
{
    Announce(MakeWorld(1));
    hooks.restoreFails = true;
    EXPECT_EQ(Rr::kDiscarded, stage.Restore(ReadyRecord(42)));
    EXPECT_TRUE(Called("RestoreCommitted"));
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.HasActive());
    EXPECT_EQ(0, FakeSim::sLive.load());
    EXPECT_TRUE(stage.SpinUpRunning());
}

// Step 5: active = target = current revision, ready at once.
TEST_F(WorldSimSaveCodecTest, RestoreStep5ActivatesAtTheCurrentRevisionAndIsReadyAtOnce)
{
    Announce(MakeWorld(4));
    EXPECT_FALSE(stage.IsReady());
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    EXPECT_TRUE(stage.IsReady());
    EXPECT_TRUE(stage.HasActive());
    EXPECT_FALSE(stage.HasPending());
    EXPECT_EQ(4u, stage.TargetRevision());
    EXPECT_EQ(4u, stage.RestoredRevision());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
    stage.Capture(5);
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(42 * 2 + 5, hooks.publishedValue);
}

// Step 6: params_hash mismatch -> one Warning, Degraded until a reload.
TEST_F(WorldSimSaveCodecTest, RestoreStep6MatchingParamsHashStaysOk)
{
    stage.SetCurrentParamsHash(7);
    Announce(MakeWorld(1));
    const unsigned before = Sink().warnings;
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42, 7)));
    EXPECT_EQ(before, Sink().warnings);
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
}

TEST_F(WorldSimSaveCodecTest, RestoreStep6HashMismatchWarnsOnceAndDegradesUntilReload)
{
    stage.SetCurrentParamsHash(8);
    Announce(MakeWorld(1));
    const unsigned before = Sink().warnings;
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42, 7)));
    EXPECT_EQ(before + 1, Sink().warnings) << "one Warning";
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kDegraded, stage.GetHealth().status);
    EXPECT_TRUE(stage.IsReady()) << "degraded, still ready";

    // A full-queue cycle must not clear the params degradation.
    for (uint32_t i = 0; i < FakeBase::kQueueCapacity + 1; ++i) { FakeEntry e; stage.Enqueue(e); }
    stage.Capture(1);
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kDegraded, stage.GetHealth().status);

    stage.NotifyParamsReloaded();
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
    stage.NotifyParamsReloaded();   // idempotent
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
}

TEST_F(WorldSimSaveCodecTest, RestoreStep6WithoutAKnownHashDoesNotCompare)
{
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42, 12345)));
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
}

// Step 7: a re-announce at the restored revision is ignored.
TEST_F(WorldSimSaveCodecTest, RestoreStep7ReannounceAtRestoredRevisionIsIgnored)
{
    Announce(MakeWorld(3));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    hooks.log.Clear();
    Announce(MakeWorld(3));
    EXPECT_FALSE(stage.SpinUpRunning());
    EXPECT_FALSE(Called("InitialiseSim"));
    EXPECT_TRUE(stage.IsReady());
    EXPECT_EQ(1, FakeSim::sLive.load());
}

TEST_F(WorldSimSaveCodecTest, RestoreStep7NewRevisionAfterRestoreSpinsUpNormally)
{
    Announce(MakeWorld(3));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    Announce(MakeWorld(4));
    EXPECT_TRUE(stage.SpinUpRunning());
    EXPECT_FALSE(stage.IsReady()) << "revision 4 has no sim yet";
}

TEST_F(WorldSimSaveCodecTest, RestoredStageDoesNotRespinWhenStartedAgain)
{
    Announce(MakeWorld(3));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    stage.Start();   // already started: no-op
    EXPECT_FALSE(stage.SpinUpRunning());
    EXPECT_TRUE(stage.IsReady());
}

// ---- Task 19: event-order fallback and the pending blob ------------------------------------------------

TEST_F(WorldSimSaveCodecTest, FallbackStashesTheRecordUntilTheWorldArrives)
{
    hooks.log.Clear();
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42)));
    EXPECT_TRUE(stage.HasStashedRecord());
    EXPECT_FALSE(Called("InitialiseSim"));
    EXPECT_FALSE(stage.IsReady());

    Announce(MakeWorld(6));
    EXPECT_FALSE(stage.HasStashedRecord());
    EXPECT_TRUE(stage.IsReady());
    EXPECT_FALSE(stage.SpinUpRunning()) << "restored, so no spin-up";
    EXPECT_EQ(6u, stage.RestoredRevision());
    stage.Capture(5);
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(42 * 2 + 5, hooks.publishedValue);
}

TEST_F(WorldSimSaveCodecTest, FallbackStashAgainstAMismatchingWorldDiscardsAndSpinsUp)
{
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42, 7, 3, 3)));
    const unsigned before = Sink().warnings;
    Announce(MakeWorld(6, 2, 2));
    EXPECT_EQ(before + 1, Sink().warnings);
    EXPECT_FALSE(stage.HasStashedRecord());
    EXPECT_FALSE(stage.IsReady());
    EXPECT_TRUE(stage.SpinUpRunning());
}

TEST_F(WorldSimSaveCodecTest, FallbackStashIsAppliedOnceOnly)
{
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42)));
    Announce(MakeWorld(6));
    ASSERT_TRUE(stage.IsReady());
    hooks.log.Clear();
    Announce(MakeWorld(6));                    // re-announce: step 7
    Announce(MakeWorld(7));                    // later world: normal spin-up, no second restore
    EXPECT_FALSE(Called("RestoreCommitted"));
    EXPECT_TRUE(stage.SpinUpRunning());
}

TEST_F(WorldSimSaveCodecTest, FallbackStashIsDroppedWhenTheWorldFails)
{
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42)));
    stage.OnWorldGenerationFailed();
    EXPECT_FALSE(stage.HasStashedRecord());
}

TEST_F(WorldSimSaveCodecTest, NotReadyRecordIsNotStashedWithoutAWorld)
{
    Json::Value rec(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(rec);
    EXPECT_EQ(Rr::kNotReady, stage.Restore(rec));
    EXPECT_FALSE(stage.HasStashedRecord());
}

namespace {

const SavedField kValueField = { "value", SavedKind::F32, SavedScope::PerCell, 0.0, 1000.0 };

RecordSpec MakeSpec(bool biome = false)
{
    RecordSpec s;
    s.fields = &kValueField;
    s.fieldCount = 1;
    s.hasBiome = biome;
    return s;
}

Json::Value ValidBlob(float v = 89.f)
{
    Json::Value blob(Json::objectValue);
    WritePendingHeader(blob, 5, 4);
    const float arr[4] = { v, v, v, v };
    Draoi::HexWorld::WriteArray(blob, "value", arr, 4);
    return blob;
}

Json::Value CommittedWithPalette()
{
    Json::Value root(Json::objectValue);
    Draoi::HexWorld::SaveHeader hd;
    hd.width = 2; hd.height = 2; hd.cellCount = 4; hd.stepDay = 3; hd.paramsHash = 7; hd.worldHash = 9;
    Draoi::HexWorld::WriteHeader(root, hd);
    Json::Value palette(Json::arrayValue);
    palette.append("forest");
    palette.append("lake");
    root["biome_palette"] = palette;
    return root;
}

}  // namespace

TEST_F(WorldSimSaveCodecTest, PendingInvalidFormIsExactlyVersionAndValidFalse)
{
    Json::Value blob(Json::objectValue);
    WritePendingInvalid(blob);
    EXPECT_EQ(2u, blob.size());
    EXPECT_EQ(1u, blob["pending_version"].asUInt());
    EXPECT_TRUE(blob["valid"].isBool());
    EXPECT_FALSE(blob["valid"].asBool());
    SaveError err;
    EXPECT_TRUE(ValidatePendingBlob(blob, nullptr, MakeSpec(), err)) << "needs no committed record";
}

TEST_F(WorldSimSaveCodecTest, ValidatePendingBlobRejectsCorruptBlobsNamingTheField)
{
    const Json::Value committed = ReadyRecord(42);
    SaveError err;
    ASSERT_TRUE(ValidatePendingBlob(ValidBlob(), &committed, MakeSpec(), err)) << "baseline accepted";

    struct Case { const char* name; const char* field; Json::Value blob; };
    std::vector<Case> cases;
    { Json::Value b = ValidBlob(); b["value"][1u] = 5000.0;            cases.push_back({ "out of range", "value", b }); }
    { Json::Value b = ValidBlob(); b["value"].append(1.0);              cases.push_back({ "too long", "value", b }); }
    { Json::Value b = ValidBlob(); b.removeMember("value");            cases.push_back({ "missing array", "value", b }); }
    { Json::Value b = ValidBlob(); b["value"][2u] = "x";               cases.push_back({ "not a number", "value", b }); }
    { Json::Value b = ValidBlob(); b["pending_version"] = 2;           cases.push_back({ "future version", "pending_version", b }); }
    { Json::Value b = ValidBlob(); b.removeMember("pending_version");  cases.push_back({ "no version", "pending_version", b }); }
    { Json::Value b = ValidBlob(); b["valid"] = 1;                     cases.push_back({ "valid not bool", "valid", b }); }
    { Json::Value b = ValidBlob(); b["cell_count"] = 9;                cases.push_back({ "cell_count", "cell_count", b }); }
    { Json::Value b = ValidBlob(); b.removeMember("step_day");         cases.push_back({ "no step_day", "step_day", b }); }
    { Json::Value b = ValidBlob(); b["biome_palette"] = Json::Value(Json::arrayValue); cases.push_back({ "repeats palette", "biome_palette", b }); }
    cases.push_back({ "not an object", "pending_version", Json::Value(3) });
    for (const Case& c : cases)
    {
        SCOPED_TRACE(c.name);
        SaveError e;
        EXPECT_FALSE(ValidatePendingBlob(c.blob, &committed, MakeSpec(), e));
        EXPECT_STREQ(c.field, e.field.AsCStr());
    }
}

TEST_F(WorldSimSaveCodecTest, ValidatePendingBlobWithoutACommittedRecordIsRejectedNamingTheField)
{
    Json::Value notReady(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(notReady);
    SaveError e1, e2, e3;
    EXPECT_FALSE(ValidatePendingBlob(ValidBlob(), nullptr, MakeSpec(), e1));
    EXPECT_STREQ("committed_record", e1.field.AsCStr());
    EXPECT_FALSE(ValidatePendingBlob(ValidBlob(), &notReady, MakeSpec(), e2));
    EXPECT_STREQ("committed_record", e2.field.AsCStr());
    // With a biome layer the field that cannot be decoded is the palette.
    EXPECT_FALSE(ValidatePendingBlob(ValidBlob(), nullptr, MakeSpec(true), e3));
    EXPECT_STREQ("biome_palette", e3.field.AsCStr());
}

TEST_F(WorldSimSaveCodecTest, PendingBlobDecodesBiomeAgainstTheCommittedPalette)
{
    const Json::Value committed = CommittedWithPalette();
    Json::Value blob = ValidBlob();
    Json::Value biome(Json::arrayValue);
    for (int i = 0; i < 4; ++i) { biome.append(i % 2); }
    blob["biome"] = biome;
    SaveError err;
    EXPECT_TRUE(ValidatePendingBlob(blob, &committed, MakeSpec(true), err)) << err.field.AsCStr();
    EXPECT_FALSE(blob.isMember("biome_palette")) << "the blob carries no palette of its own";

    Json::Value outside = blob;
    outside["biome"][3u] = 2;                    // palette has 2 entries
    EXPECT_FALSE(ValidatePendingBlob(outside, &committed, MakeSpec(true), err));
    EXPECT_STREQ("biome", err.field.AsCStr());

    Json::Value shortBiome = blob;
    shortBiome["biome"] = Json::Value(Json::arrayValue);
    EXPECT_FALSE(ValidatePendingBlob(shortBiome, &committed, MakeSpec(true), err));
    EXPECT_STREQ("biome", err.field.AsCStr());

    Json::Value missing = blob;
    missing.removeMember("biome");
    EXPECT_FALSE(ValidatePendingBlob(missing, &committed, MakeSpec(true), err));
    EXPECT_STREQ("biome", err.field.AsCStr());
}

TEST_F(WorldSimSaveCodecTest, StageValidatePendingUsesTheNotedCommittedRecordAndIsReadOnly)
{
    const RecordSpec spec = MakeSpec();
    stage.SetRecordSpec(&spec);
    const Json::Value blob = ValidBlob();
    Dia::SaveGame::LoadContext ctx(blob);
    EXPECT_FALSE(stage.ValidatePending(ctx)) << "no committed record known yet";
    stage.NoteCommittedRecord(ReadyRecord(42));
    EXPECT_TRUE(stage.ValidatePending(ctx));
    EXPECT_FALSE(stage.HasActive()) << "validation never touches live state";
    EXPECT_FALSE(stage.IsReady());

    Json::Value bad = blob;
    bad["value"][0u] = -1.0;
    Dia::SaveGame::LoadContext badCtx(bad);
    EXPECT_FALSE(stage.ValidatePending(badCtx));
}

TEST_F(WorldSimSaveCodecTest, StageValidatePendingRejectsWhenTheNotedCommittedRecordIsNotReady)
{
    const RecordSpec spec = MakeSpec();
    stage.SetRecordSpec(&spec);
    Json::Value notReady(Json::objectValue);
    Draoi::HexWorld::WriteNotReady(notReady);
    stage.NoteCommittedRecord(notReady);
    const Json::Value blob = ValidBlob();
    Dia::SaveGame::LoadContext ctx(blob);
    EXPECT_FALSE(stage.ValidatePending(ctx)) << "a ready:false record cannot decode a pending blob";
    EXPECT_FALSE(stage.HasActive());
    // A later ready record makes the same blob valid again.
    stage.NoteCommittedRecord(ReadyRecord(42));
    EXPECT_TRUE(stage.ValidatePending(ctx));
}

TEST_F(WorldSimSaveCodecTest, PendingBlobRoundTripsAStepInFlight)
{
    const RecordSpec spec = MakeSpec();
    stage.SetRecordSpec(&spec);
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    stage.Capture(5);
    stage.Run(exec);                             // working value is now 42 * 2 + 5 = 89

    Dia::SaveGame::SaveContext out;
    stage.SerializePending(out);
    const Json::Value blob = out.Root();
    ASSERT_TRUE(blob["valid"].asBool());
    EXPECT_EQ(1u, blob["pending_version"].asUInt());
    EXPECT_EQ(5u, blob["step_day"].asUInt());
    EXPECT_EQ(4u, blob["cell_count"].asUInt());
    EXPECT_EQ(89, blob["value"][0u].asInt());
    EXPECT_FALSE(blob.isMember("biome_palette"));

    // A second stage restored from the same committed record takes the blob back.
    FakeHooks hooks2;
    FakeStage stage2(hooks2, &hooks2.log);
    stage2.SetRecordSpec(&spec);
    stage2.Start();
    stage2.OnWorldGenerationComplete(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage2.Restore(ReadyRecord(42)));
    Dia::SaveGame::LoadContext ctx(blob);
    EXPECT_TRUE(stage2.ValidatePending(ctx));
    stage2.DeserializePending(ctx);
    EXPECT_EQ(89, hooks2.pendingRestoredValue);
    EXPECT_TRUE(stage2.IsReady());
    stage2.Stop();
}

TEST_F(WorldSimSaveCodecTest, RestoredPendingStepCommitsAtItsStepDayAndReSerializes)
{
    const RecordSpec spec = MakeSpec();
    stage.SetRecordSpec(&spec);
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    stage.Capture(5);
    stage.Run(exec);
    Dia::SaveGame::SaveContext out;
    stage.SerializePending(out);
    const Json::Value blob = out.Root();
    ASSERT_TRUE(blob["valid"].asBool());

    FakeHooks hooks2;
    FakeStage stage2(hooks2, &hooks2.log);
    stage2.SetRecordSpec(&spec);
    stage2.Start();
    stage2.OnWorldGenerationComplete(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage2.Restore(ReadyRecord(42)));
    Dia::SaveGame::LoadContext ctx(blob);
    ASSERT_TRUE(stage2.ValidatePending(ctx));
    stage2.DeserializePending(ctx);

    Dia::SaveGame::SaveContext again;
    stage2.SerializePending(again);
    EXPECT_TRUE(again.Root()["valid"].asBool());
    EXPECT_EQ(5u, again.Root()["step_day"].asUInt());

    stage2.Commit();
    EXPECT_EQ(1u, hooks2.publishCount);
    EXPECT_EQ(5u, hooks2.publishedDay);
    EXPECT_EQ(89, hooks2.publishedValue);
    stage2.Stop();
}

TEST_F(WorldSimSaveCodecTest, InvalidPendingBlobLeavesCommitALoggedNoOp)
{
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    Json::Value blob(Json::objectValue);
    WritePendingInvalid(blob);
    Dia::SaveGame::LoadContext ctx(blob);
    stage.DeserializePending(ctx);
    hooks.publishCount = 0;
    const unsigned before = Sink().discardLogs;
    stage.Commit();
    EXPECT_EQ(0u, hooks.publishCount);
    EXPECT_EQ(before + 1, Sink().discardLogs);
    Dia::SaveGame::SaveContext out;
    stage.SerializePending(out);
    EXPECT_FALSE(out.Root()["valid"].asBool());
}

TEST_F(WorldSimSaveCodecTest, DiscardedStepWritesTheInvalidFormAndRoundTrips)
{
    const RecordSpec spec = MakeSpec();
    stage.SetRecordSpec(&spec);
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    stage.Capture(5);
    stage.Run(exec);
    Announce(MakeWorld(2));                      // the world changed mid-step: Commit discards
    stage.Commit();

    Dia::SaveGame::SaveContext out;
    stage.SerializePending(out);
    const Json::Value blob = out.Root();
    EXPECT_EQ(2u, blob.size());
    EXPECT_FALSE(blob["valid"].asBool());
    EXPECT_EQ(1u, blob["pending_version"].asUInt());

    Dia::SaveGame::LoadContext ctx(blob);
    stage.NoteCommittedRecord(ReadyRecord(42));
    EXPECT_TRUE(stage.ValidatePending(ctx));
    hooks.log.Clear();
    stage.DeserializePending(ctx);
    EXPECT_FALSE(Called("RestorePending"));
}

TEST_F(WorldSimSaveCodecTest, NoStepInFlightWritesTheInvalidForm)
{
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    Dia::SaveGame::SaveContext out;
    stage.SerializePending(out);
    EXPECT_FALSE(out.Root()["valid"].asBool());
    EXPECT_EQ(2u, out.Root().size());
}

TEST_F(WorldSimSaveCodecTest, PendingBlobThatCannotBeRestoredLeavesTheStageNotReady)
{
    Announce(MakeWorld(1));
    ASSERT_EQ(Rr::kApplied, stage.Restore(ReadyRecord(42)));
    ASSERT_TRUE(stage.IsReady());
    hooks.pendingRestoreFails = true;
    const Json::Value blob = ValidBlob();
    Dia::SaveGame::LoadContext ctx(blob);
    const unsigned before = Sink().warnings;
    stage.DeserializePending(ctx);
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.HasActive());
    EXPECT_EQ(before + 1, Sink().warnings);
    // A step on a not-ready stage is tolerated: nothing runs, nothing is published.
    hooks.publishCount = 0;
    stage.Capture(6);
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(0u, hooks.publishCount);
}

TEST_F(WorldSimSaveCodecTest, PendingBlobBeforeTheWorldIsAppliedAfterTheRestore)
{
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42)));
    const Json::Value blob = ValidBlob(77.f);
    Dia::SaveGame::LoadContext ctx(blob);
    stage.DeserializePending(ctx);
    EXPECT_FALSE(Called("RestorePending")) << "no sim yet";
    Announce(MakeWorld(1));
    EXPECT_TRUE(Called("RestorePending"));
    EXPECT_EQ(77, hooks.pendingRestoredValue);
    EXPECT_TRUE(stage.IsReady());
}

TEST_F(WorldSimSaveCodecTest, DiscardedRestoreDropsAStashedPendingBlob)
{
    ASSERT_EQ(Rr::kStashed, stage.Restore(ReadyRecord(42, 7, 3, 3)));
    const Json::Value blob = ValidBlob(77.f);
    Dia::SaveGame::LoadContext ctx(blob);
    stage.DeserializePending(ctx);
    Announce(MakeWorld(1));                      // 2x2 world vs 3x3 record: discarded
    EXPECT_FALSE(Called("RestorePending"));
    EXPECT_FALSE(stage.IsReady());
}

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
