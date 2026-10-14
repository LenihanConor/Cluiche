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
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <Modules/SaveLoadModule.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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

    uint32_t version = 1;
    uint32_t GetVersion() const override { return version; }

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

// ===========================================================================
// Edge cases (slot ids, registry misuse, migrations, atomicity, ordering, restarts)
// ===========================================================================

namespace {

std::string ReadAllBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

Json::Value ParseJson(const std::string& text)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(text, root);
    return root;
}

// The save file with the manifest timestamp removed, as canonical text.
std::string NormalizedSave(const std::string& path)
{
    Json::Value root = ParseJson(ReadAllBytes(path));
    if (root.isMember("manifest"))
        root["manifest"].removeMember("timestamp");
    return root.toStyledString();
}

size_t CountEntries(const std::string& dir)
{
    size_t n = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) { (void)e; ++n; }
    return n;
}

void WriteBytes(const std::string& path, const std::string& bytes)
{
    std::ofstream out(path, std::ios::binary);
    out << bytes;
}

} // namespace

// ---- H4: slot id validation at the module boundary ----

TEST(SaveLoadSlotIds, InvalidIdsAreRejectedByRequestsAndCommandsAndWriteNothing)
{
    Dia::API::Initialize();
    {
        Fixture f("bad_ids");
        ASSERT_TRUE(f.Boot());
        FakeSaveable a("A");
        f.module->RegisterParticipant(StringCRC("part_a"), &a);

        const char* const kBad[] = { "../x", "a/b", "a\\b", "", "..", ".hidden", "x/../y", "CON", "a:b", "C:\\x", "a b" };
        for (const char* id : kBad)
        {
            EXPECT_FALSE(f.module->RequestSave(StringCRC(id))) << "id: " << id;
            EXPECT_FALSE(f.module->RequestLoad(StringCRC(id))) << "id: " << id;
            EXPECT_FALSE(f.module->HasPendingSave()) << "id: " << id;
            EXPECT_FALSE(f.module->HasPendingLoad()) << "id: " << id;

            Dia::API::CommandArgs args;
            args.positionalArgs.Add(id);
            EXPECT_NE(Dia::API::ExecuteCommand(StringCRC("cow.save"), args), 0) << "id: " << id;
            EXPECT_NE(Dia::API::ExecuteCommand(StringCRC("cow.load"), args), 0) << "id: " << id;
            EXPECT_FALSE(f.module->HasPendingSave()) << "id: " << id;
            EXPECT_FALSE(f.module->HasPendingLoad()) << "id: " << id;
        }

        f.Pump(5);
        EXPECT_EQ(f.module->GetSaveCount(), 0u);
        EXPECT_EQ(f.module->GetLoadCount(), 0u);
        EXPECT_EQ(CountEntries(f.dir), 0u) << "nothing may be written, not even inside the save directory";
        // Canaries: where a traversal / absolute-path id would have landed.
        EXPECT_FALSE(std::filesystem::exists("out/test_saves/x.sav"));
        EXPECT_FALSE(std::filesystem::exists("out/test_saves/x"));
        EXPECT_FALSE(std::filesystem::exists(f.dir + "slot_../x.sav"));
        EXPECT_FALSE(std::filesystem::exists(f.dir + "slot_a/b.sav"));

        // Valid ids still work.
        EXPECT_TRUE(f.module->RequestSave(StringCRC("ok-1_x.v2")));
        EXPECT_TRUE(f.module->HasPendingSave());
        ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    }
    Dia::API::Shutdown();
}

// ---- H1 at module level: a failed write never damages the previous save ----

TEST(SaveLoadAtomicity, FailedWriteLeavesPreviousSaveLoadableAndByteIdentical)
{
    Fixture f("atomic");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 11;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("keep"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    const std::string before = ReadAllBytes(f.SlotPath("keep"));
    ASSERT_FALSE(before.empty());

    // A directory squatting on the temp path makes the write stage fail before the replace.
    std::filesystem::create_directories(f.SlotPath("keep") + ".tmp");
    a.value = 22;
    f.module->RequestSave(StringCRC("keep"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    EXPECT_FALSE(f.module->GetLastSaveResult().Ok());
    EXPECT_EQ(f.module->GetLastSaveResult().code, SaveResultCode::FileWriteError);
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
    EXPECT_EQ(f.module->GetSaveCount(), 1u);
    EXPECT_EQ(ReadAllBytes(f.SlotPath("keep")), before);

    a.value = -5;
    f.module->RequestLoad(StringCRC("keep"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_TRUE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.value, 11);
}

// ---- H5: save -> load -> save does not drift ----

TEST(SaveLoadDrift, RepeatedSaveLoadCyclesProduceIdenticalFilesExceptTimestamp)
{
    Fixture f("drift");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A"), b("B");
    a.value = 42; std::snprintf(a.text, sizeof(a.text), "%s", "hello");
    b.value = -7; std::snprintf(b.text, sizeof(b.text), "%s", "world");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RegisterParticipant(StringCRC("part_b"), &b);

    const char* const slots[] = { "d1", "d2", "d3" };
    for (int i = 0; i < 3; ++i)
    {
        f.module->RequestSave(StringCRC(slots[i]));
        ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
        ASSERT_TRUE(f.module->GetLastSaveResult().Ok());
        // Scramble, then restore from the file just written.
        a.value = 0; b.value = 0; a.text[0] = b.text[0] = '\0';
        f.module->RequestLoad(StringCRC(slots[i]));
        ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
        ASSERT_TRUE(f.module->GetLastLoadResult().Ok());
    }
    const std::string first = NormalizedSave(f.SlotPath("d1"));
    EXPECT_EQ(NormalizedSave(f.SlotPath("d2")), first);
    EXPECT_EQ(NormalizedSave(f.SlotPath("d3")), first);
    EXPECT_EQ(a.value, 42);
    EXPECT_STREQ(b.text, "world");
}

// ---- H6 / migrations: nothing is restored when a later participant cannot load ----

namespace {
struct TwoFakes
{
    explicit TwoFakes(Fixture& fx) : f(fx), a("A"), b("B")
    {
        a.value = 1; b.value = 2;
        f.module->RegisterParticipant(StringCRC("part_a"), &a);
        f.module->RegisterParticipant(StringCRC("part_b"), &b);
    }
    // Saves with b at `savedVersion`, scrambles both, then sets b's live version.
    bool SaveThenChangeVersion(uint32_t savedVersion, uint32_t liveVersion)
    {
        b.version = savedVersion;
        f.module->RequestSave(StringCRC("mig"));
        if (!f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }))
            return false;
        a.value = 100; b.value = 200;
        b.version = liveVersion;
        return true;
    }
    LoadResult Load()
    {
        f.module->RequestLoad(StringCRC("mig"));
        f.PumpUntil([&] { return !f.module->HasPendingLoad(); });
        return f.module->GetLastLoadResult();
    }
    void ExpectUntouched()
    {
        EXPECT_EQ(a.deserializeCount, 0);
        EXPECT_EQ(b.deserializeCount, 0);
        EXPECT_EQ(a.value, 100);
        EXPECT_EQ(b.value, 200);
        EXPECT_EQ(f.module->GetLoadCount(), 0u);
    }
    Fixture& f;
    FakeSaveable a, b;
};
}

TEST(SaveLoadMigration, MissingMigrationLeavesAllParticipantsUntouched)
{
    Fixture f("mig_missing");
    ASSERT_TRUE(f.Boot());
    TwoFakes t(f);
    ASSERT_TRUE(t.SaveThenChangeVersion(1, 2));
    EXPECT_EQ(t.Load().code, LoadResultCode::MigrationError);
    t.ExpectUntouched();
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
}

TEST(SaveLoadMigration, MissingFirstStepOfMultiStepChainLeavesAllUntouched)
{
    Fixture f("mig_chain");
    ASSERT_TRUE(f.Boot());
    TwoFakes t(f);
    ASSERT_TRUE(t.SaveThenChangeVersion(1, 3));
    f.module->GetRegistry().RegisterMigration(StringCRC("part_b"), 2, [](LoadContext&) {});   // 1->2 missing
    EXPECT_EQ(t.Load().code, LoadResultCode::MigrationError);
    t.ExpectUntouched();
}

TEST(SaveLoadMigration, RegisteredMigrationMakesTheLoadSucceed)
{
    Fixture f("mig_ok");
    ASSERT_TRUE(f.Boot());
    TwoFakes t(f);
    ASSERT_TRUE(t.SaveThenChangeVersion(1, 3));
    int ran = 0;
    f.module->GetRegistry().RegisterMigration(StringCRC("part_b"), 1, [&](LoadContext&) { ++ran; });
    f.module->GetRegistry().RegisterMigration(StringCRC("part_b"), 2, [&](LoadContext&) { ++ran; });
    EXPECT_TRUE(t.Load().Ok());
    EXPECT_EQ(ran, 2);
    EXPECT_EQ(t.a.value, 1);
    EXPECT_EQ(t.b.value, 2);
    EXPECT_EQ(t.a.deserializeCount, 1);
    EXPECT_EQ(t.b.deserializeCount, 1);
}

TEST(SaveLoadMigration, SaveNewerThanLiveParticipantIsRejectedBeforeAnyRestore)
{
    Fixture f("mig_downgrade");
    ASSERT_TRUE(f.Boot());
    TwoFakes t(f);
    ASSERT_TRUE(t.SaveThenChangeVersion(3, 2));   // file has v3, this build reads v2
    EXPECT_EQ(t.Load().code, LoadResultCode::MigrationError);
    t.ExpectUntouched();
}

// ---- M1: ordering of requests inside one frame ----

TEST(SaveLoadOrdering, TwoSaveRequestsBeforeOneUpdateWriteOnlyTheLaterSlot)
{
    Fixture f("two_saves");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    EXPECT_TRUE(f.module->RequestSave(StringCRC("first")));
    EXPECT_TRUE(f.module->RequestSave(StringCRC("second")));
    f.Pump(5);
    EXPECT_EQ(f.module->GetSaveCount(), 1u);
    EXPECT_FALSE(f.SlotFileExists("first"));
    EXPECT_TRUE(f.SlotFileExists("second"));
}

TEST(SaveLoadOrdering, SaveAndLoadInTheSameFrameRunSaveFirstThenLoad)
{
    Fixture f("save_then_load");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    a.value = 10;
    f.module->RequestSave(StringCRC("base"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));

    a.value = 20;
    // Requested in the "wrong" order on purpose: the module always runs the save before the load.
    EXPECT_TRUE(f.module->RequestLoad(StringCRC("base")));
    EXPECT_TRUE(f.module->RequestSave(StringCRC("after")));
    f.Pump(1);
    EXPECT_FALSE(f.module->HasPendingSave());
    EXPECT_FALSE(f.module->HasPendingLoad());
    EXPECT_EQ(f.module->GetSaveCount(), 2u);
    EXPECT_EQ(f.module->GetLoadCount(), 1u);

    // "after" captured the pre-load state (20), and the load then restored "base" (10).
    EXPECT_EQ(ParseJson(ReadAllBytes(f.SlotPath("after")))["part_a"]["value"].asInt(), 20);
    EXPECT_EQ(a.value, 10);
}

// ---- M2: buffer limits ----

namespace {
struct SizedSaveable : ISaveable
{
    std::string text;
    int deserializeCount = 0;
    void Serialize(SaveContext& ctx) const override { ctx.Write(StringCRC("blob"), text.c_str()); }
    void Deserialize(LoadContext&) override { ++deserializeCount; }
    uint32_t GetVersion() const override { return 1; }
};
}

TEST(SaveLoadBufferLimits, OversizedSaveFailsWithSerializeErrorAndKeepsPreviousSave)
{
    Fixture f("oversize_save");
    ASSERT_TRUE(f.Boot());
    SizedSaveable big;
    big.text = "small";
    f.module->RegisterParticipant(StringCRC("big"), &big);

    f.module->RequestSave(StringCRC("grow"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    const std::string before = ReadAllBytes(f.SlotPath("grow"));

    big.text.assign(SaveManager::kBufferSize + 1024, 'x');
    f.module->RequestSave(StringCRC("grow"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingSave(); }));
    EXPECT_EQ(f.module->GetLastSaveResult().code, SaveResultCode::SerializeError);
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
    EXPECT_EQ(f.module->GetSaveCount(), 1u);
    EXPECT_EQ(ReadAllBytes(f.SlotPath("grow")), before);
}

TEST(SaveLoadBufferLimits, HandPlacedFileLargerThanBufferFailsLoadCleanly)
{
    Fixture f("oversize_load");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 3;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("src"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    // A perfectly valid save followed by trailing whitespace past the limit: only size can reject it.
    std::string padded = ReadAllBytes(f.SlotPath("src"));
    padded.append(SaveManager::kBufferSize, ' ');
    WriteBytes(f.SlotPath("huge"), padded);

    a.value = 9;
    f.module->RequestLoad(StringCRC("huge"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_FALSE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(a.value, 9);
    EXPECT_EQ(StatusOf(f), HealthStatus::kDegraded);
}

// ---- M3: file / registry mismatches ----

TEST(SaveLoadMismatch, UnknownParticipantKeyInFileIsIgnored)
{
    Fixture f("unknown_key");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 5;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RequestSave(StringCRC("src"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));

    Json::Value root = ParseJson(ReadAllBytes(f.SlotPath("src")));
    root["ghost"]["value"] = 123;
    WriteBytes(f.SlotPath("ghosted"), root.toStyledString());

    a.value = 99;
    f.module->RequestLoad(StringCRC("ghosted"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_TRUE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.value, 5);
}

TEST(SaveLoadMismatch, RegisteredParticipantMissingFromFileIsLeftUnchangedOthersRestored)
{
    Fixture f("missing_key");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    a.value = 5;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RequestSave(StringCRC("only_a"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));

    FakeSaveable b("B");
    b.value = 77;
    f.module->RegisterParticipant(StringCRC("part_b"), &b);   // joined after the save was made

    a.value = 99;
    f.module->RequestLoad(StringCRC("only_a"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_TRUE(f.module->GetLastLoadResult().Ok());
    EXPECT_EQ(a.value, 5);
    EXPECT_EQ(b.deserializeCount, 0);
    EXPECT_EQ(b.value, 77);
}

// ---- M4: registry misuse is never fatal ----

TEST(SaveLoadRegistry, DuplicateRegisterReturnsFalseAndOriginalKeepsWorking)
{
    Fixture f("dup_register");
    ASSERT_TRUE(f.Boot());
    std::vector<std::string> order;
    g_order = &order;
    FakeSaveable first("first"), second("second");
    EXPECT_TRUE(f.module->RegisterParticipant(StringCRC("p"), &first));
    EXPECT_FALSE(f.module->RegisterParticipant(StringCRC("p"), &second));
    EXPECT_EQ(f.module->GetRegistry().GetParticipantCount(), 1u);

    first.value = 8;
    f.module->RequestSave(StringCRC("s"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    g_order = nullptr;
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], "first");

    first.value = 0;
    f.module->RequestLoad(StringCRC("s"));
    ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
    EXPECT_EQ(first.value, 8);
    EXPECT_EQ(second.deserializeCount, 0);
}

TEST(SaveLoadRegistry, NullParticipantIsRejected)
{
    Fixture f("null_register");
    ASSERT_TRUE(f.Boot());
    EXPECT_FALSE(f.module->RegisterParticipant(StringCRC("p"), nullptr));
    EXPECT_EQ(f.module->GetRegistry().GetParticipantCount(), 0u);
}

TEST(SaveLoadRegistry, RegisterBeyondCapacityReturnsFalse)
{
    Fixture f("full_register");
    ASSERT_TRUE(f.Boot());
    std::vector<std::unique_ptr<FakeSaveable>> fakes;
    for (unsigned i = 0; i < SaveRegistry::kMaxParticipants; ++i)
    {
        fakes.push_back(std::make_unique<FakeSaveable>("x"));
        char id[16];
        std::snprintf(id, sizeof(id), "p%u", i);
        ASSERT_TRUE(f.module->RegisterParticipant(StringCRC(id), fakes.back().get())) << i;
    }
    FakeSaveable extra("extra");
    EXPECT_FALSE(f.module->RegisterParticipant(StringCRC("one_too_many"), &extra));
    EXPECT_EQ(f.module->GetRegistry().GetParticipantCount(), SaveRegistry::kMaxParticipants);

    // Still functional at capacity.
    f.module->RequestSave(StringCRC("full"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
}

TEST(SaveLoadRegistry, UnregisterUnknownIdIsANoOp)
{
    Fixture f("unreg_unknown");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a("A");
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->UnregisterParticipant(StringCRC("never_registered"));
    EXPECT_EQ(f.module->GetRegistry().GetParticipantCount(), 1u);
    f.module->UnregisterParticipant(StringCRC("part_a"));
    f.module->UnregisterParticipant(StringCRC("part_a"));   // second time: unknown, still fine
    EXPECT_EQ(f.module->GetRegistry().GetParticipantCount(), 0u);
}

// ---- M5: restart, base_directory forms, listing ----

TEST(SaveLoadRestart, RestartedApplicationReRegistersCommandsAndHasNothingPending)
{
    Dia::API::Initialize();
    {
        Fixture f("restart");
        ASSERT_TRUE(f.Boot());
        ASSERT_TRUE(f.module->RequestSave(StringCRC("stale")));
        ASSERT_TRUE(f.module->RequestLoad(StringCRC("stale")));

        // Stop with requests still pending, then start a brand new Application.
        f.app->RequestShutdown();
        for (int i = 0; i < 50 && f.app->Update(kDt); ++i) {}
        f.app.reset();
        f.module = nullptr;

        ASSERT_TRUE(f.Boot());
        EXPECT_FALSE(f.module->HasPendingSave());
        EXPECT_FALSE(f.module->HasPendingLoad());
        f.Pump(3);
        EXPECT_FALSE(f.SlotFileExists("stale"));
        EXPECT_EQ(f.module->GetSaveCount(), 0u);

        Dia::API::CommandArgs withSlot;
        withSlot.positionalArgs.Add("again");
        EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cow.save"), withSlot), 0);
        ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
        EXPECT_TRUE(f.SlotFileExists("again"));
    }
    Dia::API::Shutdown();
}

TEST(SaveLoadConfig, BaseDirectoryOverrideWorksWithAndWithoutTrailingSlash)
{
    const char* const kDirs[] = { "out/test_saves/basedir_a", "out/test_saves/basedir_b/" };
    for (const char* base : kDirs)
    {
        std::filesystem::remove_all(base);
        {
            Fixture f("basedir_fixture", base);
            ASSERT_TRUE(f.Boot());
            EXPECT_EQ(std::string(f.module->GetConfig().baseDirectory).back(), '/');
            FakeSaveable a("A");
            a.value = 6;
            f.module->RegisterParticipant(StringCRC("part_a"), &a);
            f.module->RequestSave(StringCRC("q"));
            ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; })) << base;
            std::string expected = base;
            if (expected.back() != '/') expected += '/';
            EXPECT_TRUE(std::filesystem::exists(expected + "slot_q.sav")) << base;
            a.value = 0;
            f.module->RequestLoad(StringCRC("q"));
            ASSERT_TRUE(f.PumpUntil([&] { return !f.module->HasPendingLoad(); }));
            EXPECT_EQ(a.value, 6) << base;
        }
        std::filesystem::remove_all(base);
    }
}

TEST(SaveLoadConfig, ListCommandShowsSlotIdsAsPassedToSave)
{
    Dia::API::Initialize();
    {
        Fixture f("list_cmd");
        ASSERT_TRUE(f.Boot());
        FakeSaveable a("A");
        f.module->RegisterParticipant(StringCRC("part_a"), &a);
        const char* const ids[] = { "Alpha", "beta-2" };
        unsigned want = 0;
        for (const char* id : ids)
        {
            f.module->RequestSave(StringCRC(id));
            ++want;
            ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == want; }));
        }

        Dia::API::CommandArgs none;
        testing::internal::CaptureStdout();
        const int rc = Dia::API::ExecuteCommand(StringCRC("cow.save.list"), none);
        const std::string out = testing::internal::GetCapturedStdout();
        EXPECT_EQ(rc, 0);
        EXPECT_NE(out.find("2 save slot(s)"), std::string::npos) << out;
        EXPECT_NE(out.find("Alpha"), std::string::npos) << out;
        EXPECT_NE(out.find("beta-2"), std::string::npos) << out;
    }
    Dia::API::Shutdown();
}
