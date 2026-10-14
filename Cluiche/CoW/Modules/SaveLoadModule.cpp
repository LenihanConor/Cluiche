#include "Modules/SaveLoadModule.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveManifest.h>

#include <cstdio>
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
    config.maxSlots      = 0;
    config.format        = Dia::SaveGame::SaveFormat::Json;
    return config;
}

SaveLoadModule::SaveLoadModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    std::snprintf(mBaseDirectory, kBaseDirCapacity, "%s", kDefaultBaseDirectory);
    mConfig = DefaultConfig();
    mConfig.baseDirectory = mBaseDirectory;
}

SaveLoadModule::~SaveLoadModule() = default;

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
    }
}

void SaveLoadModule::RegisterParticipant(Dia::Core::StringCRC id, Dia::SaveGame::ISaveable* participant)
{
    mRegistry.Register(id, participant);
}

void SaveLoadModule::UnregisterParticipant(Dia::Core::StringCRC id)
{
    mRegistry.Unregister(id);
}

void SaveLoadModule::RequestSave(Dia::Core::StringCRC slotId)
{
    mPendingSaveSlot = slotId;
    mHasPendingSave = true;
}

void SaveLoadModule::RequestLoad(Dia::Core::StringCRC slotId)
{
    mPendingLoadSlot = slotId;
    mHasPendingLoad = true;
}

void SaveLoadModule::EnumerateSlots(Dia::Core::Containers::DynamicArrayC<Dia::SaveGame::SlotInfo, 32>& out) const
{
    const unsigned int first = out.Size();
    mSlots.EnumerateSlots(out);

    // SlotManager reports the file name minus ".sav" (e.g. "slot_quick"), not the slot id that was
    // passed to Save. Strip the literal pattern prefix ("slot_") so the ids round-trip into Load.
    const char* pattern = mConfig.slotPattern;
    const char* placeholder = std::strstr(pattern, "{id}");
    const size_t prefixLen = placeholder ? static_cast<size_t>(placeholder - pattern) : 0;
    if (prefixLen == 0)
        return;
    for (unsigned int i = first; i < out.Size(); ++i)
    {
        const char* name = out[i].id.AsChar();
        if (std::strncmp(name, pattern, prefixLen) == 0)
            out[i].id = Dia::Core::StringCRC(name + prefixLen);
    }
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
        RequestSave(Dia::Core::StringCRC(args.positionalArgs[0]));
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
        RequestLoad(Dia::Core::StringCRC(args.positionalArgs[0]));
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
            std::printf("  %s  %llu bytes\n", slots[i].id.AsChar(),
                        static_cast<unsigned long long>(ec ? 0 : size));
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

    std::error_code ec;
    std::filesystem::create_directories(mBaseDirectory, ec);
    if (ec)
        DIA_LOG_ERROR("CoW", "SaveLoadModule: cannot create '%s' (%s)", mBaseDirectory, ec.message().c_str());

    mManager.Init(mConfig, mRegistry);
    mSlots.Init(mConfig);
    mStarted = true;
    mHasPendingSave = false;
    mHasPendingLoad = false;

    RegisterCommands();
    DIA_LOG_INFO("CoW", "SaveLoadModule: started, saving to '%s'", mBaseDirectory);
    return Dia::ApplicationFlow::StartResult::kReady;
}

LoadResult SaveLoadModule::PreValidateLoad(Dia::Core::StringCRC slotId)
{
    if (!mSlots.SlotExists(slotId))
        return LoadResult::Fail(LoadResultCode::SlotNotFound);

    char path[Dia::SaveGame::SaveManager::kMaxPathLen];
    mSlots.BuildPath(slotId, path, sizeof(path));

    auto buffer = std::make_unique<char[]>(Dia::SaveGame::SaveManager::kBufferSize);
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return LoadResult::Fail(LoadResultCode::FileReadError);
    const size_t bytes = std::fread(buffer.get(), 1, Dia::SaveGame::SaveManager::kBufferSize - 1, f);
    std::fclose(f);
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

    return LoadResult::Success();
}

void SaveLoadModule::ExecuteSave(Dia::Core::StringCRC slotId)
{
    mLastSaveResult = mManager.Save(slotId);
    if (mLastSaveResult.Ok())
    {
        ++mSaveCount;
        mSaveCounter->Inc();
        DIA_LOG_INFO("CoW", "SaveLoadModule: saved slot '%s'", slotId.AsChar());
    }
    else
    {
        mSaveFailCounter->Inc();
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
        DIA_LOG_ERROR("CoW", "SaveLoadModule: load of slot '%s' failed (LoadResultCode %d), game state untouched",
                      slotId.AsChar(), static_cast<int>(mLastLoadResult.code));
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
    }
    if (mHasPendingLoad)
    {
        const Dia::Core::StringCRC slot = mPendingLoadSlot;
        mHasPendingLoad = false;
        ExecuteLoad(slot);
    }
}

Dia::ApplicationFlow::StopResult SaveLoadModule::DoStop()
{
    UnregisterCommands();
    if (mStarted)
    {
        mManager.Shutdown();
        mStarted = false;
    }
    mHasPendingSave = false;
    mHasPendingLoad = false;
    mSaveCounter = mSaveFailCounter = mLoadCounter = mLoadFailCounter = nullptr;
    DIA_LOG_INFO("CoW", "SaveLoadModule: stopped (%u saves, %u loads)", mSaveCount, mLoadCount);
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using SaveLoadModule_ = CoW::SaveLoadModule; }
DIA_MODULE(SaveLoadModule_);
DIA_DESCRIBE(SaveLoadModule_::kTypeId, "Save/load orchestration over DiaSaveGame; executes requests at the sim-frame boundary.");
