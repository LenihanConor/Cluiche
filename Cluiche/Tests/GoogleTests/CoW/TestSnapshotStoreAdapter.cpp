// Tests for CoW::SnapshotStoreAdapter and SaveLoadModule::CaptureState, over a real SaveLoadModule
// running in an Application (30 Hz SimPU) with a temp base directory under out/test_saves/.
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <Modules/SaveLoadModule.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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

struct FakeSaveable : ISaveable
{
    int32_t value = 0;
    int     serializeCount = 0;
    int     deserializeCount = 0;
    // Simulates a user request arriving mid-save: while > 0, each Serialize re-requests a save.
    SaveLoadModule* requestModule = nullptr;
    int             rerequests = 0;

    void Serialize(SaveContext& ctx) const override
    {
        const_cast<FakeSaveable*>(this)->serializeCount++;
        ctx.Write(StringCRC("value"), value);
        if (requestModule && rerequests != 0)
        {
            requestModule->RequestSave(StringCRC("user"));
            if (rerequests > 0)
                const_cast<FakeSaveable*>(this)->rerequests--;
        }
    }
    void Deserialize(LoadContext& ctx) override { ++deserializeCount; ctx.Read(StringCRC("value"), value); }
    uint32_t GetVersion() const override { return 1; }
};

struct Result
{
    int  calls = 0;
    bool ok = false;
    char message[192] = {};
};
void OnDone(bool ok, const char* message, void* user)
{
    Result* r = static_cast<Result*>(user);
    ++r->calls;
    r->ok = ok;
    std::snprintf(r->message, sizeof(r->message), "%s", message ? message : "");
}

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    SaveLoadModule* module = nullptr;
    std::string dir;
    std::string configJson;

    explicit Fixture(const char* testName)
    {
        dir = std::string("out/test_saves/snapadapter_") + testName + "/";
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
        configJson = std::string("{\"base_directory\":\"") + dir + "\"}";
        decl.configJson = configJson.c_str();
        pu.modules.Add(decl);
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

    bool Boot()
    {
        g_module = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start()) return false;
        module = g_module;
        if (!module) return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (module->GetState() == ModuleState::kActive) return true;
        }
        return false;
    }

    template <class Pred> bool PumpUntil(Pred done, int maxFrames = 20)
    {
        for (int i = 0; i < maxFrames && !done(); ++i) app->Update(kDt);
        return done();
    }
    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }

    std::string SnapPath(const char* name) const { return dir + "snapshots/" + name + ".sav"; }

    // Files under the base directory (recursive), as sorted relative strings.
    std::vector<std::string> Listing() const
    {
        std::vector<std::string> out;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(dir, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
            out.push_back(it->path().generic_string());
        std::sort(out.begin(), out.end());
        return out;
    }
};

} // namespace

TEST(SnapshotStoreAdapter, TakeThenRestoreRoundTrip)
{
    Fixture f("roundtrip");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 42;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    Result take;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &take);
    EXPECT_EQ(take.calls, 0) << "callback must wait for the frame boundary";
    ASSERT_TRUE(f.PumpUntil([&] { return take.calls > 0; }));
    EXPECT_TRUE(take.ok) << take.message;
    EXPECT_TRUE(std::filesystem::exists(f.SnapPath("s1")));
    EXPECT_TRUE(f.module->GetSnapshotStore().Exists("s1"));

    a.value = -1;
    Result restore;
    f.module->GetSnapshotStore().RequestRestore("s1", &OnDone, &restore);
    ASSERT_TRUE(f.PumpUntil([&] { return restore.calls > 0; }));
    EXPECT_TRUE(restore.ok) << restore.message;
    EXPECT_EQ(a.value, 42);
    EXPECT_EQ(take.calls, 1);
    EXPECT_EQ(restore.calls, 1);
    EXPECT_EQ(f.module->GetSnapshotTakeCount(), 1u);
    EXPECT_EQ(f.module->GetSnapshotRestoreCount(), 1u);
    EXPECT_EQ(f.module->GetSaveCount(), 0u) << "snapshots are not slot saves";
    EXPECT_EQ(f.module->GetLoadCount(), 0u);
}

TEST(SnapshotStoreAdapter, InvalidNamesAreRejectedAndTouchNoDisk)
{
    Fixture f("badnames");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 7;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    const std::vector<std::string> before = f.Listing();

    const std::string tooLong(64, 'x');
    const char* names[] = { "", "../escape", "a/b", "a\\b", "..", "a.b", "has space", "CON", "NUL", tooLong.c_str() };
    for (const char* n : names)
    {
        EXPECT_FALSE(SnapshotStoreAdapter::IsValidName(n)) << "'" << n << "'";
        EXPECT_FALSE(f.module->GetSnapshotStore().Exists(n));
        EXPECT_FALSE(f.module->GetSnapshotStore().Delete(n));

        Result take, restore;
        f.module->GetSnapshotStore().RequestTake(n, &OnDone, &take);
        f.module->GetSnapshotStore().RequestRestore(n, &OnDone, &restore);
        ASSERT_TRUE(f.PumpUntil([&] { return take.calls > 0 && restore.calls > 0; })) << "'" << n << "'";
        EXPECT_FALSE(take.ok);
        EXPECT_FALSE(restore.ok);
        EXPECT_EQ(take.calls, 1);
        EXPECT_EQ(restore.calls, 1);
    }
    EXPECT_EQ(f.Listing(), before);
    EXPECT_EQ(a.value, 7);
    EXPECT_TRUE(SnapshotStoreAdapter::IsValidName("ok_name-1"));
    EXPECT_TRUE(SnapshotStoreAdapter::IsValidName(std::string(63, 'x').c_str()));
}

TEST(SnapshotStoreAdapter, MissingAndCorruptRestoreLeaveGameUntouched)
{
    Fixture f("badfiles");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 5;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    Result missing;
    f.module->GetSnapshotStore().RequestRestore("nothere", &OnDone, &missing);
    ASSERT_TRUE(f.PumpUntil([&] { return missing.calls > 0; }));
    EXPECT_FALSE(missing.ok);
    EXPECT_NE(std::string(missing.message).find("not found"), std::string::npos) << missing.message;

    std::filesystem::create_directories(f.dir + "snapshots");
    { std::ofstream(f.SnapPath("junk")) << "{ this is not json"; }
    Result corrupt;
    f.module->GetSnapshotStore().RequestRestore("junk", &OnDone, &corrupt);
    ASSERT_TRUE(f.PumpUntil([&] { return corrupt.calls > 0; }));
    EXPECT_FALSE(corrupt.ok);

    { std::ofstream(f.SnapPath("nomanifest")) << "{ \"part_a\": { \"value\": 99 } }"; }
    Result noManifest;
    f.module->GetSnapshotStore().RequestRestore("nomanifest", &OnDone, &noManifest);
    ASSERT_TRUE(f.PumpUntil([&] { return noManifest.calls > 0; }));
    EXPECT_FALSE(noManifest.ok);

    EXPECT_EQ(a.value, 5);
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(f.module->GetSnapshotFailureCount(), 3u);
    EXPECT_EQ(f.module->GetSnapshotRestoreCount(), 0u);
}

TEST(SnapshotStoreAdapter, DefersBehindPendingUserSaveAndNeverOverwritesIt)
{
    Fixture f("defer");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 1;
    a.requestModule = f.module;
    a.rerequests = 1;   // the first save re-requests "user" mid-update, so a save is pending when the snapshot runs
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("user"));
    Result take;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &take);
    f.Pump(1);   // user save runs, re-requests itself; the snapshot must wait
    EXPECT_EQ(take.calls, 0);
    EXPECT_TRUE(f.module->HasPendingSave()) << "the user's request must not be consumed or overwritten";

    ASSERT_TRUE(f.PumpUntil([&] { return take.calls > 0; }));
    EXPECT_TRUE(take.ok) << take.message;
    EXPECT_EQ(f.module->GetSaveCount(), 2u) << "both user saves ran";
    EXPECT_FALSE(f.module->HasPendingSave());
}

TEST(SnapshotStoreAdapter, DeferralExpiresAfterTheLimit)
{
    Fixture f("expire");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a;
    a.requestModule = f.module;
    a.rerequests = -1;   // every save re-requests another: a pending save never clears
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    f.module->RequestSave(StringCRC("user"));
    Result take;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &take);
    ASSERT_FALSE(f.PumpUntil([&] { return take.calls > 0; }, static_cast<int>(SnapshotStoreAdapter::kDeferLimitFrames) - 5));
    ASSERT_TRUE(f.PumpUntil([&] { return take.calls > 0; }, 20));
    EXPECT_FALSE(take.ok);
    EXPECT_EQ(take.calls, 1);
    EXPECT_FALSE(std::filesystem::exists(f.SnapPath("s1")));
    EXPECT_EQ(f.module->GetSnapshotFailureCount(), 1u);

    a.rerequests = 0;   // stop the chain; the adapter is usable again
    f.Pump(3);
    Result retry;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &retry);
    ASSERT_TRUE(f.PumpUntil([&] { return retry.calls > 0; }));
    EXPECT_TRUE(retry.ok) << retry.message;
}

TEST(SnapshotStoreAdapter, SecondRequestWhileOnePendingIsRefusedBusyAndFirstStillRuns)
{
    Fixture f("busy");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 3;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    Result first, second;
    f.module->GetSnapshotStore().RequestTake("one", &OnDone, &first);
    f.module->GetSnapshotStore().RequestTake("two", &OnDone, &second);
    ASSERT_TRUE(f.PumpUntil([&] { return first.calls > 0 && second.calls > 0; }));
    EXPECT_TRUE(first.ok);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(first.calls, 1);
    EXPECT_EQ(second.calls, 1);
    EXPECT_TRUE(std::filesystem::exists(f.SnapPath("one")));
    EXPECT_FALSE(std::filesystem::exists(f.SnapPath("two")));
}

TEST(SnapshotStoreAdapter, SnapshotsAreOutsideTheSlotsAndTheSlotCap)
{
    Fixture f("slots");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    const uint32_t cap = f.module->GetConfig().maxSlots;
    ASSERT_GT(cap, 0u);
    const uint32_t count = cap + 8;   // more than the slot cap
    for (uint32_t i = 0; i < count; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "snap%u", i);
        Result r;
        f.module->GetSnapshotStore().RequestTake(name, &OnDone, &r);
        ASSERT_TRUE(f.PumpUntil([&] { return r.calls > 0; }));
        ASSERT_TRUE(r.ok) << name << ": " << r.message;
    }

    Dia::Core::Containers::DynamicArrayC<SlotInfo, 32> slots;
    f.module->EnumerateSlots(slots);
    EXPECT_EQ(slots.Size(), 0u);

    // The slot cap still applies to user slots, and the config was restored after each snapshot.
    EXPECT_EQ(f.module->GetConfig().maxSlots, cap);
    EXPECT_STREQ(f.module->GetConfig().slotPattern, "slot_{id}.sav");
    f.module->RequestSave(StringCRC("user"));
    ASSERT_TRUE(f.PumpUntil([&] { return f.module->GetSaveCount() == 1; }));
    EXPECT_TRUE(std::filesystem::exists(f.dir + "slot_user.sav"));
}

TEST(SnapshotStoreAdapter, ExistsDeleteAndFolder)
{
    Fixture f("files");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    Dia::CellHistory::ISnapshotStore& store = f.module->GetSnapshotStore();

    EXPECT_EQ(std::string(store.Folder()), f.dir + "snapshots");
    EXPECT_FALSE(store.Exists("s1"));
    Result r;
    store.RequestTake("s1", &OnDone, &r);
    ASSERT_TRUE(f.PumpUntil([&] { return r.calls > 0; }));
    EXPECT_TRUE(store.Exists("s1"));
    EXPECT_TRUE(store.Delete("s1"));
    EXPECT_FALSE(store.Exists("s1"));
    EXPECT_FALSE(store.Delete("s1"));
}

TEST(SnapshotStoreAdapter, FailureDegradesHealthAndNextSuccessClearsIt)
{
    using Dia::Observation::Health::HealthStatus;
    Fixture f("health");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);

    Result bad;
    f.module->GetSnapshotStore().RequestRestore("nothere", &OnDone, &bad);
    ASSERT_TRUE(f.PumpUntil([&] { return bad.calls > 0; }));
    EXPECT_EQ(f.module->GetHealthReporter().Report().status, HealthStatus::kDegraded);
    EXPECT_EQ(f.module->GetHealthReporter().Report().reason, StringCRC("last_snapshot_failed"));

    Result good;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &good);
    ASSERT_TRUE(f.PumpUntil([&] { return good.calls > 0; }));
    EXPECT_EQ(f.module->GetHealthReporter().Report().status, HealthStatus::kOK);
}

TEST(SaveLoadModuleCaptureState, ReturnsParticipantsInRegistrationOrderWithoutSideEffects)
{
    Fixture f("capture");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a, b;
    a.value = 11;
    b.value = 22;
    f.module->RegisterParticipant(StringCRC("part_b"), &b);
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    f.module->RequestSave(StringCRC("user"));   // pending flag must survive the capture
    const std::vector<std::string> before = f.Listing();

    CapturedStateList list;
    f.module->CaptureState(list);

    ASSERT_EQ(list.Size(), 2u);
    EXPECT_EQ(list[0].id, StringCRC("part_b"));
    EXPECT_EQ(list[0].state["value"].asInt(), 22);
    EXPECT_EQ(list[1].id, StringCRC("part_a"));
    EXPECT_EQ(list[1].state["value"].asInt(), 11);

    EXPECT_EQ(f.Listing(), before) << "no disk write";
    EXPECT_TRUE(f.module->HasPendingSave());
    EXPECT_EQ(f.module->GetSaveCount(), 0u);
    EXPECT_EQ(f.module->GetLoadCount(), 0u);
    EXPECT_EQ(a.deserializeCount, 0);
    EXPECT_EQ(a.value, 11);

    // Stable across calls; empty registry gives an empty list.
    CapturedStateList again;
    f.module->CaptureState(again);
    ASSERT_EQ(again.Size(), 2u);
    EXPECT_EQ(again[0].state, list[0].state);

    f.module->UnregisterParticipant(StringCRC("part_a"));
    f.module->UnregisterParticipant(StringCRC("part_b"));
    f.module->CaptureState(again);
    EXPECT_EQ(again.Size(), 0u);
}

TEST(SnapshotStoreAdapter, QueuedRequestGetsFailureCallbackWhenModuleStops)
{
    Result take;
    {
        Fixture f("stop");
        ASSERT_TRUE(f.Boot());
        f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &take);
        f.app->RequestShutdown();
        for (int i = 0; i < 50 && f.app->Update(kDt); ++i) {}
    }
    EXPECT_EQ(take.calls, 1);
    EXPECT_FALSE(take.ok);
}

TEST(SnapshotStoreAdapter, EmptyParticipantListTakesAndRestores)
{
    Fixture f("empty");
    ASSERT_TRUE(f.Boot());
    Result take, restore;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &take);
    ASSERT_TRUE(f.PumpUntil([&] { return take.calls > 0; }));
    EXPECT_TRUE(take.ok) << take.message;
    f.module->GetSnapshotStore().RequestRestore("s1", &OnDone, &restore);
    ASSERT_TRUE(f.PumpUntil([&] { return restore.calls > 0; }));
    EXPECT_TRUE(restore.ok) << restore.message;
}

TEST(SnapshotStoreAdapter, TakeOverExistingNameOverwritesIt)
{
    Fixture f("overwrite");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 1;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    Result r1, r2, rr;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &r1);
    ASSERT_TRUE(f.PumpUntil([&] { return r1.calls > 0; }));
    a.value = 2;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &r2);
    ASSERT_TRUE(f.PumpUntil([&] { return r2.calls > 0; }));
    EXPECT_TRUE(r2.ok);
    a.value = 99;
    f.module->GetSnapshotStore().RequestRestore("s1", &OnDone, &rr);
    ASSERT_TRUE(f.PumpUntil([&] { return rr.calls > 0; }));
    EXPECT_EQ(a.value, 2);
}

TEST(SnapshotStoreAdapter, RestoreDefersBehindPendingUserLoadAndLoadRunsFirst)
{
    Fixture f("loaddefer");
    ASSERT_TRUE(f.Boot());
    FakeSaveable a; a.value = 10;
    f.module->RegisterParticipant(StringCRC("part_a"), &a);
    Result snap;
    f.module->GetSnapshotStore().RequestTake("s1", &OnDone, &snap);
    f.module->RequestSave(StringCRC("u"));
    ASSERT_TRUE(f.PumpUntil([&] { return snap.calls > 0 && f.module->GetSaveCount() == 1; }));

    a.value = 20;
    f.module->RequestLoad(StringCRC("u"));   // user load pending alongside the snapshot restore
    Result restore;
    f.module->GetSnapshotStore().RequestRestore("s1", &OnDone, &restore);
    ASSERT_TRUE(f.PumpUntil([&] { return restore.calls > 0; }));
    EXPECT_TRUE(restore.ok) << restore.message;
    EXPECT_EQ(f.module->GetLoadCount(), 1u) << "the user's load was not dropped";
    EXPECT_EQ(a.value, 10);
}
