#include "Modules/SaveLoadModule.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaObservation/Metric/MetricInfo.h>
#include <DiaSaveGame/SaveManifest.h>

#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <memory>
#include <system_error>

namespace CoW {

const Dia::Core::StringCRC SaveLoadModule::kTypeId("SaveLoadModule");

namespace {
    using Dia::API::CommandArgs;
    using Dia::API::CommandInfo;
    using Dia::SaveGame::LoadResult;
    using Dia::SaveGame::LoadResultCode;
    using Dia::SaveGame::SaveResult;
    using Dia::SaveGame::SaveResultCode;
    using Dia::Observation::Metric::MetricRegistry;

    const Dia::Core::StringCRC kCmdSave("cow.save");
    const Dia::Core::StringCRC kCmdLoad("cow.load");
    const Dia::Core::StringCRC kCmdList("cow.save.list");

    const char* const kDefaultBaseDirectory = "out/saves/cow/";
}

Dia::SaveGame::SaveConfig SaveLoadModule::DefaultConfig()
{
    Dia::SaveGame::SaveConfig config;
    config.baseDirectory = kDefaultBaseDirectory;
    config.slotPattern   = "slot_{id}.sav";
    config.maxSlots      = 8;
    config.format        = Dia::SaveGame::SaveFormat::Json;
    return config;
}

SaveLoadModule::SaveLoadModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    std::snprintf(mBaseDirectory, kBaseDirCapacity, "%s", kDefaultBaseDirectory);
    mConfig = DefaultConfig();
    mConfig.baseDirectory = mBaseDirectory;
    UpdateSnapshotPaths();
}

void SaveLoadModule::UpdateSnapshotPaths()
{
    std::snprintf(mSnapshotFolder, sizeof(mSnapshotFolder), "%ssnapshots", mBaseDirectory);
    std::snprintf(mSnapshotPrefix, sizeof(mSnapshotPrefix), "%ssnapshots/", mBaseDirectory);
}

void SaveLoadModule::BuildSnapshotPath(const char* name, char* out, uint32_t capacity) const
{
    std::snprintf(out, capacity, "%s%s.sav", mSnapshotPrefix, name);
}

SaveLoadModule::~SaveLoadModule()
{
    // Destroyed without a Stop: never leave a dangling reporter in the process-wide registry.
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
}

void SaveLoadModule::OnConfigure(const char* configJson)
{
    if (configJson == nullptr || configJson[0] == '\0')
        return;

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(configJson, root) || !root.isObject())
    {
        DIA_LOG_ERROR("CoW", "SaveLoadModule: config malformed, using defaults");
        return;
    }
    if (root.isMember("base_directory") && root["base_directory"].isString())
    {
        const std::string dir = root["base_directory"].asString();
        const bool needsSlash = !dir.empty() && dir.back() != '/' && dir.back() != '\\';
        std::snprintf(mBaseDirectory, kBaseDirCapacity, "%s%s", dir.c_str(), needsSlash ? "/" : "");
        UpdateSnapshotPaths();
    }
}

void SaveLoadModule::OnConnectStreams(Dia::ApplicationFlow::Application&)
{
    Dia::ApplicationFlow::ProcessingUnit* pu = GetProcessingUnit();
    if (pu == nullptr)
        return;
    // FindModule ignores lifecycle state (ModuleRef would not: the sibling is not Active yet).
    Dia::ApplicationFlow::Module* found = pu->FindModule(Dia::SimTime::DiaSimTimeModule::kTypeId);
    if (found == nullptr)
        return;
    static_cast<Dia::SimTime::DiaSimTimeModule*>(found)->SetSaveRegistry(mRegistry);
    DIA_LOG_INFO("CoW", "SaveLoadModule: save registry injected into DiaSimTimeModule");
}

bool SaveLoadModule::FindParticipant(Dia::Core::StringCRC id, uint32_t& outIndex) const
{
    for (uint32_t i = 0; i < mRegistry.GetParticipantCount(); ++i)
    {
        if (mRegistry.GetIdAt(i) == id)
        {
            outIndex = i;
            return true;
        }
    }
    return false;
}

bool SaveLoadModule::RegisterParticipant(Dia::Core::StringCRC id, Dia::SaveGame::ISaveable* participant)
{
    if (participant == nullptr)
    {
        DIA_LOG_ERROR("CoW", "SaveLoadModule: cannot register null participant '%s'", id.AsChar());
        return false;
    }
    uint32_t existing = 0;
    if (FindParticipant(id, existing))
    {
        DIA_LOG_ERROR("CoW", "SaveLoadModule: participant '%s' already registered, ignoring", id.AsChar());
        return false;
    }
    if (mRegistry.GetParticipantCount() >= Dia::SaveGame::SaveRegistry::kMaxParticipants)
    {
        DIA_LOG_ERROR("CoW", "SaveLoadModule: registry full (%u), cannot register '%s'",
                      static_cast<unsigned>(Dia::SaveGame::SaveRegistry::kMaxParticipants), id.AsChar());
        return false;
    }
    mRegistry.Register(id, participant);
    return true;
}

void SaveLoadModule::UnregisterParticipant(Dia::Core::StringCRC id)
{
    uint32_t existing = 0;
    if (!FindParticipant(id, existing))
    {
        DIA_LOG_WARNING("CoW", "SaveLoadModule: unregister of unknown participant '%s' ignored", id.AsChar());
        return;
    }
    mRegistry.Unregister(id);
}

bool SaveLoadModule::RequestSave(Dia::Core::StringCRC slotId)
{
    if (!Dia::SaveGame::SlotManager::IsValidSlotId(slotId.AsChar()))
    {
        DIA_LOG_WARNING("CoW", "SaveLoadModule: save rejected, invalid slot id '%s'", slotId.AsChar());
        return false;
    }
    mPendingSaveSlot = slotId;
    mHasPendingSave = true;
    return true;
}

bool SaveLoadModule::RequestLoad(Dia::Core::StringCRC slotId)
{
    if (!Dia::SaveGame::SlotManager::IsValidSlotId(slotId.AsChar()))
    {
        DIA_LOG_WARNING("CoW", "SaveLoadModule: load rejected, invalid slot id '%s'", slotId.AsChar());
        return false;
    }
    mPendingLoadSlot = slotId;
    mHasPendingLoad = true;
    return true;
}

void SaveLoadModule::EnumerateSlots(Dia::Core::Containers::DynamicArrayC<Dia::SaveGame::SlotInfo, 32>& out) const
{
    mSlots.EnumerateSlots(out);
}

void SaveLoadModule::RegisterCommands()
{
    CommandInfo save;
    save.name        = kCmdSave;
    save.description = "Queue a save to the named slot (runs at the next sim frame)";
    save.category    = Dia::Core::StringCRC("cow");
    save.owner       = "SaveLoadModule";
    save.version     = "1.0.0";
    save.example     = "cow.save quick";
    save.callback    = [this](const CommandArgs& args) -> int {
        if (args.positionalArgs.Size() < 1)
        {
            std::printf("usage: cow.save <slotId>\n");
            return 1;
        }
        if (!RequestSave(Dia::Core::StringCRC(args.positionalArgs[0])))
        {
            std::printf("error: invalid slot id '%s' (use letters, digits, '_', '-', '.')\n", args.positionalArgs[0]);
            return 1;
        }
        std::printf("Save queued\n");
        return 0;
    };

    CommandInfo load = save;
    load.name        = kCmdLoad;
    load.description = "Queue a load from the named slot (runs at the next sim frame)";
    load.example     = "cow.load quick";
    load.callback    = [this](const CommandArgs& args) -> int {
        if (args.positionalArgs.Size() < 1)
        {
            std::printf("usage: cow.load <slotId>\n");
            return 1;
        }
        if (!RequestLoad(Dia::Core::StringCRC(args.positionalArgs[0])))
        {
            std::printf("error: invalid slot id '%s' (use letters, digits, '_', '-', '.')\n", args.positionalArgs[0]);
            return 1;
        }
        std::printf("Load queued\n");
        return 0;
    };

    CommandInfo list = save;
    list.name        = kCmdList;
    list.description = "List save slots and their file sizes";
    list.example     = "cow.save.list";
    list.callback    = [this](const CommandArgs&) -> int {
        Dia::Core::Containers::DynamicArrayC<Dia::SaveGame::SlotInfo, 32> slots;
        EnumerateSlots(slots);
        std::printf("%u save slot(s)\n", static_cast<unsigned>(slots.Size()));
        for (unsigned int i = 0; i < slots.Size(); ++i)
        {
            std::error_code ec;
            const auto size = std::filesystem::file_size(slots[i].path, ec);
            char when[32] = "unknown";
            const time_t stamp = static_cast<time_t>(slots[i].timestamp);
            tm utc;
            if (slots[i].timestamp != 0 && gmtime_s(&utc, &stamp) == 0)
                std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S UTC", &utc);
            std::printf("  %s  %llu bytes  %s\n", slots[i].id.AsChar(),
                        static_cast<unsigned long long>(ec ? 0 : size), when);
        }
        return 0;
    };

    const bool ok = Dia::API::RegisterCommand(save)
                  & Dia::API::RegisterCommand(load)
                  & Dia::API::RegisterCommand(list);
    mCommandsRegistered = true;
    if (!ok)
        DIA_LOG_WARNING("CoW", "SaveLoadModule: one or more commands failed to register");
}

void SaveLoadModule::UnregisterCommands()
{
    if (!mCommandsRegistered)
        return;
    Dia::API::UnregisterCommand(kCmdSave);
    Dia::API::UnregisterCommand(kCmdLoad);
    Dia::API::UnregisterCommand(kCmdList);
    mCommandsRegistered = false;
}

Dia::ApplicationFlow::StartResult SaveLoadModule::DoStart()
{
    DIA_TRACE_ZONE("SaveLoadModule::DoStart", Dia::Observation::Trace::Category::kNone);

    auto& metrics = MetricRegistry::Instance();
    mSaveCounter     = metrics.RegisterCounter(Dia::Core::StringCRC("cow.saveload.saves"));
    mSaveFailCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.saveload.save_failures"));
    mLoadCounter     = metrics.RegisterCounter(Dia::Core::StringCRC("cow.saveload.loads"));
    mLoadFailCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.saveload.load_failures"));

    using Dia::Observation::Metric::MetricInfo;
    mSnapshotTakeCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.save.snapshot_takes"),
        MetricInfo{ "Snapshot takes", "Game-state snapshots taken", "count" });
    mSnapshotRestoreCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.save.snapshot_restores"),
        MetricInfo{ "Snapshot restores", "Game-state snapshots restored", "count" });
    mSnapshotFailCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.save.snapshot_failures"),
        MetricInfo{ "Snapshot failures", "Snapshot takes and restores that failed or were refused", "count" });

    std::error_code ec;
    std::filesystem::create_directories(mBaseDirectory, ec);
    mSaveDirFailed = static_cast<bool>(ec);
    if (ec)
    {
        mHealth.IncrementErrors();
        DIA_LOG_ERROR("CoW", "SaveLoadModule: cannot create '%s' (%s)", mBaseDirectory, ec.message().c_str());
    }

    mManager.Init(mConfig, mRegistry);
    mSlots.Init(mConfig);
    mStarted = true;
    mHasPendingSave = false;
    mHasPendingLoad = false;

    RegisterCommands();
    mConsecutiveSaveFailures = 0;
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;
    UpdateHealth();
    DIA_LOG_INFO("CoW", "SaveLoadModule: started, saving to '%s'", mBaseDirectory);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SaveLoadModule::UpdateHealth()
{
    if (mSaveDirFailed)
        mHealth.SetFailing(Dia::Core::StringCRC("save_dir_unavailable"));
    else if (mConsecutiveSaveFailures >= kFailingAfterConsecutiveSaveFailures)
        mHealth.SetFailing(Dia::Core::StringCRC("saves_repeatedly_failing"));
    else if (!mLastSaveResult.Ok())
        mHealth.SetDegraded(Dia::Core::StringCRC("last_save_failed"));
    else if (!mLastLoadResult.Ok())
        mHealth.SetDegraded(Dia::Core::StringCRC("last_load_failed"));
    else if (mLastSnapshotFailed)
        mHealth.SetDegraded(Dia::Core::StringCRC("last_snapshot_failed"));
    else
        mHealth.SetOK();
}

LoadResult SaveLoadModule::PreValidateLoad(Dia::Core::StringCRC slotId)
{
    if (!mSlots.SlotExists(slotId))
        return LoadResult::Fail(LoadResultCode::SlotNotFound);

    char path[Dia::SaveGame::SaveManager::kMaxPathLen];
    mSlots.BuildPath(slotId, path, sizeof(path));

    auto buffer = std::make_unique<char[]>(Dia::SaveGame::SaveManager::kBufferSize);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0)
        f = nullptr;
    if (!f)
        return LoadResult::Fail(LoadResultCode::FileReadError);
    const size_t bytes = std::fread(buffer.get(), 1, Dia::SaveGame::SaveManager::kBufferSize - 1, f);
    const bool tooLarge = (bytes == Dia::SaveGame::SaveManager::kBufferSize - 1) && (std::fgetc(f) != EOF);
    std::fclose(f);
    if (tooLarge)
        return LoadResult::Fail(LoadResultCode::ParseError);
    buffer[bytes] = '\0';

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(buffer.get(), root))
        return LoadResult::Fail(LoadResultCode::ParseError);

    Dia::SaveGame::LoadContext ctx(root);
    Dia::SaveGame::SaveManifest manifest;
    if (!manifest.Read(ctx))
        return LoadResult::Fail(LoadResultCode::ParseError);

    if (manifest.CheckCompatibility(mRegistry) == Dia::SaveGame::CompatResult::EngineMismatch)
        return LoadResult::Fail(LoadResultCode::EngineMismatch);

    // SaveManager::Load restores participants one by one, so a failure found midway would leave
    // the game half-restored. Check every participant's version path up front instead.
    for (uint32_t m = 0; m < manifest.GetParticipantCount(); ++m)
    {
        const Dia::SaveGame::ManifestParticipant& saved = manifest.GetParticipantAt(m);
        uint32_t live = 0;
        if (!FindParticipant(saved.id, live))
            continue;   // not registered: ignored by Load
        const uint32_t liveVersion = mRegistry.GetParticipantAt(live)->GetVersion();
        if (saved.version > liveVersion)
        {
            DIA_LOG_ERROR("CoW", "SaveLoadModule: slot '%s' has '%s' at v%u but this build only reads v%u (downgrade)",
                          slotId.AsChar(), saved.id.AsChar(), saved.version, liveVersion);
            return LoadResult::Fail(LoadResultCode::MigrationError);
        }
        for (uint32_t from = saved.version; from < liveVersion; ++from)
        {
            if (!mRegistry.HasMigration(saved.id, from))
            {
                DIA_LOG_ERROR("CoW", "SaveLoadModule: slot '%s' needs migration for '%s' v%u->v%u, none registered",
                              slotId.AsChar(), saved.id.AsChar(), from, from + 1);
                return LoadResult::Fail(LoadResultCode::MigrationError);
            }
        }
    }

    return LoadResult::Success();
}

void SaveLoadModule::ExecuteSave(Dia::Core::StringCRC slotId)
{
    mLastSaveResult = mManager.Save(slotId);
    if (mLastSaveResult.Ok())
    {
        ++mSaveCount;
        mConsecutiveSaveFailures = 0;
        mSaveCounter->Inc();
        DIA_LOG_INFO("CoW", "SaveLoadModule: saved slot '%s'", slotId.AsChar());
    }
    else
    {
        mSaveFailCounter->Inc();
        ++mConsecutiveSaveFailures;
        mHealth.IncrementErrors();
        DIA_LOG_ERROR("CoW", "SaveLoadModule: save of slot '%s' failed (SaveResultCode %d)",
                      slotId.AsChar(), static_cast<int>(mLastSaveResult.code));
    }
}

void SaveLoadModule::ExecuteLoad(Dia::Core::StringCRC slotId)
{
    mLastLoadResult = PreValidateLoad(slotId);
    if (mLastLoadResult.Ok())
        mLastLoadResult = mManager.Load(slotId);

    if (mLastLoadResult.Ok())
    {
        ++mLoadCount;
        mLoadCounter->Inc();
        DIA_LOG_INFO("CoW", "SaveLoadModule: loaded slot '%s'", slotId.AsChar());
    }
    else
    {
        mLoadFailCounter->Inc();
        mHealth.IncrementErrors();
        DIA_LOG_ERROR("CoW", "SaveLoadModule: load of slot '%s' failed (LoadResultCode %d), game state untouched",
                      slotId.AsChar(), static_cast<int>(mLastLoadResult.code));
    }
}

namespace {
    // Points the live config at the snapshot folder for one call: SaveManager and SlotManager hold a
    // pointer to the config and read it on every call, so no Dia change is needed. Restored on exit.
    class ScopedSnapshotConfig
    {
    public:
        ScopedSnapshotConfig(Dia::SaveGame::SaveConfig& config, const char* snapshotPrefix)
            : mConfig(config), mSaved(config)
        {
            config.baseDirectory = snapshotPrefix;
            config.slotPattern   = "{id}.sav";
            config.maxSlots      = 0;   // the slot cap never applies to snapshots
        }
        ~ScopedSnapshotConfig() { mConfig = mSaved; }
        ScopedSnapshotConfig(const ScopedSnapshotConfig&) = delete;
        ScopedSnapshotConfig& operator=(const ScopedSnapshotConfig&) = delete;
    private:
        Dia::SaveGame::SaveConfig& mConfig;
        Dia::SaveGame::SaveConfig  mSaved;
    };
}

void SaveLoadModule::NoteSnapshotFailure(bool isTake, const char* name, const char* reason)
{
    ++mSnapshotFailureCount;
    mLastSnapshotFailed = true;
    if (mSnapshotFailCounter)
        mSnapshotFailCounter->Inc();
    mHealth.IncrementErrors();
    DIA_LOG_ERROR("CoW", "SaveLoadModule: snapshot %s '%s' failed: %s", isTake ? "take" : "restore", name, reason);
    UpdateHealth();
}

bool SaveLoadModule::SnapshotSave(const char* name, char* message, uint32_t messageCapacity)
{
    if (!mStarted)
    {
        std::snprintf(message, messageCapacity, "save/load module is not started");
        NoteSnapshotFailure(true, name, message);
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(mSnapshotFolder, ec);
    if (ec)
    {
        std::snprintf(message, messageCapacity, "cannot create snapshot folder: %s", ec.message().c_str());
        NoteSnapshotFailure(true, name, message);
        return false;
    }

    SaveResult result = SaveResult::Success();
    {
        ScopedSnapshotConfig scoped(mConfig, mSnapshotPrefix);
        result = mManager.Save(Dia::Core::StringCRC(name));
    }
    if (!result.Ok())
    {
        std::snprintf(message, messageCapacity, "snapshot save failed (SaveResultCode %d)", static_cast<int>(result.code));
        NoteSnapshotFailure(true, name, message);
        return false;
    }

    ++mSnapshotTakeCount;
    mLastSnapshotFailed = false;
    if (mSnapshotTakeCounter)
        mSnapshotTakeCounter->Inc();
    UpdateHealth();
    DIA_LOG_INFO("CoW", "SaveLoadModule: snapshot '%s' taken", name);
    std::snprintf(message, messageCapacity, "ok");
    return true;
}

bool SaveLoadModule::SnapshotLoad(const char* name, char* message, uint32_t messageCapacity)
{
    if (!mStarted)
    {
        std::snprintf(message, messageCapacity, "save/load module is not started");
        NoteSnapshotFailure(false, name, message);
        return false;
    }

    LoadResult result = LoadResult::Success();
    {
        ScopedSnapshotConfig scoped(mConfig, mSnapshotPrefix);
        const Dia::Core::StringCRC id(name);
        result = PreValidateLoad(id);   // a bad file leaves the game untouched
        if (result.Ok())
            result = mManager.Load(id);
    }
    if (!result.Ok())
    {
        if (result.code == LoadResultCode::SlotNotFound)
            std::snprintf(message, messageCapacity, "snapshot '%s' not found", name);
        else
            std::snprintf(message, messageCapacity, "snapshot restore failed (LoadResultCode %d), game state untouched",
                          static_cast<int>(result.code));
        NoteSnapshotFailure(false, name, message);
        return false;
    }

    ++mSnapshotRestoreCount;
    mLastSnapshotFailed = false;
    if (mSnapshotRestoreCounter)
        mSnapshotRestoreCounter->Inc();
    UpdateHealth();
    DIA_LOG_INFO("CoW", "SaveLoadModule: snapshot '%s' restored", name);
    std::snprintf(message, messageCapacity, "ok");
    return true;
}

void SaveLoadModule::CaptureState(CapturedStateList& out) const
{
    out.Clear();
    for (uint32_t i = 0; i < mRegistry.GetParticipantCount() && out.Size() < CapturedStateList::kCapacity; ++i)
    {
        const Dia::Core::StringCRC id = mRegistry.GetIdAt(i);
        Dia::SaveGame::SaveContext ctx;   // one per participant: its root is then exactly that participant's JSON
        ctx.BeginObject(id);
        mRegistry.GetParticipantAt(i)->Serialize(ctx);
        ctx.EndObject();

        CapturedState& item = out.Append();
        item.id = id;
        item.state = ctx.Root()[id.AsChar()];
    }
}

void SaveLoadModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    DIA_TRACE_ZONE("SaveLoadModule::DoUpdate", Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("SaveLoadModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    // Clear before executing so a request raised during a save/load waits for the next frame.
    if (mHasPendingSave)
    {
        const Dia::Core::StringCRC slot = mPendingSaveSlot;
        mHasPendingSave = false;
        ExecuteSave(slot);
        UpdateHealth();
    }
    if (mHasPendingLoad)
    {
        const Dia::Core::StringCRC slot = mPendingLoadSlot;
        mHasPendingLoad = false;
        ExecuteLoad(slot);
        UpdateHealth();
    }

    // After the user's own save/load, so a snapshot only waits when a request arrived mid-update.
    mSnapshots.RunFrameBoundary();
}

Dia::ApplicationFlow::StopResult SaveLoadModule::DoStop()
{
    mSnapshots.FailAll("save/load module is stopping");
    UnregisterCommands();
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    if (mStarted)
    {
        mManager.Shutdown();
        mStarted = false;
    }
    mHasPendingSave = false;
    mHasPendingLoad = false;
    mSaveCounter = mSaveFailCounter = mLoadCounter = mLoadFailCounter = nullptr;
    mSnapshotTakeCounter = mSnapshotRestoreCounter = mSnapshotFailCounter = nullptr;
    DIA_LOG_INFO("CoW", "SaveLoadModule: stopped (%u saves, %u loads)", mSaveCount, mLoadCount);
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using SaveLoadModule_ = CoW::SaveLoadModule; }
DIA_MODULE(SaveLoadModule_);
DIA_DESCRIBE(SaveLoadModule_::kTypeId, "Save/load orchestration over DiaSaveGame; executes requests at the sim-frame boundary.");
