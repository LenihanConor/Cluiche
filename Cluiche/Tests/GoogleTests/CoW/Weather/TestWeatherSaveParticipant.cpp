// A4 weather-save Task 9: the cow.weather save participant against a real SaveManager on a temp slot.
// AC 4 (ready:false), AC 7 (Validate mutates nothing), AC 8 (a failed Validate aborts the load), version 1, happy path.
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <Modules/Weather/WeatherModule.h>   // first: pulls WorldGenerationModule.h before windows.h
#include "../WorldStep/WorldStepSaveRig.h"
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaMessageBus/Bus.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Modules/Weather/WeatherMessages.h>
#include <Modules/Weather/WeatherSaveParticipant.h>
#include <Modules/WorldGen/WorldGenerationMessages.h>
#include <Shared/Weather/WeatherSaveFields.h>
#include "WeatherParamTestKit.h"
#include "WeatherTestWorld.h"

namespace
{
using Dia::Core::StringCRC;
using CoW::Weather::kWeatherSaveFieldCount;

const char* kLayersSidecarPath = "Cluiche/Assets/CoW/Global/Weather/weather_layers.schema.json";

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

class WeatherProbe : public CoW::WeatherModule
{
public:
    explicit WeatherProbe(const StringCRC& id) : CoW::WeatherModule(id) {}
    Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
    Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
};

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

// Everything one live weather world needs, with a real SaveManager whose registry holds cow.weather.
struct SaveWorld
{
    explicit SaveWorld(const std::string& slotName)
        : harness(WsTest::Config("weather", 7, true))
        , module(StringCRC("WeatherModule"))
        , slotId(slotName.c_str())
        , saveConfig{ "temp/worldstep_save_tests/", "slot_{id}.sav", 0, Dia::SaveGame::SaveFormat::Json }
    {
        _mkdir("temp");
        _mkdir("temp/worldstep_save_tests");
        bus.Initialize();
        bus.RegisterType<CoW::WorldGenerationComplete, 4>();
        bus.RegisterType<CoW::WorldGenerationFailed, 4>();
        bus.RegisterType<CoW::WeatherUpdated, 4>();
        CoW::WeatherModule::TestHosts hosts;
        hosts.worldStep = &harness.module;
        hosts.bus = &bus;
        hosts.worldSource = &source;
        hosts.generationHold = &hold;
        module.UseTestHosts(hosts);
        module.GetStage().SetRegistry(&Draoi::HexWorld::Weather::Testing::TestRegistry());
        module.GetStage().SetParameters(Draoi::HexWorld::Weather::Testing::MakeTestParams());
        module.GetStage().SetSpinupWeeks(4);
        module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);

        EXPECT_TRUE(PublishSidecar(layers, ReadShippedSidecar()));
        Draoi::HexWorld::SaveError err;
        EXPECT_TRUE(module.GetSaveParticipant().ConfigureSchema(layers, err)) << err.field.AsCStr();

        manager.Init(saveConfig, registry);
        registry.Register(StringCRC("cow.weather"), &module.GetSaveParticipant());
        EXPECT_EQ(Dia::ApplicationFlow::StartResult::kReady, module.CallStart());
    }
    ~SaveWorld()
    {
        registry.Unregister(StringCRC("cow.weather"));
        module.CallStop();
    }

    void Announce(uint32_t revision)
    {
        CoW::WorldGenerationComplete msg;
        msg.worldRevision = revision;
        ASSERT_TRUE(bus.Broadcast(msg));
        bus.Update();
        module.GetStage().Update();
    }

    // One weekly step per call at the given week index (day = week * 7).
    void StepWeeks(uint32_t firstWeek, uint32_t count)
    {
        Dia::ParallelExecutor::SerialExecutor executor;
        for (uint32_t i = 0; i < count; ++i)
        {
            module.GetStage().Capture(static_cast<Dia::DeferredStep::StepKey>((firstWeek + i) * 7));
            module.GetStage().Run(executor);
            module.GetStage().Commit();
        }
    }

    Dia::SaveGame::SaveResult Save() { return manager.Save(slotId); }
    Dia::SaveGame::LoadResult Load() { return manager.Load(slotId); }

    // The participant's own record, as a parsed value (through real JSON text).
    Json::Value Record()
    {
        Dia::SaveGame::SaveContext ctx;
        module.GetSaveParticipant().Serialize(ctx);
        const std::string text = Draoi::HexWorld::Serialize(ctx.Root());
        Json::Value parsed;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, parsed));
        return parsed;
    }

    WsTest::Harness harness;
    WeatherProbe module;
    Dia::MessageBus::Bus bus;
    OneWorldSource source;
    NullHold hold;
    Dia::ValueSchema::ValueSchemaRegistry layers;
    StringCRC slotId;
    Dia::SaveGame::SaveConfig saveConfig;
    Dia::SaveGame::SaveRegistry registry;
    Dia::SaveGame::SaveManager manager;
};

// A value snapshot of everything a failed load must leave alone.
struct Snap
{
    bool hasSim = false;
    bool ready = false;
    bool stashed = false;
    uint32_t cells = 0;
    uint64_t week = 0;
    uint32_t queue = 0;
    std::vector<uint32_t> bits;
    std::vector<uint32_t> biomes;
    bool operator==(const Snap& o) const
    {
        return hasSim == o.hasSim && ready == o.ready && stashed == o.stashed && cells == o.cells && week == o.week
            && queue == o.queue && bits == o.bits && biomes == o.biomes;
    }
};

Snap Take(CoW::Weather::WeatherStage& stage)
{
    Snap s;
    s.ready = stage.IsReady();
    s.stashed = stage.HasStashedRecord();
    s.queue = stage.QueueCount();
    const Draoi::HexWorld::Weather::WeatherSimulation* sim = stage.ReadableSim();
    if (sim == nullptr) { return s; }
    s.hasSim = true;
    s.week = sim->GetCommittedWeek();
    s.cells = sim->Fields().CellCount();
    const float* ptrs[kWeatherSaveFieldCount];
    CoW::Weather::WeatherFieldPointers(sim->Fields(), ptrs);
    for (uint32_t f = 0; f < kWeatherSaveFieldCount; ++f)
    {
        for (uint32_t c = 0; c < s.cells; ++c)
        {
            uint32_t u;
            std::memcpy(&u, &ptrs[f][c], sizeof(u));
            s.bits.push_back(u);
        }
    }
    for (uint32_t c = 0; c < sim->Terrain().biome.Size(); ++c) { s.biomes.push_back(static_cast<uint32_t>(sim->Terrain().biome[c])); }
    return s;
}

std::string SlotPath(const std::string& slot) { return WsTest::TempDir() + "slot_" + slot + ".sav"; }
}

TEST(WeatherSaveParticipant, IdIsCowWeatherAndVersionIsOne)
{
    EXPECT_STREQ("cow.weather", CoW::World::SaveId(CoW::World::kWeather).c_str());
    CoW::Weather::WeatherStage stage;
    CoW::Weather::WeatherSaveParticipant participant(stage);
    EXPECT_EQ(1u, participant.GetVersion());
}

TEST(WeatherSaveParticipant, SaveThenLoadIntoAFreshModuleGivesEqualCommittedFields)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    Snap saved;
    {
        SaveWorld a(slot);
        a.Announce(5);
        a.StepWeeks(1, 3);
        saved = Take(a.module.GetStage());
        ASSERT_EQ(3u, saved.week);
        ASSERT_TRUE(saved.ready);
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveWorld b(slot);
    b.Announce(5);
    ASSERT_NE(saved, Take(b.module.GetStage()));   // the spun-up world has not been stepped
    const Dia::SaveGame::LoadResult result = b.Load();
    ASSERT_TRUE(result.Ok()) << static_cast<int>(result.code);
    EXPECT_EQ(saved, Take(b.module.GetStage()));
}

TEST(WeatherSaveParticipant, WithNoActiveSimSerializeWritesReadyFalseOnly)
{
    SaveWorld a(WsTest::UniqueSlot());
    const Json::Value record = a.Record();
    ASSERT_TRUE(record.isObject());
    EXPECT_EQ(1u, record.size());
    ASSERT_TRUE(record["ready"].isBool());
    EXPECT_FALSE(record["ready"].asBool());
}

TEST(WeatherSaveParticipant, ReadyFalseRecordLoadsAsANoOpAndTheNextCompletionSpinsUpNormally)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    {
        SaveWorld a(slot);
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveWorld b(slot);
    const Dia::SaveGame::LoadResult result = b.Load();
    ASSERT_TRUE(result.Ok()) << static_cast<int>(result.code);
    EXPECT_FALSE(b.module.GetStage().IsReady());
    EXPECT_EQ(nullptr, b.module.GetStage().ReadableSim());
    EXPECT_FALSE(b.module.GetStage().HasStashedRecord());
    b.Announce(5);
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
    EXPECT_EQ(30u, b.module.GetStage().GetSnapshot().cellCount);
}

TEST(WeatherSaveParticipant, ReadyFalseRecordLoadedOverALiveSimLeavesTheStageReady)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    {
        SaveWorld a(slot);
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveWorld b(slot);
    b.Announce(5);
    ASSERT_TRUE(b.Load().Ok());
    b.module.GetStage().Update();
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
}

TEST(WeatherSaveParticipant, ValidateRejectsEveryDefectAndMutatesNothing)
{
    SaveWorld w(WsTest::UniqueSlot());
    w.Announce(5);
    w.StepWeeks(1, 2);
    ASSERT_TRUE(w.module.QueueClearWindOverrides());
    ASSERT_TRUE(w.module.QueueClearWindOverrides());
    const Snap before = Take(w.module.GetStage());
    ASSERT_EQ(2u, before.queue);
    ASSERT_EQ(2u, before.week);

    const Json::Value good = w.Record();
    {
        Dia::SaveGame::LoadContext ctx(good);
        EXPECT_TRUE(w.module.GetSaveParticipant().Validate(ctx)) << "control: the unmodified record must validate";
        EXPECT_EQ(before, Take(w.module.GetStage()));
    }

    std::vector<std::pair<std::string, Json::Value>> defects;
    // every top-level key missing, one at a time
    const Json::Value::Members names = good.getMemberNames();
    ASSERT_GT(names.size(), 10u);
    for (const std::string& name : names)
    {
        if (name == "biome_palette_hash") { continue; }   // optional: absent means no drift check
        Json::Value bad = good;
        bad.removeMember(name);
        defects.emplace_back("missing " + name, bad);
    }
    {
        Json::Value bad = good;
        bad["moisture"][0u] = 1.0e9;
        defects.emplace_back("moisture out of range", bad);
    }
    {
        Json::Value bad = good;
        Json::Value shorter(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i + 1 < bad["pressure"].size(); ++i) { shorter.append(bad["pressure"][i]); }
        bad["pressure"] = shorter;
        defects.emplace_back("pressure wrong length", bad);
    }
    {
        Json::Value bad = good;
        bad["wind_speed"][0u] = "not a number";
        defects.emplace_back("wind_speed not a number", bad);
    }
    {
        Json::Value bad = good;
        bad["biome"][0u] = 9999;
        defects.emplace_back("biome index past the palette", bad);
    }
    {
        Json::Value bad = good;
        bad["cell_count"] = 31;
        defects.emplace_back("cell_count disagrees with the arrays", bad);
    }
    defects.emplace_back("not an object", Json::Value(Json::arrayValue));
    {
        Json::Value bad = good;
        bad["ready"] = "yes";
        defects.emplace_back("ready not a bool", bad);
    }

    for (const auto& defect : defects)
    {
        SCOPED_TRACE(defect.first);
        Dia::SaveGame::LoadContext ctx(defect.second);
        EXPECT_FALSE(w.module.GetSaveParticipant().Validate(ctx));
        EXPECT_EQ(before, Take(w.module.GetStage()));
    }
}

TEST(WeatherSaveParticipant, AFailedValidateAbortsTheRealLoadAndLiveStateIsUntouched)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    {
        SaveWorld a(slot);
        a.Announce(5);
        a.StepWeeks(1, 3);
        ASSERT_TRUE(a.Save().Ok());
    }
    // Corrupt the participant's block in the slot file: one value out of its sidecar range.
    {
        std::ifstream in(SlotPath(slot), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        in.close();
        Json::Value root;
        Json::Reader reader;
        ASSERT_TRUE(reader.parse(ss.str(), root));
        ASSERT_TRUE(root.isMember("cow.weather"));
        root["cow.weather"]["temperature"][0u] = 1.0e9;
        Json::FastWriter writer;
        std::ofstream out(SlotPath(slot), std::ios::binary | std::ios::trunc);
        out << writer.write(root);
    }
    SaveWorld b(slot);
    b.Announce(5);
    b.StepWeeks(1, 1);
    const Snap before = Take(b.module.GetStage());
    ASSERT_EQ(1u, before.week);
    const Dia::SaveGame::LoadResult result = b.Load();
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(Dia::SaveGame::LoadResultCode::DeserializeError, result.code);
    EXPECT_EQ(before, Take(b.module.GetStage()));
}

// ---- Task 10: A4's restore outcomes (AC 5, 6, 9, 10, 12, 14) --------------------------------------------------------

namespace
{
typedef Dia::CellLayers::Testing::LogCapture RestoreLog;

// Saves a world stepped to week 3 (revision 5) and returns what it held.
Snap SaveSteppedWorld(const std::string& slot)
{
    SaveWorld a(slot);
    a.Announce(5);
    a.StepWeeks(1, 3);
    const Snap saved = Take(a.module.GetStage());
    EXPECT_EQ(3u, saved.week);
    EXPECT_TRUE(a.Save().Ok());
    return saved;
}
}

TEST(WeatherSaveParticipant, RestoredWorldStepsBitIdenticalToAnUninterruptedRun)
{
    const uint32_t kExtraWeeks[] = { 1, 13, 52 };
    for (uint32_t extra : kExtraWeeks)
    {
        SCOPED_TRACE(extra);
        const std::string slot = WsTest::UniqueSlot();
        WsTest::SlotFile file(slot);
        SaveSteppedWorld(slot);

        SaveWorld control(WsTest::UniqueSlot());
        control.Announce(5);
        control.StepWeeks(1, 3);
        control.StepWeeks(4, extra);
        const Snap expected = Take(control.module.GetStage());
        ASSERT_EQ(3u + extra, expected.week);

        SaveWorld b(slot);
        b.Announce(5);
        ASSERT_TRUE(b.Load().Ok());
        b.StepWeeks(4, extra);
        EXPECT_EQ(expected, Take(b.module.GetStage()));
    }
}

TEST(WeatherSaveParticipant, SuccessfulRestoreStartsNoSpinUpAndIsReadyAtOnce)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const Snap saved = SaveSteppedWorld(slot);

    SaveWorld b(slot);
    uint32_t spinWeeks = 0;
    b.module.GetStage().SetSpinUpWeekObserver([&spinWeeks](uint32_t) { ++spinWeeks; });
    b.module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kManual);
    b.Announce(5);
    ASSERT_EQ(1u, b.module.GetStage().QueuedJobCount());
    ASSERT_TRUE(b.module.GetStage().RunNextJob());
    b.module.GetStage().Update();
    ASSERT_TRUE(b.module.GetStage().IsReadyForRevision(5));
    const uint32_t weeksBefore = spinWeeks;
    ASSERT_GT(weeksBefore, 0u);

    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(0u, b.module.GetStage().QueuedJobCount());
    EXPECT_FALSE(b.module.GetStage().SpinUpRunning());
    EXPECT_EQ(weeksBefore, spinWeeks) << "the restore must not run a spin-up";
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));   // no Update needed
    EXPECT_EQ(saved, Take(b.module.GetStage()));
}

TEST(WeatherSaveParticipant, DimensionMismatchDiscardsTheSavedStateWithOneWarningAndSpinsUp)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const Snap saved = SaveSteppedWorld(slot);

    SaveWorld b(slot);
    b.source.columns = 7;
    b.Announce(5);
    RestoreLog log("WTH");
    const Dia::SaveGame::LoadResult result = b.Load();
    EXPECT_TRUE(result.Ok()) << static_cast<int>(result.code);
    EXPECT_EQ(1u, log.Warnings());
    EXPECT_EQ(1u, log.Warnings("6x5"));
    EXPECT_EQ(1u, log.Warnings("7x5"));
    b.module.GetStage().Update();
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
    const Snap now = Take(b.module.GetStage());
    EXPECT_EQ(35u, now.cells);
    EXPECT_NE(saved.cells, now.cells);
}

TEST(WeatherSaveParticipant, SaveRestoredIntoAWorldWithNoWaterSourceIsDiscardedWithoutACrash)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveSteppedWorld(slot);

    SaveWorld b(slot);
    b.source.waterColumns = 0;
    b.Announce(5);
    ASSERT_FALSE(b.module.GetStage().IsReady());
    RestoreLog log("WTH");
    const Dia::SaveGame::LoadResult result = b.Load();
    EXPECT_TRUE(result.Ok()) << static_cast<int>(result.code);
    EXPECT_GE(log.Warnings("discarded"), 1u);
    b.module.GetStage().Update();
    EXPECT_FALSE(b.module.GetStage().IsReady());
    EXPECT_EQ(nullptr, b.module.GetStage().ReadableSim());
    // A later world with water spins up normally.
    b.source.waterColumns = 1;
    b.Announce(6);
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(6));
}

TEST(WeatherSaveParticipant, ReAnnounceOfTheRestoredRevisionIsIgnoredAndAGreaterRevisionSpinsUp)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const Snap saved = SaveSteppedWorld(slot);

    SaveWorld b(slot);
    uint32_t spinWeeks = 0;
    b.module.GetStage().SetSpinUpWeekObserver([&spinWeeks](uint32_t) { ++spinWeeks; });
    b.Announce(5);
    ASSERT_TRUE(b.Load().Ok());
    const uint32_t afterLoad = spinWeeks;
    b.Announce(5);   // the completion caused by the load
    EXPECT_EQ(afterLoad, spinWeeks);
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
    EXPECT_EQ(saved, Take(b.module.GetStage()));

    b.Announce(6);
    EXPECT_GT(spinWeeks, afterLoad);
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(6));
    EXPECT_NE(saved, Take(b.module.GetStage()));
}

TEST(WeatherSaveParticipant, RestoredWeekIsTheSavedWeekAndTheFirstStepPublishesItsOwnWeek)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveSteppedWorld(slot);

    SaveWorld b(slot);
    std::vector<uint64_t> published;
    b.module.GetStage().SetPublishSeam([&published](uint64_t week, Draoi::HexWorld::Weather::Season) { published.push_back(week); });
    b.Announce(5);
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(3u, Take(b.module.GetStage()).week);
    EXPECT_EQ(3u, b.module.GetStage().GetSnapshot().week);
    b.StepWeeks(4, 1);   // stepDay 28
    ASSERT_EQ(1u, published.size());
    EXPECT_EQ(4u, published[0]);
    EXPECT_EQ(4u, Take(b.module.GetStage()).week);
}

// ---- Task 11: params_hash on load (AC 11) ---------------------------------------------------------------------------

namespace
{
// Loads the shipped parameter sidecar (plus optional packs) into a SaveWorld's stage.
bool LoadSidecarParams(SaveWorld& w, const std::vector<Json::Value>& packs = {})
{
    static WeatherParamKit::CountingSource source;
    CoW::Weather::ParamError err;
    const bool ok = w.module.GetStage().LoadParameters(source, packs, err);
    EXPECT_TRUE(ok) << err.key << ": " << err.reason;
    return ok;
}

// A full copy of the sidecar's baseline.spring map with Grassland moved.
Json::Value SpringMapPack(double grassland)
{
    Json::Value doc = WeatherParamKit::ParseJson(WeatherParamKit::SidecarText());
    Json::Value pack(Json::objectValue);
    pack["format"] = "cow.weather_params";
    pack["version"] = 1;
    for (const Json::Value& e : doc["entries"])
    {
        if (e["key"].asString() == "baseline.spring")
        {
            Json::Value map = e["default"];
            map["Biome.Grassland"] = grassland;
            pack["baseline.spring"] = map;
        }
    }
    return pack;
}

Json::Value AlbedoPack(double v)
{
    return WeatherParamKit::ParseJson("{\"format\":\"cow.weather_params\",\"version\":1,\"solar.cloud_albedo\":" + std::to_string(v) + "}");
}

bool IsDegraded(SaveWorld& w)
{
    return w.module.GetStage().GetHealth().status == Dia::Observation::Health::HealthStatus::kDegraded;
}

// Saves a world stepped under `packs`, returning the saved params hash.
uint64_t SaveUnderParams(const std::string& slot, const std::vector<Json::Value>& packs)
{
    SaveWorld a(slot);
    LoadSidecarParams(a, packs);
    a.Announce(5);
    a.StepWeeks(1, 3);
    EXPECT_TRUE(a.Save().Ok());
    return a.module.GetStage().ParametersHash();
}
}

TEST(WeatherSaveParticipant, SavedParamsHashEqualsParametersHashAndChangesWithAPerBiomeMapValue)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveWorld a(slot);
    ASSERT_TRUE(LoadSidecarParams(a));
    a.Announce(5);
    a.StepWeeks(1, 2);
    const uint64_t hash = a.module.GetStage().ParametersHash();
    ASSERT_NE(0u, hash);
    EXPECT_EQ(hash, a.Record()["params_hash"].asUInt64());

    SaveWorld b(WsTest::UniqueSlot());
    ASSERT_TRUE(LoadSidecarParams(b, { SpringMapPack(0.56) }));
    EXPECT_NE(hash, b.module.GetStage().ParametersHash());
    SaveWorld c(WsTest::UniqueSlot());
    ASSERT_TRUE(LoadSidecarParams(c, { SpringMapPack(0.56) }));
    EXPECT_EQ(b.module.GetStage().ParametersHash(), c.module.GetStage().ParametersHash());
}

TEST(WeatherSaveParticipant, LoadWithEqualParametersGivesNoWarningAndIsNotDegraded)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveUnderParams(slot, {});

    SaveWorld b(slot);
    ASSERT_TRUE(LoadSidecarParams(b));
    b.Announce(5);
    RestoreLog log("WTH");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(0u, log.Warnings());
    EXPECT_FALSE(IsDegraded(b));
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
}

TEST(WeatherSaveParticipant, LoadUnderDifferentScalarParametersWarnsOnceDegradesAndRunsOnCurrentParameters)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const uint64_t savedHash = SaveUnderParams(slot, {});

    SaveWorld b(slot);
    ASSERT_TRUE(LoadSidecarParams(b, { AlbedoPack(0.55) }));
    ASSERT_NE(savedHash, b.module.GetStage().ParametersHash());
    b.Announce(5);
    RestoreLog log("WTH");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(1u, log.Warnings());
    EXPECT_EQ(1u, log.Warnings("params_hash"));
    EXPECT_TRUE(IsDegraded(b));
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));

    // The run continues under the current parameters.
    Draoi::HexWorld::Weather::WeatherParameters p;
    b.module.GetStage().Parameters().Snapshot(p);
    EXPECT_FLOAT_EQ(0.55f, p.cloudAlbedo);
    b.StepWeeks(4, 1);
    EXPECT_EQ(4u, Take(b.module.GetStage()).week);
    EXPECT_TRUE(IsDegraded(b));
}

TEST(WeatherSaveParticipant, LoadUnderADifferentPerBiomeMapValueWarnsOnceAndDegrades)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveUnderParams(slot, {});

    SaveWorld b(slot);
    ASSERT_TRUE(LoadSidecarParams(b, { SpringMapPack(0.56) }));
    b.Announce(5);
    RestoreLog log("WTH");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(1u, log.Warnings());
    EXPECT_TRUE(IsDegraded(b));
}

TEST(WeatherSaveParticipant, LoadAfterAQueuedOverrideAppliedWarnsOnceAndDegrades)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveUnderParams(slot, {});

    SaveWorld b(slot);
    ASSERT_TRUE(LoadSidecarParams(b));
    b.Announce(5);
    CoW::Weather::ParamError err;
    ASSERT_TRUE(b.module.GetStage().QueueParameterOverrides(AlbedoPack(0.5), err));
    b.StepWeeks(1, 1);   // applied at Capture; the hash moves
    RestoreLog log("WTH");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(1u, log.Warnings());
    EXPECT_TRUE(IsDegraded(b));
}

TEST(WeatherSaveParticipant, AFollowingOverridePackOrReloadApplyClearsDegraded)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    SaveUnderParams(slot, {});

    for (int viaReload = 0; viaReload < 2; ++viaReload)
    {
        SCOPED_TRACE(viaReload);
        SaveWorld b(slot);
        ASSERT_TRUE(LoadSidecarParams(b, { AlbedoPack(0.55) }));
        b.Announce(5);
        ASSERT_TRUE(b.Load().Ok());
        ASSERT_TRUE(IsDegraded(b));
        ASSERT_EQ(StringCRC("params_changed"), b.module.GetStage().GetHealth().reason);
        CoW::Weather::ParamError err;
        if (viaReload) { ASSERT_TRUE(b.module.GetStage().QueueReload()); }
        else { ASSERT_TRUE(b.module.GetStage().QueueParameterOverrides(AlbedoPack(0.5), err)); }
        b.StepWeeks(4, 1);
        // The stepped stage may still be Degraded for the no-baseline-provider fallback; params_changed must be gone.
        EXPECT_NE(StringCRC("params_changed"), b.module.GetStage().GetHealth().reason);
    }
}

// ---- Task 12 / AC 13: registration with SaveLoadModule ----
namespace
{
class OrderProbe : public Dia::SaveGame::ISaveable
{
public:
    void Serialize(Dia::SaveGame::SaveContext&) const override {}
    void Deserialize(Dia::SaveGame::LoadContext&) override {}
    uint32_t GetVersion() const override { return 1; }
};

struct RegistrationRig
{
    RegistrationRig()
        : harness(WsTest::Config("weather", 7, true))
        , module(StringCRC("WeatherModule"))
        , saveLoad(StringCRC("SaveLoadModule"))
    {
        hosts.worldStep = &harness.module;
        hosts.worldSource = &source;
        hosts.generationHold = &hold;
    }
    void Arm() { module.UseTestHosts(hosts); module.GetStage().SetRegistry(&Draoi::HexWorld::Weather::Testing::TestRegistry()); }
    WsTest::Harness harness;
    OneWorldSource source;
    NullHold hold;
    WeatherProbe module;
    CoW::SaveLoadModule saveLoad;
    CoW::WeatherModule::TestHosts hosts;
};
}

TEST(WeatherSaveRegistration, WithoutSaveLoadModuleTheModuleRunsAndRegistersNothing)
{
    RegistrationRig rig;
    rig.Arm();
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(0u, rig.saveLoad.GetRegistry().GetParticipantCount());
    rig.module.CallStop();
}

TEST(WeatherSaveRegistration, RegistersCowWeatherInDoStartAndUnregistersInDoStop)
{
    RegistrationRig rig;
    rig.hosts.saveLoad = &rig.saveLoad;
    rig.Arm();
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(1u, rig.saveLoad.GetRegistry().GetParticipantCount());
    EXPECT_EQ(&rig.module.GetSaveParticipant(), rig.saveLoad.GetRegistry().GetParticipantAt(0));
    rig.module.CallStop();
    EXPECT_EQ(0u, rig.saveLoad.GetRegistry().GetParticipantCount());
}

TEST(WeatherSaveRegistration, ArrivesBeforeTheDeferredStepParticipant)
{
    RegistrationRig rig;
    rig.hosts.saveLoad = &rig.saveLoad;
    rig.Arm();
    OrderProbe deferred;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    // DeferredStepModule attaches its participant on its first DoUpdate, after every DoStart.
    ASSERT_TRUE(rig.saveLoad.RegisterParticipant(StringCRC("deferred_step"), &deferred));
    ASSERT_EQ(2u, rig.saveLoad.GetRegistry().GetParticipantCount());
    EXPECT_EQ(&rig.module.GetSaveParticipant(), rig.saveLoad.GetRegistry().GetParticipantAt(0));
    EXPECT_EQ(&deferred, rig.saveLoad.GetRegistry().GetParticipantAt(1));
    rig.module.CallStop();
    rig.saveLoad.UnregisterParticipant(StringCRC("deferred_step"));
}

TEST(WeatherSaveRegistration, StartStopStartRegistersExactlyOnce)
{
    RegistrationRig rig;
    rig.hosts.saveLoad = &rig.saveLoad;
    rig.Arm();
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    rig.module.CallStop();
    EXPECT_EQ(0u, rig.saveLoad.GetRegistry().GetParticipantCount());
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(1u, rig.saveLoad.GetRegistry().GetParticipantCount());
    rig.module.CallStop();
    EXPECT_EQ(0u, rig.saveLoad.GetRegistry().GetParticipantCount());
}

TEST(WeatherSaveRegistration, SchemaFromTheHostConfiguresTheParticipantAndAMissingOneLeavesItUnconfigured)
{
    {
        RegistrationRig rig;
        rig.Arm();
        ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
        EXPECT_FALSE(rig.module.GetSaveParticipant().IsConfigured());
        rig.module.CallStop();
    }
    {
        Dia::ValueSchema::ValueSchemaRegistry layers;
        ASSERT_TRUE(PublishSidecar(layers, ReadShippedSidecar()));
        RegistrationRig rig;
        rig.hosts.schema = &layers;
        rig.Arm();
        ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
        EXPECT_TRUE(rig.module.GetSaveParticipant().IsConfigured());
        rig.module.CallStop();
    }
}

// ---- Task 15 / AC 20, AC 8: Large world through SaveLoadModule::CaptureState ----
namespace
{
struct LargeCaptureRig
{
    LargeCaptureRig()
        : harness(WsTest::Config("weather", 7, true))
        , module(StringCRC("WeatherModule"))
        , saveLoad(StringCRC("SaveLoadModule"))
    {
        source.columns = 104;
        source.rows = 64;   // 6,656 cells (Large)
        bus.Initialize();
        bus.RegisterType<CoW::WorldGenerationComplete, 4>();
        bus.RegisterType<CoW::WorldGenerationFailed, 4>();
        bus.RegisterType<CoW::WeatherUpdated, 4>();
        EXPECT_TRUE(PublishSidecar(layers, ReadShippedSidecar()));
        CoW::WeatherModule::TestHosts hosts;
        hosts.worldStep = &harness.module;
        hosts.bus = &bus;
        hosts.worldSource = &source;
        hosts.generationHold = &hold;
        hosts.saveLoad = &saveLoad;
        hosts.schema = &layers;
        module.UseTestHosts(hosts);
        module.GetStage().SetRegistry(&Draoi::HexWorld::Weather::Testing::TestRegistry());
        module.GetStage().SetParameters(Draoi::HexWorld::Weather::Testing::MakeTestParams());
        module.GetStage().SetSpinupWeeks(1);
        module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
        EXPECT_EQ(Dia::ApplicationFlow::StartResult::kReady, module.CallStart());
    }
    ~LargeCaptureRig() { module.CallStop(); }

    void Prepare()
    {
        CoW::WorldGenerationComplete msg;
        msg.worldRevision = 5;
        ASSERT_TRUE(bus.Broadcast(msg));
        bus.Update();
        module.GetStage().Update();
        Dia::ParallelExecutor::SerialExecutor executor;
        for (uint32_t week = 1; week <= 2; ++week)
        {
            module.GetStage().Capture(static_cast<Dia::DeferredStep::StepKey>(week * 7));
            module.GetStage().Run(executor);
            module.GetStage().Commit();
        }
        // a third step in flight: Capture and Run, no Commit
        module.GetStage().Capture(static_cast<Dia::DeferredStep::StepKey>(3 * 7));
        module.GetStage().Run(executor);
    }

    WsTest::Harness harness;
    WeatherProbe module;
    CoW::SaveLoadModule saveLoad;
    Dia::MessageBus::Bus bus;
    OneWorldSource source;
    NullHold hold;
    Dia::ValueSchema::ValueSchemaRegistry layers;
};
}

TEST(WeatherSaveSize, LargeWorldRecordPlusPendingBlobIsUnder5MBAndCaptureStateIsWithinTheBuffer)
{
    LargeCaptureRig rig;
    rig.Prepare();
    ASSERT_EQ(6656u, rig.module.GetStage().ReadableSim()->Fields().CellCount());

    auto list = std::make_unique<CoW::CapturedStateList>();
    const auto t0 = std::chrono::steady_clock::now();
    rig.saveLoad.CaptureState(*list);
    const auto t1 = std::chrono::steady_clock::now();
    ASSERT_EQ(1u, list->Size());
    ASSERT_EQ(StringCRC("cow.weather"), (*list)[0].id);
    const size_t recordBytes = Draoi::HexWorld::Serialize((*list)[0].state).size();

    Dia::SaveGame::SaveContext pendingCtx;
    pendingCtx.BeginObject(StringCRC("stage_0"));
    rig.module.GetStage().SerializePending(pendingCtx);
    pendingCtx.EndObject();
    const std::string pendingText = Draoi::HexWorld::Serialize(pendingCtx.Root());
    ASSERT_TRUE(pendingCtx.Root()["stage_0"]["valid"].asBool());
    const size_t pendingBytes = pendingText.size();

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("[WeatherSaveSize] record=%zu bytes, pending=%zu bytes, total=%zu bytes, CaptureState=%.2f ms (Debug)\n",
                recordBytes, pendingBytes, recordBytes + pendingBytes, ms);
    EXPECT_LT(recordBytes + pendingBytes, 5u * 1024u * 1024u);
    EXPECT_LT(recordBytes + pendingBytes, static_cast<size_t>(Dia::SaveGame::SaveContext::kBufferSize));
}

TEST(WeatherSaveSize, ACorruptedCapturedRecordFailsValidateAndLiveStateIsUnchanged)
{
    LargeCaptureRig rig;
    rig.Prepare();
    auto list = std::make_unique<CoW::CapturedStateList>();
    rig.saveLoad.CaptureState(*list);
    ASSERT_EQ(1u, list->Size());
    const Snap before = Take(rig.module.GetStage());
    ASSERT_TRUE(before.hasSim);

    {
        Dia::SaveGame::LoadContext ctx((*list)[0].state);
        EXPECT_TRUE(rig.module.GetSaveParticipant().Validate(ctx)) << "control";
    }
    Json::Value bad = (*list)[0].state;
    bad["temperature"][0u] = 1.0e9;
    Dia::SaveGame::LoadContext ctx(bad);
    EXPECT_FALSE(rig.module.GetSaveParticipant().Validate(ctx));
    EXPECT_EQ(before, Take(rig.module.GetStage()));
}

// ---- Task 1 spike (Open Design Question 1): the real E1 load order ------------------------------------------------------
// Real SaveManager::Load, real WorldStepModule observing OnLoadStarted, real WeatherModule + participant. A recording
// wrapper stands in for the registered participant so the order of Validate / Deserialize is visible, and a bus probe
// records every WorldGenerationComplete. Finding: no module re-announces a world on a load (WorldGenerationModule is not
// ISaveable and does not observe the SaveManager), so a load produces ZERO completions; the record is applied at once when
// the world is already present, and stashed until the next completion when it is not.
namespace
{
class OrderRecorder final : public Dia::SaveGame::ISaveable
{
public:
    OrderRecorder(Dia::SaveGame::ISaveable& inner, std::vector<std::string>& log, CoW::Weather::WeatherStage& stage)
        : mInner(inner), mLog(log), mStage(stage) {}
    void Serialize(Dia::SaveGame::SaveContext& ctx) const override { mInner.Serialize(ctx); }
    bool Validate(Dia::SaveGame::LoadContext& ctx) const override
    {
        mLog.push_back("Validate");
        return mInner.Validate(ctx);
    }
    void Deserialize(Dia::SaveGame::LoadContext& ctx) override
    {
        mLog.push_back(mStage.ReadableSim() != nullptr ? "Deserialize(world present, sim live)" : "Deserialize(no sim)");
        mInner.Deserialize(ctx);
        mLog.push_back(mStage.HasStashedRecord() ? "after:stashed" : (mStage.IsReadyForRevision(5) ? "after:applied" : "after:other"));
    }
    uint32_t GetVersion() const override { return mInner.GetVersion(); }
private:
    Dia::SaveGame::ISaveable& mInner;
    std::vector<std::string>& mLog;
    CoW::Weather::WeatherStage& mStage;
};

// Wires the real load observer and the recording wrapper around a SaveWorld; undone in the destructor.
struct LoadOrderRig
{
    explicit LoadOrderRig(SaveWorld& world)
        : w(world)
        , recorder(w.module.GetSaveParticipant(), log, w.module.GetStage())
    {
        w.harness.module.ObserveLoads(w.manager);
        w.registry.Unregister(StringCRC("cow.weather"));
        w.registry.Register(StringCRC("cow.weather"), &recorder);
        completions = w.bus.Subscribe<CoW::WorldGenerationComplete>(
            StringCRC("LoadOrderProbe"), [this](const CoW::WorldGenerationComplete& m) {
                log.push_back("WorldGenerationComplete(rev " + std::to_string(m.worldRevision) + ")");
            });
    }
    ~LoadOrderRig()
    {
        w.registry.Unregister(StringCRC("cow.weather"));
        w.registry.Register(StringCRC("cow.weather"), &w.module.GetSaveParticipant());   // SaveWorld's dtor unregisters it
        w.harness.module.StopObservingLoads();
    }
    // Everything the bus delivers after a load, as the frame loop would.
    void Pump() { w.bus.Update(); w.module.GetStage().Update(); }
    uint32_t Count(const std::string& prefix) const
    {
        uint32_t n = 0;
        for (const std::string& e : log) { if (e.rfind(prefix, 0) == 0) { ++n; } }
        return n;
    }

    SaveWorld& w;
    std::vector<std::string> log;
    OrderRecorder recorder;
    decltype(std::declval<Dia::MessageBus::Bus&>().Subscribe<CoW::WorldGenerationComplete>(StringCRC(), nullptr)) completions;
};
}

TEST(WeatherSaveLoadOrder, WorldAlreadyAnnouncedAtLoad_LoadCausesNoCompletion_RecordAppliedAtOnce_ReAnnounceIgnored)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const Snap saved = SaveSteppedWorld(slot);

    SaveWorld b(slot);
    uint32_t spinWeeks = 0;
    b.module.GetStage().SetSpinUpWeekObserver([&spinWeeks](uint32_t) { ++spinWeeks; });
    b.Announce(5);   // the world generation announced before the load (the in-game order)
    const uint32_t spinAfterAnnounce = spinWeeks;
    LoadOrderRig rig(b);
    rig.log.clear();

    ASSERT_TRUE(b.Load().Ok());
    rig.Pump();

    // Order: Validate, then Deserialize; the world is present and applied inside Deserialize; nothing is announced by the load.
    ASSERT_GE(rig.log.size(), 3u);
    EXPECT_EQ("Validate", rig.log[0]);
    EXPECT_EQ("Deserialize(world present, sim live)", rig.log[1]);
    EXPECT_EQ("after:applied", rig.log[2]);
    EXPECT_EQ(0u, rig.Count("WorldGenerationComplete")) << "E1 gained a completion on load: re-check restore step 7";
    EXPECT_EQ(3u, rig.log.size());
    EXPECT_FALSE(b.module.GetStage().HasStashedRecord());
    EXPECT_EQ(spinAfterAnnounce, spinWeeks);
    EXPECT_EQ(saved, Take(b.module.GetStage()));

    // Were a completion of the restored revision to arrive anyway, it is ignored: no spin-up, state untouched.
    b.Announce(5);
    EXPECT_EQ(spinAfterAnnounce, spinWeeks);
    EXPECT_EQ(saved, Take(b.module.GetStage()));
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
}

TEST(WeatherSaveLoadOrder, LoadBeforeAnyWorldIsAnnounced_RecordStashed_NextCompletionAppliesItWithoutSpinUp)
{
    const std::string slot = WsTest::UniqueSlot();
    WsTest::SlotFile file(slot);
    const Snap saved = SaveSteppedWorld(slot);

    SaveWorld b(slot);
    uint32_t spinWeeks = 0;
    b.module.GetStage().SetSpinUpWeekObserver([&spinWeeks](uint32_t) { ++spinWeeks; });
    LoadOrderRig rig(b);
    rig.log.clear();

    ASSERT_TRUE(b.Load().Ok());
    rig.Pump();
    ASSERT_EQ(3u, rig.log.size());
    EXPECT_EQ("Validate", rig.log[0]);
    EXPECT_EQ("Deserialize(no sim)", rig.log[1]);
    EXPECT_EQ("after:stashed", rig.log[2]);
    EXPECT_EQ(0u, rig.Count("WorldGenerationComplete")) << "the load itself announces nothing";
    EXPECT_FALSE(b.module.GetStage().IsReady());

    b.Announce(5);   // the first real completion
    EXPECT_EQ(1u, rig.Count("WorldGenerationComplete(rev 5)"));
    EXPECT_FALSE(b.module.GetStage().HasStashedRecord());
    EXPECT_EQ(0u, spinWeeks) << "stash applied at the completion, no spin-up";
    EXPECT_TRUE(b.module.GetStage().IsReadyForRevision(5));
    EXPECT_EQ(saved, Take(b.module.GetStage()));
}
