// Integration tests for CoW::SaveLoadModule running inside a real Application (30 Hz SimPU).
// Each test saves into its own directory under out/test_saves/ and removes it afterwards.
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <Modules/SaveLoadModule.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::SaveGame;
using namespace CoW;

namespace {

SaveLoadModule* g_module = nullptr;
Module* CreateModule(const StringCRC& id) { g_module = new SaveLoadModule(id); return g_module; }

constexpr float kDt = 1.0f / 30.0f;

// Records the order participants are serialized in across all fakes.
std::vector<std::string>* g_order = nullptr;

struct FakeSaveable : ISaveable
{
    explicit FakeSaveable(const char* tag) { std::snprintf(mTag, sizeof(mTag), "%s", tag); }

    int32_t value = 0;
    char    text[32] = "";
    int     deserializeCount = 0;
    // Optional hook used to simulate a request made mid-frame (from inside the save).
    SaveLoadModule* requestModule = nullptr;
    const char*     requestSlot = nullptr;

    void Serialize(SaveContext& ctx) const override
    {
        if (g_order)
            g_order->push_back(mTag);
        ctx.Write(StringCRC("value"), value);
        ctx.Write(StringCRC("text"), static_cast<const char*>(text));
        if (requestModule && requestSlot)
            requestModule->RequestSave(StringCRC(requestSlot));
    }

    void Deserialize(LoadContext& ctx) override
    {
        ++deserializeCount;
        ctx.Read(StringCRC("value"), value);
        ctx.Read(StringCRC("text"), text, sizeof(text));
    }

    uint32_t GetVersion() const override { return 1; }

private:
    char mTag[16] = "";
};

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    SaveLoadModule* module = nullptr;
    std::string dir;
    std::string configJson;

    explicit Fixture(const char* testName, const char* baseDirOverride = nullptr)
    {
        dir = std::string("out/test_saves/") + testName + "/";
        std::filesystem::remove_all(dir);

        reg.Register(StringCRC("SaveLoadModule"), &CreateModule);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration decl;
        decl.instanceId = StringCRC("SaveLoadModule");
        decl.typeId = StringCRC("SaveLoadModule");
        decl.stages.Add(StringCRC("all"));
        configJson = std::string("{\"base_directory\":\"") + (baseDirOverride ? baseDirOverride : dir.c_str()) + "\"}";
        decl.configJson = configJson.c_str();
        pu.modules.Add(decl);
        manifest.processingUnits.Add(pu);
    }

    ~Fixture()
    {
        if (app)
        {
            // Shut down properly so DoStop unregisters the commands before the module is destroyed.
            app->RequestShutdown();
            for (int i = 0; i < 50 && app->Update(kDt); ++i) {}
        }
        app.reset();
        std::filesystem::remove_all(dir);
    }

    bool Boot()
    {
        g_module = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        module = g_module;
        if (!module)
            return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (module->GetState() == ModuleState::kActive)
                return true;
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }

    // Pumps until `done` or a frame budget runs out.
    template <class Pred> bool PumpUntil(Pred done, int maxFrames = 20)
    {
        for (int i = 0; i < maxFrames && !done(); ++i)
            app->Update(kDt);
        return done();
    }

    std::string SlotPath(const char* slot) const { return dir + "slot_" + slot + ".sav"; }
    bool SlotFileExists(const char* slot) const { return std::filesystem::exists(SlotPath(slot)); }
};

} // namespace

TEST(SaveLoadModule, ConfigDefaults)
{
    // Default config (no JSON override) must match the E1 spec.
    const SaveConfig config = SaveLoadModule::DefaultConfig();
    EXPECT_STREQ(config.baseDirectory, "out/saves/cow/");
    EXPECT_STREQ(config.slotPattern, "slot_{id}.sav");
    EXPECT_EQ(config.maxSlots, 0u);
    EXPECT_EQ(config.format, SaveFormat::Json);
}

TEST(SaveLoadModule, ParticipantsSerializeInRegistrationOrder)
{
    Fixture f("order");
    ASSERT_TRUE(f.Boot());
    std::vector<std::string> order;
    g_order = &order;

    FakeSaveable a("A"), b("B");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RegisterParticipant(StringCRC("part_b"), &b);

    f.module->RequestSave(StringCRC("one"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    g_order = nullptr;

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "A");
    EXPECT_EQ(order[1], "B");
}

TEST(SaveLoadModule, UnregisteredParticipantIsNotSerialized)
{
    Fixture f("unregister");
    ASSERT_TRUE(f.Boot());
    std::vector<std::string> order;
    g_order = &order;

    FakeSaveable a("A"), b("B");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RegisterParticipant(StringCRC("part_b"), &b);
    f.module->UnregisterParticipant(StringCRC("part_a"));

    f.module->RequestSave(StringCRC("one"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    g_order = nullptr;

    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], "B");
}

TEST(SaveLoadModule, RoundTripRestoresState)
{
    Fixture f("roundtrip");
    ASSERT_TRUE(f.Boot());

    FakeSaveable a("A");
    a.value = 42;
    std::snprintf(a.text, sizeof(a.text), "%s", "hello");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("rt"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    EXPECT_TRUE(f.module->GetLastSaveResult().Ok());

    a.value = -1;
    std::snprintf(a.text, sizeof(a.text), "%s", "mutated");

    f.module->RequestLoad(StringCRC("rt"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetLoadCount() == 1; }));
    EXPECT_TRUE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.value, 42);
    EXPECT_STREQ(a.text, "hello");
}

TEST(SaveLoadModule, RequestSaveDefersToUpdateAndRunsOnce)
{
    Fixture f("deferred");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("later"));
    EXPECT_TRUE(f.module->HasPendingSave());
    EXPECT_FALSE(f.SlotFileExists("later"));
    EXPECT_EQ(f.module->GetSaveCount(), 0u);

    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    EXPECT_TRUE(f.SlotFileExists("later"));
    EXPECT_FALSE(f.module->HasPendingSave());

    f.Pump(5);
    EXPECT_EQ(f.module->GetSaveCount(), 1u);
}

TEST(SaveLoadModule, RequestFromInsideSaveRunsAtNextUpdate)
{
    Fixture f("midframe");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.requestModule = f.module;
    a.requestSlot = "second";
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("first"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() >= 1; }));

    // The request raised during the first save must not have run in the same update.
    EXPECT_EQ(f.module->GetSaveCount(), 1u);
    EXPECT_TRUE(f.SlotFileExists("first"));
    EXPECT_FALSE(f.SlotFileExists("second"));
    EXPECT_TRUE(f.module->HasPendingSave());

    a.requestModule = nullptr; // stop the chain
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 2; }));
    EXPECT_TRUE(f.SlotFileExists("second"));
}

TEST(SaveLoadModule, EnumerateSlotsListsSavedSlots)
{
    Fixture f("enumerate");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    const char* slots[3] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < 3; ++i)
    {
        f.module->RequestSave(StringCRC(slots[i]));
        const unsigned int want = static_cast<unsigned int>(i + 1);
        ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == want; }));
    }

    Dia::Core::Containers::DynamicArrayC<SlotInfo, 32> out;
    f.module->EnumerateSlots(out);
    ASSERT_EQ(out.Size(), 3u);
    int found = 0;
    for (unsigned int i = 0; i < out.Size(); ++i)
        for (int s = 0; s < 3; ++s)
            if (out[i].id == StringCRC(slots[s]))
                ++found;
    EXPECT_EQ(found, 3);
}

TEST(SaveLoadModule, LoadOfMissingSlotLeavesStateUntouched)
{
    Fixture f("missing");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 7;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestLoad(StringCRC("nope"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    f.Pump(1);

    EXPECT_FALSE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(f.module->GetLastLoadResult().code, LoadResultCode::SlotNotFound);
    EXPECT_EQ(f.module->GetLoadCount(), 0u);
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(a.value, 7);
}

TEST(SaveLoadModule, LoadOfCorruptedFileLeavesStateUntouched)
{
    Fixture f("corrupt");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 7;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    {
        std::ofstream out(f.SlotPath("bad"), std::ios::binary);
        ASSERT_TRUE(out.good());
        out << "this is {{ not json at all";
    }

    f.module->RequestLoad(StringCRC("bad"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));

    EXPECT_FALSE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(f.module->GetLastLoadResult().code, LoadResultCode::ParseError);
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(a.value, 7);
}

TEST(SaveLoadModule, LoadOfFileWithoutManifestLeavesStateUntouched)
{
    Fixture f("nomanifest");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 7;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    {
        std::ofstream out(f.SlotPath("nm"), std::ios::binary);
        out << "{\"part_a\":{\"value\":99,\"text\":\"x\"}}";
    }

    f.module->RequestLoad(StringCRC("nm"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));

    EXPECT_FALSE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(a.value, 7);
}

TEST(SaveLoadModule, CommandsRequireSlotArgumentAndQueueWithOne)
{
    Dia::API::Initialize();
    {
        Fixture f("commands");
        ASSERT_TRUE(f.Boot());

        Dia::API::CommandArgs none;
        EXPECT_NE(Dia::API::ExecuteCommand(StringCRC("cow.save"), none), 0);
        EXPECT_NE(Dia::API::ExecuteCommand(StringCRC("cow.load"), none), 0);
        EXPECT_FALSE(f.module->HasPendingSave());
        EXPECT_FALSE(f.module->HasPendingLoad());

        Dia::API::CommandArgs withSlot;
        withSlot.positionalArgs.Add("quick");
        EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cow.save"), withSlot), 0);
        EXPECT_TRUE(f.module->HasPendingSave());
        EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cow.load"), withSlot), 0);
        EXPECT_TRUE(f.module->HasPendingLoad());
        EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cow.save.list"), none), 0);
    }
    Dia::API::Shutdown();
}

// ---------------------------------------------------------------------------
// Large saves (SaveManager buffer is 10 MB)
// ---------------------------------------------------------------------------

namespace {
struct BigSaveable : ISaveable
{
    std::string text;
    void Serialize(SaveContext& ctx) const override { ctx.Write(StringCRC("blob"), text.c_str()); }
    void Deserialize(LoadContext& ctx) override
    {
        std::vector<char> buf(text.capacity() + 1);
        std::string out;
        if (ctx.Read(StringCRC("blob"), buf.data(), static_cast<unsigned int>(buf.size())))
            text.assign(buf.data());
    }
    uint32_t GetVersion() const override { return 1; }
};
}

TEST(SaveLoadModule, SaveLargerThanOldSixtyFourKbBufferRoundTrips)
{
    Fixture f("bigsave");
    ASSERT_TRUE(f.Boot());

    BigSaveable big;
    big.text.assign(500000, 'x');
    big.text[1234] = 'Q';
    f.module->RegisterParticipant(StringCRC("big"), &big);

    f.module->RequestSave(StringCRC("big"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    ASSERT_TRUE(f.module->GetLastSaveResult().Ok());
    EXPECT_GT(std::filesystem::file_size(f.SlotPath("big")), 65536u);

    const std::string expected = big.text;
    big.text.assign(expected.size(), 'y');
    f.module->RequestLoad(StringCRC("big"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_TRUE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(big.text, expected);
}

// ---------------------------------------------------------------------------
// Health
// ---------------------------------------------------------------------------

namespace {
using Dia::Observation::Health::HealthStatus;
HealthStatus StatusOf(const Fixture& f) { return f.module->GetHealthReporter().Report().status; }
StringCRC ReasonOf(const Fixture& f) { return f.module->GetHealthReporter().Report().reason; }
}

TEST(SaveLoadHealth, OkAfterStart)
{
    Fixture f("health_start");
    ASSERT_TRUE(f.Boot());
    EXPECT_EQ(StatusOf(f), HealthStatus::kOK);
}

TEST(SaveLoadHealth, FailedLoadDegradesAndNextSuccessfulLoadRecovers)
{
    Fixture f("health_load");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestLoad(StringCRC("nothere"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
    EXPECT_EQ(ReasonOf(f), StringCRC("last_load_failed"));
    EXPECT_GE(f.module->GetHealthReporter().Report().errors, 1u);

    f.module->RequestSave(StringCRC("good"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    f.module->RequestLoad(StringCRC("good"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_EQ(StatusOf(f), HealthStatus::kOK);
}

TEST(SaveLoadHealth, FailedSaveDegradesAndNextSuccessfulSaveRecovers)
{
    Fixture f("health_save");
    ASSERT_TRUE(f.Boot());
    // A directory where the slot file should go makes fopen("wb") fail.
    std::filesystem::create_directories(f.SlotPath("bad"));

    f.module->RequestSave(StringCRC("bad"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    EXPECT_FALSE(f.module->GetLastSaveResult().Ok());
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
    EXPECT_EQ(ReasonOf(f), StringCRC("last_save_failed"));

    f.module->RequestSave(StringCRC("good"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    EXPECT_EQ(StatusOf(f), HealthStatus::kOK);
}

TEST(SaveLoadHealth, RepeatedSaveFailuresEscalateToFailing)
{
    Fixture f("health_repeat");
    ASSERT_TRUE(f.Boot());
    std::filesystem::create_directories(f.SlotPath("bad"));

    for (uint32_t i = 0; i < SaveLoadModule::kFailingAfterConsecutiveSaveFailures; ++i)
    {
        EXPECT_EQ(StatusOf(f), i == 0 ? HealthStatus::kOK : HealthStatus::kDegraded);
        f.module->RequestSave(StringCRC("bad"));
        ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    }
    EXPECT_EQ(StatusOf(f), HealthStatus::kFailing);
    EXPECT_EQ(ReasonOf(f), StringCRC("saves_repeatedly_failing"));

    f.module->RequestSave(StringCRC("good"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    EXPECT_EQ(StatusOf(f), HealthStatus::kOK);
}

TEST(SaveLoadHealth, UncreatableSaveDirectoryIsFailing)
{
    // A regular file in the path makes create_directories fail.
    // (The blocker lives outside the fixture's own directory, which the fixture wipes.)
    std::filesystem::remove_all("out/test_saves/health_dir_blocker");
    std::filesystem::create_directories("out/test_saves/health_dir_blocker");
    { std::ofstream blocker("out/test_saves/health_dir_blocker/blocker"); blocker << "x"; }

    Fixture f("health_dir", "out/test_saves/health_dir_blocker/blocker/sub/");
    ASSERT_TRUE(f.Boot());
    EXPECT_EQ(StatusOf(f), HealthStatus::kFailing);
    EXPECT_EQ(ReasonOf(f), StringCRC("save_dir_unavailable"));
    EXPECT_GE(f.module->GetHealthReporter().Report().errors, 1u);
    f.app.reset();
    std::filesystem::remove_all("out/test_saves/health_dir_blocker");
}
