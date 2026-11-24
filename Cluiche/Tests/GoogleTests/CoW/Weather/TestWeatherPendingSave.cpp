// A4 weather-save, Task 13: the weather stage's pending (in-flight step) blob. Covers ACs 16, 17, 19, 19a and 21 for the
// pending blob, through the real WeatherStage, the real WeatherSaveParticipant and real JSON text. The blob is written
// and read inside a nested stage_0 object, the way WorldStepModule does it.
#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Modules/Weather/WeatherSaveParticipant.h>
#include <Shared/CoWCalendar.h>
#include <Shared/Weather/WeatherSaveFields.h>
#include <Shared/Weather/WeatherStage.h>

#include "../WorldStep/WorldStepSaveRig.h"
#include "WeatherTestWorld.h"

namespace
{
using CoW::Weather::kWeatherSaveFieldCount;
using Draoi::HexWorld::Weather::Season;
using Draoi::HexWorld::Weather::WeatherFields;

const char* kLayersSidecarPath = "Cluiche/Assets/CoW/Global/Weather/weather_layers.schema.json";
const char* kFieldNames[kWeatherSaveFieldCount] = { "moisture", "temperature", "pressure", "wind_direction", "wind_speed",
                                                    "precipitation", "cloud_cover", "water_to_ground", "snow_pack",
                                                    "pressure_anomaly" };

std::string ReadShippedSidecar()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / kLayersSidecarPath;
        if (std::filesystem::is_regular_file(candidate, ec))
        {
            std::ifstream in(candidate, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    ADD_FAILURE() << "sidecar not found: " << kLayersSidecarPath;
    return std::string();
}

bool PublishSidecar(Dia::ValueSchema::ValueSchemaRegistry& reg, const std::string& text)
{
    Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "fixture.json");
    if (!r.mOk) { return false; }
    for (unsigned i = 0; i < r.mEntries.Size(); ++i)
    {
        if (reg.Register(r.mEntries[i]) != Dia::ValueSchema::RegisterResult::Ok) { return false; }
    }
    reg.Publish();
    return true;
}

class OneWorldSource : public CoW::Weather::IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, Draoi::HexWorld::Weather::WeatherTerrain& terrain) override
    {
        Draoi::HexWorld::Weather::Testing::HexTerrainOptions o;
        o.waterColumns = waterColumns;
        WeatherTestWorld::CopyTerrain(Draoi::HexWorld::Weather::Testing::MakeHexTerrain(columns, rows, o), terrain);
        view.present = true;
        view.width = columns;
        view.height = rows;
        return true;
    }
    uint32_t columns = 6;
    uint32_t rows = 5;
    uint32_t waterColumns = 1;
};

struct NullHold : public CoW::WorldStep::IGenerationHold
{
    void Hold() override {}
    void Release() override {}
};

struct Published
{
    uint64_t week;
    Season season;
};

// One weather stage with its participant (no module, no SaveManager): the pending blob goes through nested contexts.
struct PendWorld
{
    PendWorld() : participant(stage)
    {
        EXPECT_TRUE(PublishSidecar(layers, ReadShippedSidecar()));
        Draoi::HexWorld::SaveError err;
        EXPECT_TRUE(participant.ConfigureSchema(layers, err)) << err.field.AsCStr();
        stage.SetWorldSource(&source);
        stage.SetRegistry(&Draoi::HexWorld::Weather::Testing::TestRegistry());
        stage.SetParameters(Draoi::HexWorld::Weather::Testing::MakeTestParams());
        stage.SetSpinupWeeks(4);
        stage.SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
        stage.SetGenerationHold(&hold);
        stage.SetPublishSeam([this](uint64_t week, Season season) { published.push_back({ week, season }); });
        stage.Start();
        stage.NotifySubscribed();
    }
    ~PendWorld() { stage.Stop(); }

    void Announce(uint32_t revision)
    {
        stage.OnWorldComplete(revision);
        stage.Update();
    }

    void CommitWeeks(uint32_t firstWeek, uint32_t count)
    {
        Dia::ParallelExecutor::SerialExecutor executor;
        for (uint32_t i = 0; i < count; ++i)
        {
            stage.Capture(static_cast<Dia::DeferredStep::StepKey>((firstWeek + i) * 7));
            stage.Run(executor);
            stage.Commit();
        }
    }

    // Capture and Run, no Commit: a step result in flight.
    void RunInFlight(uint32_t day)
    {
        Dia::ParallelExecutor::SerialExecutor executor;
        stage.Capture(static_cast<Dia::DeferredStep::StepKey>(day));
        stage.Run(executor);
    }

    static Json::Value Reparse(const Json::Value& value)
    {
        const std::string text = Draoi::HexWorld::Serialize(value);
        Json::Value parsed;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, parsed));
        return parsed;
    }

    // The committed record, through real text.
    Json::Value CommittedRecord()
    {
        Dia::SaveGame::SaveContext ctx;
        participant.Serialize(ctx);
        return Reparse(ctx.Root());
    }

    // The whole pending save as the coordinator writes it: { stage_0 : blob }. Real text.
    Json::Value PendingSave()
    {
        Dia::SaveGame::SaveContext ctx;
        ctx.BeginObject(Dia::Core::StringCRC("stage_0"));
        stage.SerializePending(ctx);
        ctx.EndObject();
        return Reparse(ctx.Root());
    }

    bool ValidatePending(const Json::Value& save)
    {
        Dia::SaveGame::LoadContext ctx(save);
        if (!ctx.BeginObject(Dia::Core::StringCRC("stage_0"))) { return false; }
        const bool ok = stage.ValidatePending(ctx);
        ctx.EndObject();
        return ok;
    }

    void DeserializePending(const Json::Value& save)
    {
        Dia::SaveGame::LoadContext ctx(save);
        ASSERT_TRUE(ctx.BeginObject(Dia::Core::StringCRC("stage_0")));
        stage.DeserializePending(ctx);
        ctx.EndObject();
    }

    // The committed record's Validate + Deserialize, as SaveManager runs them before the pending blob.
    bool LoadCommitted(const Json::Value& record)
    {
        Dia::SaveGame::LoadContext validateCtx(record);
        if (!participant.Validate(validateCtx)) { return false; }
        Dia::SaveGame::LoadContext ctx(record);
        participant.Deserialize(ctx);
        return true;
    }

    OneWorldSource source;
    NullHold hold;
    Dia::ValueSchema::ValueSchemaRegistry layers;
    CoW::Weather::WeatherStage stage;
    CoW::Weather::WeatherSaveParticipant participant;
    std::vector<Published> published;
};

const float* FieldPtr(const WeatherFields& fields, uint32_t index)
{
    const float* ptrs[kWeatherSaveFieldCount];
    CoW::Weather::WeatherFieldPointers(fields, ptrs);
    return ptrs[index];
}

std::vector<uint32_t> Bits(const WeatherFields& fields)
{
    std::vector<uint32_t> bits;
    for (uint32_t f = 0; f < kWeatherSaveFieldCount; ++f)
    {
        for (uint32_t c = 0; c < fields.CellCount(); ++c)
        {
            uint32_t u;
            std::memcpy(&u, &FieldPtr(fields, f)[c], sizeof(u));
            bits.push_back(u);
        }
    }
    return bits;
}

const WeatherFields& Committed(PendWorld& w) { return w.stage.ReadableSim()->Fields(); }
const WeatherFields& Working(PendWorld& w) { return w.stage.ReadableSim()->GetWorkingFields(); }

// Test-only: poke a value into the in-flight working set.
float& WorkingCell(PendWorld& w, uint32_t field, uint32_t cell)
{
    return const_cast<float*>(FieldPtr(Working(w), field))[cell];
}

constexpr uint32_t kPressureAnomaly = 9;
}

// ---- AC 16 ----------------------------------------------------------------------------------------------------------

TEST(WeatherPendingSave, WorkingSetIsWrittenAfterTheStepHasRunAndTheCommittedSetIsUnchanged)
{
    PendWorld w;
    w.Announce(5);
    w.CommitWeeks(1, 2);
    const std::vector<uint32_t> committedBefore = Bits(Committed(w));
    w.RunInFlight(3 * 7);
    ASSERT_NE(committedBefore, Bits(Working(w)));   // the step has run: the working set moved

    const Json::Value save = w.PendingSave();
    ASSERT_TRUE(save.isMember("stage_0"));
    EXPECT_FALSE(save.isMember("pending_version"));   // nothing leaks into the enclosing object
    const Json::Value& blob = save["stage_0"];
    ASSERT_TRUE(blob["valid"].asBool());
    ASSERT_EQ(30u, blob["cell_count"].asUInt());
    for (uint32_t f = 0; f < kWeatherSaveFieldCount; ++f)
    {
        SCOPED_TRACE(kFieldNames[f]);
        ASSERT_TRUE(blob[kFieldNames[f]].isArray());
        ASSERT_EQ(30u, blob[kFieldNames[f]].size());
        for (uint32_t c = 0; c < 30; ++c)
        {
            EXPECT_EQ(FieldPtr(Working(w), f)[c], static_cast<float>(blob[kFieldNames[f]][c].asDouble()));
        }
    }
    EXPECT_FALSE(blob.isMember("biome"));
    EXPECT_FALSE(blob.isMember("biome_palette"));
    EXPECT_EQ(committedBefore, Bits(Committed(w)));
}

// ---- AC 17 ----------------------------------------------------------------------------------------------------------

TEST(WeatherPendingSave, AMalformedBlobFailsValidatePendingAndChangesNothing)
{
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(3 * 7);
    const Json::Value good = a.PendingSave();
    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(good));

    std::vector<std::pair<std::string, Json::Value>> defects;
    {
        Json::Value bad = good;
        bad["stage_0"]["cell_count"] = 31;
        defects.emplace_back("wrong cell_count", bad);
    }
    {
        Json::Value bad = good;
        bad["stage_0"]["temperature"][0u] = std::numeric_limits<double>::quiet_NaN();
        defects.emplace_back("non-finite value", bad);
    }
    {
        Json::Value bad = good;
        bad["stage_0"]["moisture"][0u] = 1.0e9;
        defects.emplace_back("out-of-range value", bad);
    }
    {
        Json::Value bad = good;
        bad["stage_0"].removeMember("snow_pack");
        defects.emplace_back("missing array", bad);
    }
    {
        Json::Value bad = good;
        bad["stage_0"]["biome_palette"] = Json::Value(Json::arrayValue);
        defects.emplace_back("repeated palette", bad);
    }

    const std::vector<uint32_t> before = Bits(Committed(b));
    for (const auto& defect : defects)
    {
        SCOPED_TRACE(defect.first);
        EXPECT_FALSE(b.ValidatePending(defect.second));
        EXPECT_EQ(before, Bits(Committed(b)));
        EXPECT_TRUE(b.stage.IsReady());
    }
}

// ---- AC 19 ----------------------------------------------------------------------------------------------------------

TEST(WeatherPendingSave, AStepWhoseRevisionNoLongerMatchesWritesValidFalseAndTheLaterCommitIsANoOp)
{
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(3 * 7);
    a.stage.OnWorldComplete(6);   // the world changed under the in-flight step
    const Json::Value save = a.PendingSave();
    const Json::Value& blob = save["stage_0"];
    ASSERT_TRUE(blob.isMember("pending_version"));
    EXPECT_EQ(1u, blob["pending_version"].asUInt());
    ASSERT_TRUE(blob["valid"].isBool());
    EXPECT_FALSE(blob["valid"].asBool());
    EXPECT_FALSE(blob.isMember("moisture"));

    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    EXPECT_TRUE(b.ValidatePending(save));
    b.DeserializePending(save);
    const std::vector<uint32_t> before = Bits(Committed(b));
    b.stage.Commit();   // the later Apply: a logged no-op
    EXPECT_TRUE(b.published.empty());
    EXPECT_EQ(before, Bits(Committed(b)));
}

// ---- AC 19a ---------------------------------------------------------------------------------------------------------

TEST(WeatherPendingSave, StepDayIsWrittenAndAValidBlobWithoutAUsableStepDayFails)
{
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    const uint32_t day = 20 * 7 + 3;
    a.RunInFlight(day);
    const Json::Value good = a.PendingSave();
    EXPECT_EQ(day, good["stage_0"]["step_day"].asUInt());

    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(good));

    Json::Value missing = good;
    missing["stage_0"].removeMember("step_day");
    EXPECT_FALSE(b.ValidatePending(missing));
    Json::Value negative = good;
    negative["stage_0"]["step_day"] = -1;
    EXPECT_FALSE(b.ValidatePending(negative));
    Json::Value fractional = good;
    fractional["stage_0"]["step_day"] = 1.5;
    EXPECT_FALSE(b.ValidatePending(fractional));
    Json::Value text = good;
    text["stage_0"]["step_day"] = "28";
    EXPECT_FALSE(b.ValidatePending(text));
}

TEST(WeatherPendingSave, AnAppliedRestoredStepPublishesTheWeekAndSeasonOfItsOwnStepDayAndEqualsTheUninterruptedRun)
{
    const uint32_t day = 20 * 7 + 3;   // week 20, well away from the restored committed week 2
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(day);
    const Json::Value save = a.PendingSave();
    a.stage.Commit();   // the uninterrupted run
    ASSERT_EQ(3u, a.published.size());

    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(save));
    b.DeserializePending(save);
    ASSERT_TRUE(b.published.empty());
    b.stage.Commit();
    ASSERT_EQ(1u, b.published.size());
    EXPECT_EQ(CoW::WeekOf(day), b.published[0].week);
    EXPECT_EQ(static_cast<Season>(CoW::SeasonOf(day) % CoW::kSeasonsPerYear), b.published[0].season);
    EXPECT_EQ(a.published[2].week, b.published[0].week);
    EXPECT_EQ(Bits(Committed(a)), Bits(Committed(b)));
    EXPECT_EQ(CoW::WeekOf(day), b.stage.ReadableSim()->GetCommittedWeek());
}

// ---- Real nesting: the blob is read and written inside stage_N ------------------------------------------------------

TEST(WeatherPendingSave, TheBlobIsWrittenIntoTheNestedStageObjectNotTheTopLevel)
{
    PendWorld a;
    a.Announce(5);
    a.RunInFlight(7);
    Dia::SaveGame::SaveContext ctx;
    ctx.Write(Dia::Core::StringCRC("marker"), static_cast<int32_t>(1));
    ctx.BeginObject(Dia::Core::StringCRC("stage_0"));
    a.stage.SerializePending(ctx);
    ctx.EndObject();
    EXPECT_TRUE(ctx.Root().isMember("marker"));
    EXPECT_FALSE(ctx.Root().isMember("pending_version"));
    EXPECT_FALSE(ctx.Root().isMember("valid"));
    EXPECT_TRUE(ctx.Root()["stage_0"].isMember("pending_version"));
}

// ---- AC 21 for the pending blob -------------------------------------------------------------------------------------

TEST(WeatherPendingSave, PressureAnomalyAtPlusAndMinusHalfRoundTripsAndNegativeZeroIsWrittenAsPositiveZero)
{
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(3 * 7);
    WorkingCell(a, kPressureAnomaly, 0) = 0.5f;
    WorkingCell(a, kPressureAnomaly, 1) = -0.5f;
    WorkingCell(a, kPressureAnomaly, 2) = -0.0f;
    const Json::Value save = a.PendingSave();
    EXPECT_EQ(0.5, save["stage_0"]["pressure_anomaly"][0u].asDouble());
    EXPECT_EQ(-0.5, save["stage_0"]["pressure_anomaly"][1u].asDouble());
    const float zero = static_cast<float>(save["stage_0"]["pressure_anomaly"][2u].asDouble());
    uint32_t zeroBits;
    std::memcpy(&zeroBits, &zero, sizeof(zeroBits));
    EXPECT_EQ(0u, zeroBits);
    a.stage.Commit();

    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(save));
    b.DeserializePending(save);
    b.stage.Commit();
    EXPECT_EQ(0.5f, Committed(b).pressureAnomaly[0]);
    EXPECT_EQ(-0.5f, Committed(b).pressureAnomaly[1]);
    EXPECT_EQ(0.0f, Committed(b).pressureAnomaly[2]);
    EXPECT_FALSE(std::signbit(Committed(b).pressureAnomaly[2]));
}

TEST(WeatherPendingSave, PressureAnomalyMissingOrOutOfRangeIsRejected)
{
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(3 * 7);
    const Json::Value good = a.PendingSave();
    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(good));

    Json::Value missing = good;
    missing["stage_0"].removeMember("pressure_anomaly");
    EXPECT_FALSE(b.ValidatePending(missing));
    Json::Value high = good;
    high["stage_0"]["pressure_anomaly"][0u] = 0.51;
    EXPECT_FALSE(b.ValidatePending(high));
    Json::Value low = good;
    low["stage_0"]["pressure_anomaly"][0u] = -0.51;
    EXPECT_FALSE(b.ValidatePending(low));
    Json::Value edge = good;
    edge["stage_0"]["pressure_anomaly"][0u] = 0.5;
    edge["stage_0"]["pressure_anomaly"][1u] = -0.5;
    EXPECT_TRUE(b.ValidatePending(edge));
}

// ---- Task 14: DFS-005 for weather (AC 15 stage half, AC 15 coordinator half, AC 18) -----------------------------------

namespace
{
struct Checkpoint
{
    std::vector<uint32_t> bits;
    Json::Value record;
    bool operator==(const Checkpoint& o) const { return bits == o.bits && record == o.record; }
};

Checkpoint Take(PendWorld& w) { return { Bits(Committed(w)), w.CommittedRecord() }; }

// Weather stage + fake neighbour behind the real WorldStepModule, the real SaveManager and the real
// DeferredStepSaveParticipant. The weather participant registers first (the contract: stage state before the channel).
struct CoordWorld
{
    explicit CoordWorld(const std::string& slot)
        : slotId(slot.c_str())
        , saveConfig{ "temp/worldstep_save_tests/", "slot_{id}.sav", 0, Dia::SaveGame::SaveFormat::Json }
        , harness(WsTest::kDefaultConfig)
        , scheduleParticipant(harness.scheduler)
    {
        _mkdir("temp");
        _mkdir("temp/worldstep_save_tests");
        EXPECT_TRUE(harness.module.RegisterStage(Dia::Core::StringCRC("weather"), world.stage));
        EXPECT_TRUE(harness.module.RegisterStage(Dia::Core::StringCRC("ecology"), ecology));
        manager.Init(saveConfig, registry);
        harness.module.ObserveLoads(manager);
        registry.Register(Dia::Core::StringCRC("CowWeather"), &world.participant);
        scheduleParticipant.Attach(registry, manager);
    }
    ~CoordWorld()
    {
        harness.module.StopObservingLoads();
        registry.Unregister(Dia::Core::StringCRC("CowWeather"));
        harness.module.UnregisterStage(Dia::Core::StringCRC("weather"));
        harness.module.UnregisterStage(Dia::Core::StringCRC("ecology"));
    }

    void Days(uint64_t first, uint64_t last)
    {
        for (uint64_t d = first; d <= last; ++d) { harness.StepDay(d); }
    }

    PendWorld world;
    Dia::DeferredStep::Testing::CallLog log;
    Dia::DeferredStep::Testing::FakeStepStage ecology{ Dia::Core::StringCRC("ecology"), log };
    Dia::Core::StringCRC slotId;
    Dia::SaveGame::SaveConfig saveConfig;
    Dia::SaveGame::SaveRegistry registry;
    Dia::SaveGame::SaveManager manager;
    WsTest::Harness harness;
    Dia::DeferredStep::DeferredStepSaveParticipant scheduleParticipant;
};
}

TEST(WeatherPendingSaveDfs005, StageLevelSaveAfterRunAndBeforeCommitAppliesAtTheOriginalKeyBitIdentically)
{
    const uint32_t day = 3 * 7;
    PendWorld a;
    a.Announce(5);
    a.CommitWeeks(1, 2);
    const Json::Value committedRecord = a.CommittedRecord();
    a.RunInFlight(day);
    const Json::Value save = a.PendingSave();
    ASSERT_TRUE(save["stage_0"]["valid"].asBool());
    a.stage.Commit();
    std::vector<Checkpoint> ref;
    ref.push_back(Take(a));          // after the apply
    a.CommitWeeks(4, 1);
    ref.push_back(Take(a));          // N = 1
    a.CommitWeeks(5, 12);
    ref.push_back(Take(a));          // N = 13

    PendWorld b;
    b.Announce(5);
    ASSERT_TRUE(b.LoadCommitted(committedRecord));
    ASSERT_TRUE(b.ValidatePending(save));
    b.DeserializePending(save);
    ASSERT_TRUE(b.stage.IsReady());
    b.stage.Commit();
    EXPECT_TRUE(ref[0] == Take(b)) << "after the apply";
    b.CommitWeeks(4, 1);
    EXPECT_TRUE(ref[1] == Take(b)) << "one week later";
    b.CommitWeeks(5, 12);
    EXPECT_TRUE(ref[2] == Take(b)) << "thirteen weeks later";
}

TEST(WeatherPendingSaveDfs005, CoordinatorMidStepSaveThroughTheRealSaveManagerLoadsAndAppliesBitIdentically)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    std::map<uint64_t, Checkpoint> ref;
    {
        CoordWorld r(slot + "r");
        r.world.Announce(5);
        for (uint64_t d = 1; d <= 112; ++d)
        {
            r.harness.StepDay(d);
            if (d == 21 || d == 28 || d == 112) { ref[d] = Take(r.world); }
        }
        ASSERT_GE(r.world.published.size(), 10u);
    }
    {
        CoordWorld a(slot);
        a.world.Announce(5);
        a.Days(1, 17);   // week 14 is complete and applies at 21
        ASSERT_EQ(Dia::DeferredStep::ChainPhase::Ran, a.harness.module.GetChain().Phase());
        ASSERT_TRUE(a.manager.Save(a.slotId).Ok());
    }
    CoordWorld b(slot);
    b.world.Announce(5);
    ASSERT_TRUE(b.manager.Load(b.slotId).Ok());
    EXPECT_EQ(Dia::DeferredStep::ChainPhase::Ran, b.harness.module.GetChain().Phase());
    EXPECT_TRUE(b.world.stage.IsReady());
    EXPECT_TRUE(b.world.published.empty());   // loading raises nothing
    for (uint64_t d = 18; d <= 112; ++d)
    {
        b.harness.StepDay(d);
        if (d == 21 || d == 28 || d == 112)
        {
            SCOPED_TRACE(d);
            EXPECT_TRUE(ref[d] == Take(b.world));
        }
    }
}

TEST(WeatherPendingSaveDfs005, WithNoActiveSimTheRestoredPendingBlobIsNotReadyWithAnErrorAndTheLaterApplyIsANoOp)
{
    for (int cause = 0; cause < 2; ++cause)   // 0 = dimension mismatch, 1 = no water source
    {
        SCOPED_TRACE(cause == 0 ? "dimension mismatch" : "no water source");
        PendWorld a;
        a.Announce(5);
        a.CommitWeeks(1, 2);
        const Json::Value committedRecord = a.CommittedRecord();
        a.RunInFlight(3 * 7);
        const Json::Value save = a.PendingSave();
        ASSERT_TRUE(save["stage_0"]["valid"].asBool());

        PendWorld b;
        b.stage.SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kManual);   // the re-spin-up has not finished
        if (cause == 0) { b.source.columns = 7; } else { b.source.waterColumns = 0; }
        b.Announce(5);
        {   // SaveManager order: every Validate first, then the Deserializes
            Dia::SaveGame::LoadContext validateCtx(committedRecord);
            ASSERT_TRUE(b.participant.Validate(validateCtx));
            ASSERT_TRUE(b.ValidatePending(save));
            Dia::SaveGame::LoadContext ctx(committedRecord);
            b.participant.Deserialize(ctx);   // the world does not fit: the restore is discarded
        }
        Dia::DeferredStep::Testing::LogCapture logs("WTH");
        b.DeserializePending(save);
        EXPECT_FALSE(b.stage.IsReady());
        EXPECT_GE(logs.Count(Dia::DeferredStep::Testing::LogCapture::Level::kError), 1u);

        logs.Clear();
        b.stage.Commit();   // the later Apply
        EXPECT_TRUE(b.published.empty());
        EXPECT_EQ(1u, logs.Infos("step discarded"));
        EXPECT_FALSE(b.stage.IsReady());

        if (cause == 0)   // the next kick works once the world is spun up again
        {
            ASSERT_TRUE(b.stage.RunNextJob());
            b.stage.Update();
            ASSERT_TRUE(b.stage.IsReady());
            b.CommitWeeks(1, 1);
            EXPECT_EQ(1u, b.published.size());
        }
    }
}

TEST(WeatherPendingSaveDfs005, CoordinatorDiscardPathLogsAnErrorTheChannelAppliesAsANoOpAndTheNextKickWorks)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    {
        CoordWorld a(slot);
        a.world.Announce(5);
        a.Days(1, 17);
        ASSERT_TRUE(a.manager.Save(a.slotId).Ok());
    }
    CoordWorld b(slot);
    b.world.stage.SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kManual);
    b.world.source.columns = 7;
    b.world.Announce(5);
    Dia::DeferredStep::Testing::LogCapture moduleLogs("WST");
    ASSERT_TRUE(b.manager.Load(b.slotId).Ok());
    EXPECT_FALSE(b.world.stage.IsReady());
    EXPECT_GE(moduleLogs.Count(Dia::DeferredStep::Testing::LogCapture::Level::kError, "weather"), 1u);

    b.Days(18, 22);   // the restored channel reaches its apply key: a logged no-op
    EXPECT_TRUE(b.world.published.empty());
    EXPECT_EQ(0u, b.harness.module.ApplyCount());

    ASSERT_TRUE(b.world.stage.RunNextJob());
    b.world.stage.Update();
    ASSERT_TRUE(b.world.stage.IsReady());
    b.Days(23, 45);
    EXPECT_GE(b.harness.module.KickCount(), 1u);
    EXPECT_GE(b.harness.module.ApplyCount(), 1u);
    EXPECT_GE(b.world.published.size(), 1u);
}
