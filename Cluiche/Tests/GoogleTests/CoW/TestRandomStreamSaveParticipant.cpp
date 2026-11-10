// Tests for the RandomStreamModule save participant (E1): each stream persists as seed + draw count
// and is replayed on restore. Real Application: SaveLoad + RandomStream.
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaSaveGame/LoadContext.h>
#include <Modules/RandomStreamModule.h>
#include <Modules/SaveLoadModule.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;

namespace {

RandomStreamModule* g_rng  = nullptr;
SaveLoadModule*     g_save = nullptr;

Module* CreateRng(const StringCRC& id)  { g_rng  = new RandomStreamModule(id); return g_rng; }
Module* CreateSave(const StringCRC& id) { g_save = new SaveLoadModule(id); return g_save; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    RandomStreamModule* rng = nullptr;
    SaveLoadModule* save = nullptr;
    std::string dir;
    std::string saveConfig;

    explicit Fixture(const char* testName, bool withSave = true)
    {
        dir = std::string("out/test_saves/") + testName + "/";
        std::filesystem::remove_all(dir);

        reg.Register(StringCRC("RandomStreamModule"), &CreateRng);
        reg.Register(StringCRC("SaveLoadModule"), &CreateSave);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        if (withSave)
        {
            ModuleDeclaration saveDecl;
            saveDecl.instanceId = StringCRC("SaveLoadModule");
            saveDecl.typeId = StringCRC("SaveLoadModule");
            saveDecl.stages.Add(StringCRC("all"));
            saveConfig = std::string("{\"base_directory\":\"") + dir + "\"}";
            saveDecl.configJson = saveConfig.c_str();
            pu.modules.Add(saveDecl);
        }

        ModuleDeclaration rngDecl;
        rngDecl.instanceId = StringCRC("RandomStreamModule");
        rngDecl.typeId = StringCRC("RandomStreamModule");
        rngDecl.stages.Add(StringCRC("all"));
        if (withSave)
            rngDecl.dependencies.Add(StringCRC("SaveLoadModule"));
        pu.modules.Add(rngDecl);

        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
        if (app)
        {
            app->RequestShutdown();
            for (int i = 0; i < 50 && app->Update(kDt); ++i) {}
        }
        app.reset();
        std::filesystem::remove_all(dir);
    }

    bool Boot(bool expectSave = true)
    {
        g_rng = nullptr; g_save = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        rng = g_rng; save = g_save;
        if (!rng || (expectSave && !save))
            return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (rng->GetState() == ModuleState::kActive && (!expectSave || save->GetState() == ModuleState::kActive))
            {
                Pump(2);   // lets the module register its participant lazily
                return true;
            }
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }

    template <class Pred> bool PumpUntil(Pred done, int maxFrames = 20)
    {
        for (int i = 0; i < maxFrames && !done(); ++i)
            app->Update(kDt);
        return done();
    }

    std::string SlotPath(const char* slot) const { return dir + "slot_" + slot + ".sav"; }

    Dia::Maths::RandomStream* Stream(int i) { return rng->GetStreamByName(StringCRC(RandomStreamModule::kStreamNames[i])); }
};

constexpr int kStreams = RandomStreamModule::kStreamCount;

} // namespace

TEST(RandomStreamSaveParticipant, SaveWritesSeedAndDrawsPerStream)
{
    Fixture f("rs_participant_key");
    ASSERT_TRUE(f.Boot());
    f.Stream(0)->RandomInt();
    f.save->RequestSave(StringCRC("k"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));

    std::ifstream in(f.SlotPath("k"), std::ios::binary);
    ASSERT_TRUE(in.good());
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(in, root));
    ASSERT_TRUE(root.isMember("random_streams"));
    for (int i = 0; i < kStreams; ++i)
    {
        const char* name = RandomStreamModule::kStreamNames[i];
        ASSERT_TRUE(root["random_streams"].isMember(name)) << name;
        EXPECT_TRUE(root["random_streams"][name].isMember("seed")) << name;
        EXPECT_TRUE(root["random_streams"][name].isMember("draws")) << name;
    }
    EXPECT_EQ(root["random_streams"]["WLD"]["seed"].asInt64(), 10000);
    EXPECT_GE(root["random_streams"]["WLD"]["draws"].asInt64(), 1);
    EXPECT_EQ(root["random_streams"]["A1"]["draws"].asInt64(), 0);
    EXPECT_EQ(f.rng->GetSaveParticipant().GetVersion(), 1u);
}

TEST(RandomStreamSaveParticipant, RoundTripReplaysIdenticalDraws)
{
    Fixture f("rs_participant_roundtrip");
    ASSERT_TRUE(f.Boot());
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 3 + i * 7; ++n)
        {
            if (i % 2 == 0) f.Stream(i)->RandomInt();
            else            f.Stream(i)->RandomUnit();
        }

    f.save->RequestSave(StringCRC("q"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));
    ASSERT_TRUE(f.save->GetLastSaveResult().Ok());

    std::vector<int> expected;
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 100; ++n)
            expected.push_back(f.Stream(i)->RandomInt());

    // Diverge: draw more from every stream.
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 50 + i; ++n)
            f.Stream(i)->RandomInt();

    f.save->RequestLoad(StringCRC("q"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    f.Pump(1);

    size_t k = 0;
    for (int i = 0; i < kStreams; ++i)
        for (int n = 0; n < 100; ++n, ++k)
            ASSERT_EQ(f.Stream(i)->RandomInt(), expected[k]) << "stream " << i << " draw " << n;
}

TEST(RandomStreamSaveParticipant, UndrawnStreamsRestoreWithZeroDraws)
{
    Fixture f("rs_participant_undrawn");
    ASSERT_TRUE(f.Boot());
    f.save->RequestSave(StringCRC("z"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetSaveCount() == 1; }));

    for (int n = 0; n < 20; ++n)
        f.Stream(3)->RandomInt();
    ASSERT_GT(f.Stream(3)->GetDrawCount(), 0u);

    f.save->RequestLoad(StringCRC("z"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.save->GetLoadCount() == 1; }));
    f.Pump(1);

    for (int i = 0; i < kStreams; ++i)
        EXPECT_EQ(f.Stream(i)->GetDrawCount(), 0u) << "stream " << i;
}

TEST(RandomStreamSaveParticipant, MissingStreamObjectLeavesThatStreamUntouched)
{
    Fixture f("rs_participant_missing");
    ASSERT_TRUE(f.Boot());

    // Build JSON for every stream except index 1 (A1), each with seed 5 and 4 draws.
    Json::Value root(Json::objectValue);
    for (int i = 0; i < kStreams; ++i)
    {
        if (i == 1)
            continue;
        Json::Value s(Json::objectValue);
        s["seed"] = static_cast<Json::Int64>(5);
        s["draws"] = static_cast<Json::Int64>(4);
        root[RandomStreamModule::kStreamNames[i]] = s;
    }

    for (int n = 0; n < 9; ++n)
        f.Stream(1)->RandomInt();
    const uint64_t untouchedDraws = f.Stream(1)->GetDrawCount();
    const unsigned untouchedSeed = f.Stream(1)->GetSeed();

    Dia::SaveGame::LoadContext ctx(root);
    f.rng->GetSaveParticipant().Deserialize(ctx);

    EXPECT_EQ(f.Stream(1)->GetDrawCount(), untouchedDraws);
    EXPECT_EQ(f.Stream(1)->GetSeed(), untouchedSeed);
    for (int i = 0; i < kStreams; ++i)
    {
        if (i == 1)
            continue;
        EXPECT_EQ(f.Stream(i)->GetSeed(), 5u) << "stream " << i;
        EXPECT_EQ(f.Stream(i)->GetDrawCount(), 4u) << "stream " << i;
    }
}

TEST(RandomStreamSaveParticipant, WorksWithoutSaveLoadModule)
{
    Fixture f("rs_participant_nosave", false);
    ASSERT_TRUE(f.Boot(false));
    f.Pump(5);
    EXPECT_EQ(f.rng->GetState(), ModuleState::kActive);
    EXPECT_NE(f.rng->GetWorldStream(), nullptr);
}
