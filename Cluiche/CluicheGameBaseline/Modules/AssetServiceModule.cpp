#include "Modules/AssetServiceModule.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaUIUltralight/UltralightUISystem.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

    void GetDirectoryFromPath(const char* filePath, char* outDir, unsigned int outDirSize)
    {
        const char* lastSlash = nullptr;
        for (const char* p = filePath; *p; ++p)
        {
            if (*p == '/' || *p == '\\')
                lastSlash = p;
        }
        if (lastSlash)
        {
            unsigned int len = static_cast<unsigned int>(lastSlash - filePath) + 1;
            if (len >= outDirSize)
                len = outDirSize - 1;
            std::memcpy(outDir, filePath, len);
            outDir[len] = '\0';
        }
        else
        {
            outDir[0] = '\0';
        }
    }

    bool ReadFileToString(const char* path, char* buffer, unsigned int bufferSize)
    {
        FILE* f = nullptr;
        fopen_s(&f, path, "rb");
        if (!f)
            return false;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0 || static_cast<unsigned int>(size) >= bufferSize)
        {
            fclose(f);
            return false;
        }

        fread(buffer, 1, size, f);
        buffer[size] = '\0';
        fclose(f);
        return true;
    }

} // namespace

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AssetServiceModule::kTypeId("AssetServiceModule");

AssetServiceModule::AssetServiceModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult AssetServiceModule::DoStart()
{
    DIA_LOG_INFO("Application", "AssetServiceModule DoStart entry");

    // Build deployRoot = <exe>/assets/
    std::string exePath;
    Dia::Core::Path::ExePath(exePath);
    mDeployRoot = Dia::Core::Containers::String512("%s/assets/", exePath.c_str());
    const char* deployRoot = mDeployRoot.AsCStr();

    // Load runtime manifest.
    Dia::Core::FilePath::ResoledFilePath manifestPath("%sassets.runtime.json", deployRoot);
    mRuntime.LoadManifest(manifestPath);

    // Parse .diagame for stage path map (global path_aliases are registered
    // in Main.cpp before the Application starts — required for
    // KernelModule's RenderWindow shader lookup).
    Dia::Core::Containers::String512 diagamePath("%scluichetest.diagame", deployRoot);
    ParseDiagame(diagamePath.AsCStr());

    // Wire Mesh3DAssetHandler to the JobSystem before any loads can be dispatched.
    DIA_ASSERT(mJobSystemRef.Get() != nullptr, "Mesh3DAssetHandler requires JobSystemModule to be initialized first");
    mMesh3DHandler.SetJobSystem(&mJobSystemRef->GetJobSystem());

    // Try to register handlers up front (requires KernelModule+UIModule).
    // Safe to retry later via EnsureHandlersRegistered if UI hasn't started.
    EnsureHandlersRegistered();

    // Kick off global load.
    RequestGlobalLoad();

    mAssetLoadStatus.stageId = Dia::Core::StringCRC{};
    mAssetLoadStatus.state   = AssetLoadStatus::State::kIdle;
    mAssetLoadStatusService.Register(mAssetLoadStatus);

    // Register health reporter.
    Dia::Observation::Health::HealthRegistry::Instance().Register(&mAssetReporter);
    mAssetReporter.SetOK();

    // Register asset metrics with the global MetricRegistry.
    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricAssetsLoaded  = reg.RegisterGauge(Dia::Core::StringCRC("dia.assets.loaded"));
        mMetricAssetsLoading = reg.RegisterGauge(Dia::Core::StringCRC("dia.assets.loading"));
        mMetricAssetsFailed  = reg.RegisterCounter(Dia::Core::StringCRC("dia.assets.failed"));
        static const float kLoadTimeBuckets[] = { 0.0f, 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f };
        mMetricLoadTimeMs = reg.RegisterHistogram(
            Dia::Core::StringCRC("dia.assets.load_time_ms"), kLoadTimeBuckets, 7);
    }

    DIA_LOG_INFO("Application", "AssetServiceModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetServiceModule::DoUpdate(float /*dt*/)
{
    // Late binding: keep trying until both handlers are registered.
    if (!mTextureHandlerRegistered || !mUIHandlerRegistered)
        EnsureHandlersRegistered();

    // 1. React to app-flow stage transitions by driving AssetRuntime's
    // per-stage load/unload. v1 did this from MainLoadPhase/MainFEPhase;
    // v2 centralises it here so each new stage doesn't need its own
    // load-trigger module.
    Dia::ApplicationFlow::IApplicationControl* app = GetApplication();
    if (app == nullptr)
        return;

    const Dia::Core::StringCRC currentStage = app->GetCurrentStage();
    if (currentStage != mCurrentAppFlowStage)
    {
        // Stage changed — unload the old stage (alias + assets), load the new.
        if (mCurrentAppFlowStage.Value() != 0)
        {
            // Unregister UI handler if UIModule has stopped — its UISystem is
            // deleted in DoStop, so the registered pointer is stale. Must happen
            // before RequestStageUnload to avoid DispatchUnload calling through
            // the dead vptr.
            if (mUIHandlerRegistered)
            {
                UIModule* ui = mUI.Get();
                if (!ui || !ui->HasStarted())
                {
                    mRuntime.UnregisterTypeHandler("ui");
                    mUIHandlerRegistered = false;
                }
            }

            // Unregister stage-scoped path aliases.
            UnregisterStageAliases();

            Dia::Core::StringCRC prevAssetStage = AssetStageIdFromAppStage(mCurrentAppFlowStage);
            if (prevAssetStage.Value() != 0)
            {
                DIA_LOG_INFO("AssetRuntime",
                    "AssetServiceModule: app-flow stage left '%s' -> unload '%s'",
                    mCurrentAppFlowStage.AsChar(), prevAssetStage.AsChar());
                RequestStageUnload(prevAssetStage);
            }
        }

        mCurrentAppFlowStage = currentStage;

        if (currentStage.Value() != 0)
        {
            // Register stage-scoped path aliases from this stage's .diastage.
            for (unsigned int i = 0; i < mStagePathMap.Size(); ++i)
            {
                if (mStagePathMap[i].mStageId == currentStage)
                {
                    RegisterStageAliases(mStagePathMap[i].mDiastagePath.AsCStr());
                    break;
                }
            }

            Dia::Core::StringCRC assetStage = AssetStageIdFromAppStage(currentStage);
            if (assetStage.Value() != 0)
            {
                DIA_LOG_INFO("AssetRuntime",
                    "AssetServiceModule: app-flow stage entered '%s' -> load '%s'",
                    currentStage.AsChar(), assetStage.AsChar());
                RequestStageLoad(assetStage);
            }

            // Find or create state slot, stamp load start time, mark kLoading.
            {
                StageStateEntry* entry = nullptr;
                for (unsigned int i = 0; i < mStageStateCount; ++i)
                {
                    if (mStageStates[i].appStageId == currentStage)
                    {
                        entry = &mStageStates[i];
                        break;
                    }
                }
                if (!entry && mStageStateCount < kMaxTrackedStages)
                {
                    entry = &mStageStates[mStageStateCount];
                    entry->appStageId = currentStage;
                    entry->state.store(StageLoadState::kIdle, std::memory_order_relaxed);
                    ++mStageStateCount;
                }
                if (entry)
                {
                    entry->loadStartMs = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    entry->state.store(StageLoadState::kLoading, std::memory_order_release);
                }
            }
        }
    }

    // 2. Pump main-thread asset handler completions.
    if (mTextureHandlerService.IsAvailable())
        mTextureHandlerService.Get().Tick();
    mMesh3DHandler.Tick();

    // 3. Recompute terminal states for any kLoading stage.
    for (unsigned int i = 0; i < mStageStateCount; ++i)
    {
        if (mStageStates[i].state.load(std::memory_order_acquire) != StageLoadState::kLoading)
            continue;

        Dia::Core::StringCRC assetStage = AssetStageIdFromAppStage(mStageStates[i].appStageId);
        if (assetStage.Value() == 0)
        {
            // Stages with no asset runtime stage (e.g. "Boot") complete immediately.
            mStageStates[i].state.store(StageLoadState::kComplete, std::memory_order_release);
            DIA_LOG_INFO("Application",
                "AssetService: stage '%s' complete (no asset stage)",
                mStageStates[i].appStageId.AsChar());
            continue;
        }

        const Dia::AssetRuntime::AssetRuntime::LoadProgress progress =
            mRuntime.GetLoadProgress(assetStage);

        if (progress.failed > 0)
        {
            mStageStates[i].state.store(StageLoadState::kFailed, std::memory_order_release);
            DIA_LOG_ERROR("Application",
                "AssetService: stage '%s' failed (%u/%u failed)",
                mStageStates[i].appStageId.AsChar(), progress.failed, progress.total);
        }
        else if (progress.total == 0 || progress.loaded == progress.total)
        {
            mStageStates[i].state.store(StageLoadState::kComplete, std::memory_order_release);
            if (mMetricLoadTimeMs && mStageStates[i].loadStartMs != 0)
            {
                uint64_t nowMs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                mMetricLoadTimeMs->Observe(static_cast<double>(nowMs - mStageStates[i].loadStartMs));
            }
            DIA_LOG_INFO("Application",
                "AssetService: stage '%s' complete (%u/%u loaded)",
                mStageStates[i].appStageId.AsChar(), progress.loaded, progress.total);
        }
    }

    // 4. Update asset health reporter — failing if any stage has failed.
    {
        bool anyFailed = false;
        for (unsigned int i = 0; i < mStageStateCount; ++i)
        {
            if (mStageStates[i].state.load(std::memory_order_acquire) == StageLoadState::kFailed)
            {
                anyFailed = true;
                break;
            }
        }
        if (anyFailed)
            mAssetReporter.SetFailing(Dia::Core::StringCRC("asset.stage.failed"));
        else
            mAssetReporter.SetOK();
    }

    // 5. Update asset metrics from current load stage.
    if (mCurrentLoadStageId.Value() != 0)
    {
        const Dia::AssetRuntime::AssetRuntime::LoadProgress progress =
            mRuntime.GetLoadProgress(mCurrentLoadStageId);

        if (mMetricAssetsLoaded)
            mMetricAssetsLoaded->Set(static_cast<double>(progress.loaded));
        if (mMetricAssetsLoading)
            mMetricAssetsLoading->Set(static_cast<double>(
                progress.total > progress.loaded ? progress.total - progress.loaded : 0u));
        if (mMetricAssetsFailed && progress.failed > mPrevAssetsFailed)
        {
            mMetricAssetsFailed->Inc(progress.failed - mPrevAssetsFailed);
            mPrevAssetsFailed = progress.failed;
        }
    }

    // 6. Publish current stage load status via ServiceStream for SimPU consumers.
    {
        AssetLoadStatus::State streamState = AssetLoadStatus::State::kIdle;
        unsigned int loaded = 0, total = 0, failed = 0;
        if (mCurrentAppFlowStage.Value() != 0)
        {
            StageLoadState raw = GetStageLoadState(mCurrentAppFlowStage);
            switch (raw)
            {
                case StageLoadState::kLoading:  streamState = AssetLoadStatus::State::kLoading;  break;
                case StageLoadState::kComplete:  streamState = AssetLoadStatus::State::kComplete; break;
                case StageLoadState::kFailed:    streamState = AssetLoadStatus::State::kFailed;   break;
                default:                         streamState = AssetLoadStatus::State::kIdle;     break;
            }
            Dia::Core::StringCRC assetStage = AssetStageIdFromAppStage(mCurrentAppFlowStage);
            if (assetStage.Value() != 0)
            {
                auto progress = mRuntime.GetLoadProgress(assetStage);
                loaded = progress.loaded;
                total  = progress.total;
                failed = progress.failed;
            }
        }
        mAssetLoadStatus.stageId = mCurrentAppFlowStage;
        mAssetLoadStatus.state   = streamState;
        mAssetLoadStatus.loaded  = loaded;
        mAssetLoadStatus.total   = total;
        mAssetLoadStatus.failed  = failed;
    }
}

// Map app-flow stage id (e.g. "DummyStage") to AssetRuntime stage id
// (e.g. "stage.dummy_stage"). Mirrors v1's PascalCase->snake_case convention.
// Returns empty StringCRC if no mapping exists (e.g. "Boot" has no asset stage).
Dia::Core::StringCRC AssetServiceModule::AssetStageIdFromAppStage(
    const Dia::Core::StringCRC& appStage) const
{
    const char* name = appStage.AsChar();
    if (name == nullptr || name[0] == '\0')
        return Dia::Core::StringCRC();

    // Boot has no runtime-assets stage — global assets loaded once in DoStart.
    if (std::strcmp(name, "Boot") == 0)
        return Dia::Core::StringCRC();

    Dia::Core::Containers::String512 out("stage.");
    for (unsigned int c = 0; name[c] != '\0'; ++c)
    {
        char ch = name[c];
        if (ch >= 'A' && ch <= 'Z')
        {
            // Insert '_' before an uppercase when:
            //  - preceded by a lowercase (e.g. "yS" in "BodyStage" → "body_stage"), OR
            //  - preceded by an uppercase that is itself followed by a lowercase (e.g. "DS" in
            //    "2DStage" → "2d_stage"; but "2D" alone or "2De…" stays "2d").
            if (c > 0)
            {
                char prev = name[c - 1];
                char next = name[c + 1];
                bool prevLower = (prev >= 'a' && prev <= 'z');
                bool prevUpper = (prev >= 'A' && prev <= 'Z');
                bool nextLower = (next >= 'a' && next <= 'z');
                if (prevLower || (prevUpper && nextLower))
                    out.Append('_');
            }
            out.Append(static_cast<char>(ch + 32));
        }
        else
        {
            out.Append(ch);
        }
    }
    return Dia::Core::StringCRC(out.AsCStr());
}

Dia::ApplicationFlow::StopResult AssetServiceModule::DoStop()
{
    DIA_LOG_INFO("Application", "AssetServiceModule DoStop entry");

    // Unregister health reporter before tearing down state.
    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mAssetReporter);

    mAssetLoadStatus = AssetLoadStatus{};
    UnregisterStageAliases();
    mRuntime.Reset();
    mJsonHandlerRegistered   = false;
    mMesh3DHandlerRegistered = false;

    // Null metric pointers — MetricRegistry owns the objects.
    mMetricAssetsLoaded  = nullptr;
    mMetricAssetsLoading = nullptr;
    mMetricAssetsFailed  = nullptr;
    mMetricLoadTimeMs    = nullptr;

    DIA_LOG_INFO("Application", "AssetServiceModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AssetServiceModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mTextureHandlerService.Connect(app);
    mAssetLoadStatusService.Connect(app);
}

bool AssetServiceModule::IsLoadComplete() const
{
    return mRuntime.IsLoadComplete(Dia::Core::StringCRC("stage.global"));
}

bool AssetServiceModule::IsStageLoadComplete(const Dia::Core::StringCRC& stageId) const
{
    return GetStageLoadState(stageId) == StageLoadState::kComplete;
}

AssetServiceModule::StageLoadState
AssetServiceModule::GetStageLoadState(const Dia::Core::StringCRC& stageId) const
{
    const std::atomic<StageLoadState>* slot = FindStateSlot(stageId);
    if (!slot)
        return StageLoadState::kIdle;
    return slot->load(std::memory_order_acquire);
}

std::atomic<AssetServiceModule::StageLoadState>*
AssetServiceModule::FindOrCreateStateSlot(const Dia::Core::StringCRC& appStageId)
{
    for (unsigned int i = 0; i < mStageStateCount; ++i)
    {
        if (mStageStates[i].appStageId == appStageId)
            return &mStageStates[i].state;
    }
    if (mStageStateCount < kMaxTrackedStages)
    {
        mStageStates[mStageStateCount].appStageId = appStageId;
        mStageStates[mStageStateCount].state.store(StageLoadState::kIdle, std::memory_order_relaxed);
        return &mStageStates[mStageStateCount++].state;
    }
    return nullptr;
}

const std::atomic<AssetServiceModule::StageLoadState>*
AssetServiceModule::FindStateSlot(const Dia::Core::StringCRC& appStageId) const
{
    for (unsigned int i = 0; i < mStageStateCount; ++i)
    {
        if (mStageStates[i].appStageId == appStageId)
            return &mStageStates[i].state;
    }
    return nullptr;
}

void AssetServiceModule::RequestGlobalLoad()
{
    EnsureHandlersRegistered();
    mCurrentLoadStageId = Dia::Core::StringCRC("stage.global");
    mRuntime.RequestStageLoad(mCurrentLoadStageId);
}

void AssetServiceModule::RequestStageLoad(const Dia::Core::StringCRC& stageId)
{
    EnsureHandlersRegistered();
    mCurrentLoadStageId = stageId;
    mRuntime.RequestStageLoad(stageId);
}

void AssetServiceModule::RequestStageUnload(const Dia::Core::StringCRC& stageId)
{
    mRuntime.RequestStageUnload(stageId);
}

void AssetServiceModule::EnsureHandlersRegistered()
{
    UIModule* ui = mUI.Get();

    bool justRegistered = false;

    if (!mTextureHandlerRegistered && mTextureHandlerService.IsAvailable())
    {
        mRuntime.RegisterTypeHandler("texture", &mTextureHandlerService.Get());
        mTextureHandlerRegistered = true;
        justRegistered = true;
    }

    if (!mUIHandlerRegistered && ui && ui->GetUISystem())
    {
        auto* uiSystem =
            static_cast<Dia::UI::Ultralight::UISystem*>(ui->GetUISystem());
        mRuntime.RegisterTypeHandler("ui", uiSystem->GetUIHandler());
        mUIHandlerRegistered = true;
        justRegistered = true;
    }

    if (!mJsonHandlerRegistered)
    {
        mRuntime.RegisterTypeHandler("json", &mJsonHandler);
        mJsonHandlerRegistered = true;
        justRegistered = true;
    }

    if (!mMesh3DHandlerRegistered)
    {
        mRuntime.RegisterTypeHandler("mesh3d", &mMesh3DHandler);
        mMesh3DHandlerRegistered = true;
        justRegistered = true;
    }

    if (justRegistered && mTextureHandlerRegistered && mUIHandlerRegistered && mMesh3DHandlerRegistered)
    {
        DIA_LOG_INFO("AssetRuntime",
            "AssetServiceModule: texture + ui + json + mesh3d type handlers registered");
    }
}

bool AssetServiceModule::ParseDiagame(const char* diagamePath)
{
    static const unsigned int kMaxFileSize = 8192;
    char fileBuffer[kMaxFileSize];
    if (!ReadFileToString(diagamePath, fileBuffer, kMaxFileSize))
    {
        DIA_LOG_WARNING("AssetRuntime",
            "AssetServiceModule: could not read .diagame '%s'", diagamePath);
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(fileBuffer, root, false))
    {
        DIA_LOG_WARNING("AssetRuntime",
            "AssetServiceModule: failed to parse .diagame '%s'", diagamePath);
        return false;
    }

    char diagameDir[512];
    GetDirectoryFromPath(diagamePath, diagameDir, sizeof(diagameDir));

    // Build stage id -> .diastage path map from typed "stage" imports.
    if (root.isMember("imports") && root["imports"].isArray())
    {
        const Json::Value& imports = root["imports"];
        for (unsigned int i = 0; i < imports.size(); ++i)
        {
            const Json::Value& entry = imports[i];
            if (!entry.isMember("type") || !entry["type"].isString())
                continue;
            if (std::strcmp(entry["type"].asCString(), "stage") != 0)
                continue;
            if (!entry.isMember("path") || !entry["path"].isString())
                continue;

            Dia::Core::Containers::String512 resolved;
            Dia::Core::Path::ResolveRelative(diagameDir, entry["path"].asCString(), resolved);

            // Read the .diastage file to get the stage name, then map it to
            // its .diastage path.
            char stageFileBuffer[4096];
            if (!ReadFileToString(resolved.AsCStr(), stageFileBuffer, sizeof(stageFileBuffer)))
                continue;

            Json::Value stageRoot;
            Json::Reader stageReader;
            if (!stageReader.parse(stageFileBuffer, stageRoot, false))
                continue;
            if (!stageRoot.isMember("name") || !stageRoot["name"].isString())
                continue;

            StagePathEntry e;
            e.mStageId       = Dia::Core::StringCRC(stageRoot["name"].asCString());
            e.mDiastagePath  = resolved;
            mStagePathMap.Add(e);
        }
    }

    return true;
}

void AssetServiceModule::RegisterStageAliases(const char* diastagePath)
{
    UnregisterStageAliases();

    static const unsigned int kMaxFileSize = 4096;
    char fileBuffer[kMaxFileSize];
    if (!ReadFileToString(diastagePath, fileBuffer, kMaxFileSize))
    {
        DIA_LOG_ERROR("AssetRuntime", "Failed to read .diastage: %s", diastagePath);
        return;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(fileBuffer, root, false) || !root.isMember("config"))
        return;

    const Json::Value& config = root["config"];
    if (!config.isMember("path_aliases") || !config["path_aliases"].isObject())
        return;

    char stageDir[512];
    GetDirectoryFromPath(diastagePath, stageDir, sizeof(stageDir));

    const Json::Value& aliases = config["path_aliases"];
    Json::Value::Members members = aliases.getMemberNames();
    for (unsigned int i = 0; i < members.size(); ++i)
    {
        const std::string& aliasName = members[i];
        const char* relativePath = aliases[aliasName].asCString();

        PathAliasEntry entry;
        entry.mAlias = Dia::Core::Containers::String32(aliasName.c_str());
        Dia::Core::Path::ResolveRelative(stageDir, relativePath, entry.mResolvedPath);

        mStageAliases.Add(entry);

        Dia::Core::Path::Alias alias(entry.mAlias.AsCStr());
        Dia::Core::Path::String pathStr(entry.mResolvedPath.AsCStr());
        Dia::Core::PathStore::RegisterToStore(alias, pathStr);

        DIA_LOG_INFO("AssetRuntime",
            "AssetServiceModule: stage alias '%s' -> '%s'",
            entry.mAlias.AsCStr(), entry.mResolvedPath.AsCStr());
    }

    // Register stage_scene alias if the .diastage declares a scene file.
    if (root.isMember("scene") && root["scene"].isString())
    {
        PathAliasEntry entry;
        entry.mAlias = Dia::Core::Containers::String32("stage_scene");
        Dia::Core::Path::ResolveRelative(stageDir, root["scene"].asCString(), entry.mResolvedPath);

        mStageAliases.Add(entry);

        Dia::Core::Path::Alias alias("stage_scene");
        Dia::Core::Path::String pathStr(entry.mResolvedPath.AsCStr());
        Dia::Core::PathStore::RegisterToStore(alias, pathStr);

        DIA_LOG_INFO("AssetRuntime",
            "AssetServiceModule: stage alias 'stage_scene' -> '%s'",
            entry.mResolvedPath.AsCStr());
    }
}

void AssetServiceModule::UnregisterStageAliases()
{
    for (unsigned int i = 0; i < mStageAliases.Size(); ++i)
    {
        Dia::Core::Path::Alias alias(mStageAliases[i].mAlias.AsCStr());
        Dia::Core::PathStore::UnregisterFromStore(alias);
    }
    mStageAliases.RemoveAll();
}

} } // namespace Cluiche::AppFlow

namespace { using AssetServiceModule_ = Cluiche::AppFlow::AssetServiceModule; }
DIA_MODULE(AssetServiceModule_);
DIA_DESCRIBE(AssetServiceModule_::kTypeId, "Owns the asset runtime: loads, caches, and serves assets to consumers via ServiceStream.");
