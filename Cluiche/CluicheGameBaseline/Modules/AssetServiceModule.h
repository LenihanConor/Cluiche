#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <atomic>

#include "Modules/KernelModule.h"
#include "Modules/UIModule.h"

namespace Cluiche { namespace AppFlow {

// AssetServiceModule (Main PU): wraps AssetRuntime.
//   - On DoStart: loads assets.runtime.json, parses the .diagame for
//     global path_aliases + stage path_aliases, registers them with
//     PathStore, registers type handlers (texture + ui), and issues a
//     RequestGlobalLoad for the "stage.global" runtime stage.
//   - RequestStageLoad / RequestStageUnload are called externally by
//     stage-loading code when a stage starts / stops (ref-counted inside
//     AssetRuntime).
class AssetServiceModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AssetServiceModule(const Dia::Core::StringCRC& instanceId);

    void RequestGlobalLoad();
    void RequestStageLoad(const Dia::Core::StringCRC& stageId);
    void RequestStageUnload(const Dia::Core::StringCRC& stageId);

    bool IsLoadComplete() const;
    const Dia::AssetRuntime::AssetRuntime& GetRuntime() const { return mRuntime; }

    // Per-stage load state (written on MainPU, read atomically from any PU)
    enum class StageLoadState { kIdle, kLoading, kComplete, kFailed };

    static AssetServiceModule* GetStatic();

    bool           IsStageLoadComplete(const Dia::Core::StringCRC& stageId) const;
    StageLoadState GetStageLoadState (const Dia::Core::StringCRC& stageId) const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    struct PathAliasEntry
    {
        Dia::Core::Containers::String32  mAlias;
        Dia::Core::Containers::String512 mResolvedPath;
    };

    struct StagePathEntry
    {
        Dia::Core::StringCRC             mStageId;
        Dia::Core::Containers::String512 mDiastagePath;
    };

    void EnsureHandlersRegistered();
    void RegisterStageAliases(const char* diastagePath);
    void UnregisterStageAliases();
    bool ParseDiagame(const char* diagamePath);

    // Translate v2 app-flow stage (e.g. "DummyStage") to its AssetRuntime
    // stage id ("stage.dummy_stage"). Mirrors the PascalCase->snake_case
    // convention v1 used in AssetServiceModule.
    Dia::Core::StringCRC AssetStageIdFromAppStage(const Dia::Core::StringCRC& appStage) const;

    // Find or create a state slot for the given app-stage id (MainPU only).
    std::atomic<StageLoadState>* FindOrCreateStateSlot(const Dia::Core::StringCRC& appStageId);
    // Const lookup: safe to call from any PU (read-only, no resize).
    const std::atomic<StageLoadState>* FindStateSlot(const Dia::Core::StringCRC& appStageId) const;

    static AssetServiceModule* sInstance;

    Dia::AssetRuntime::AssetRuntime mRuntime;
    Dia::Core::StringCRC mCurrentLoadStageId;         // AssetRuntime stage id
    Dia::Core::StringCRC mCurrentAppFlowStage;         // last app-flow stage we reacted to
    bool mHandlersRegistered = false;

    Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16>  mStageAliases;
    Dia::Core::Containers::DynamicArrayC<StagePathEntry,   8> mStagePathMap;
    Dia::Core::Containers::String512 mDeployRoot;

    // Per-stage load state tracking: plain C array avoids DynamicArrayC copy
    // issues with non-copyable std::atomic members.
    static const unsigned int kMaxTrackedStages = 8;
    struct StageStateEntry
    {
        Dia::Core::StringCRC appStageId;
        std::atomic<StageLoadState> state{ StageLoadState::kIdle };

        StageStateEntry() = default;
        StageStateEntry(const StageStateEntry&) = delete;
        StageStateEntry& operator=(const StageStateEntry&) = delete;
    };
    StageStateEntry  mStageStates[kMaxTrackedStages];
    unsigned int     mStageStateCount = 0;

    Dia::ApplicationFlow::ModuleRef<KernelModule> mKernel{this};
    Dia::ApplicationFlow::ModuleRef<UIModule>     mUI{this};
};

} } // namespace Cluiche::AppFlow
