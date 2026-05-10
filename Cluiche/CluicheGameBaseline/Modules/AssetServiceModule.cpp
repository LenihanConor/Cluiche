#include "Modules/AssetServiceModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaUIUltralight/UltralightUISystem.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

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

    // Try to register handlers up front (requires KernelModule+UIModule).
    // Safe to retry later via EnsureHandlersRegistered if UI hasn't started.
    EnsureHandlersRegistered();

    // Kick off global load.
    RequestGlobalLoad();

    DIA_LOG_INFO("Application", "AssetServiceModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetServiceModule::DoUpdate(float /*dt*/)
{
    // Late binding of the UI type handler if UIModule finished starting
    // after this module's DoStart.
    if (!mHandlersRegistered)
        EnsureHandlersRegistered();

    // React to app-flow stage transitions by driving AssetRuntime's
    // per-stage load/unload. v1 did this from MainLoadPhase/MainFEPhase;
    // v2 centralises it here so each new stage doesn't need its own
    // load-trigger module.
    Dia::ApplicationFlow::Application* app =
        GetProcessingUnit() ? GetProcessingUnit()->GetApplication() : nullptr;
    if (app == nullptr)
        return;

    const Dia::Core::StringCRC currentStage = app->GetCurrentStage();
    if (currentStage == mCurrentAppFlowStage)
        return;

    // Stage changed — unload the old stage (alias + assets), load the new.
    if (mCurrentAppFlowStage.Value() != 0)
    {
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
            if (c > 0)
                out.Append('_');
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
    UnregisterStageAliases();
    mRuntime.Reset();
    DIA_LOG_INFO("Application", "AssetServiceModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

bool AssetServiceModule::IsLoadComplete() const
{
    return mRuntime.IsLoadComplete(Dia::Core::StringCRC("stage.global"));
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
    if (mHandlersRegistered)
        return;

    KernelModule* kernel = mKernel.Get();
    UIModule*     ui     = mUI.Get();

    bool textureRegistered = false;
    bool uiRegistered      = false;

    if (kernel && kernel->GetWindow())
    {
        Dia::SFML::RenderWindow* window =
            static_cast<Dia::SFML::RenderWindow*>(kernel->GetWindow());
        mRuntime.RegisterTypeHandler("texture", window->GetTextureHandler());
        textureRegistered = true;
    }

    if (ui && ui->GetUISystem())
    {
        auto* uiSystem =
            static_cast<Dia::UI::Ultralight::UISystem*>(ui->GetUISystem());
        mRuntime.RegisterTypeHandler("ui", uiSystem->GetUIHandler());
        uiRegistered = true;
    }

    if (textureRegistered && uiRegistered)
    {
        mHandlersRegistered = true;
        DIA_LOG_INFO("AssetRuntime",
            "AssetServiceModule: texture + ui type handlers registered");
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
