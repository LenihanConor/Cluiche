#include "Modules/TestStages/TestAssetRuntimeStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestAssetRuntimeStageModule::kTypeId("TestAssetRuntimeStageModule");

TestAssetRuntimeStageModule::TestAssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC TestAssetRuntimeStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("AssetRuntimeTestStage");
}

const Dia::Core::StringCRC* TestAssetRuntimeStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.asset_runtime.all_loaded"),
        Dia::Core::StringCRC("test.asset_runtime.clean_reload")
    };
    outCount = 2;
    return names;
}

void TestAssetRuntimeStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    mLoadStartFrame = 0;
    mAllLoaded   = false;
    mCleanReload = false;

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.asset_runtime.all_loaded"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAllLoaded,
                     mAllLoaded ? "4/4 assets ready" : "loading in progress",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.asset_runtime.clean_reload"),
        [this]() -> Dia::Automation::CheckpointResult {
            if (GetEntryCount() < 2)
                return { false, "awaiting second entry", 0.0f };
            return { mCleanReload,
                     mCleanReload ? "reload state matches" : "state mismatch after reload",
                     0.0f };
        });

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricLoadCount)
        mMetricLoadCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_count"));
    if (!mMetricActiveHandles)
        mMetricActiveHandles = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.active_handles"));
    if (!mMetricLoadTimeMs)
        mMetricLoadTimeMs = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_time_ms"));
    if (!mMetricEntryCount)
        mMetricEntryCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.entry_count"));
    if (!mMetricSnapshotLoaded)
        mMetricSnapshotLoaded = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.snapshot_loaded"));

    if (mMetricEntryCount)
        mMetricEntryCount->Set(static_cast<double>(GetEntryCount()));

    DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule OnStart ready — entry %u", GetEntryCount());
}

void TestAssetRuntimeStageModule::OnUpdate(float /*deltaTime*/)
{
    if (mAllLoaded)
        return;

    if (GetFrameCount() <= 2)
        return;

    if (!mAssetLoadStatusStream.IsAvailable())
        return;

    const Cluiche::AppFlow::AssetLoadStatus& status = mAssetLoadStatusStream.Get();
    const bool stageComplete =
        status.stageId == Dia::Core::StringCRC("AssetRuntimeTestStage") &&
        status.state == Cluiche::AppFlow::AssetLoadStatus::State::kComplete;

    if (stageComplete)
    {
        mAllLoaded = true;

        if (GetEntryCount() == 1)
        {
            mFirstEntrySnapshot.loadedCount  = status.loaded;
            mFirstEntrySnapshot.allSucceeded = (status.failed == 0);
            if (mMetricSnapshotLoaded)
                mMetricSnapshotLoaded->Set(static_cast<double>(status.loaded));
        }
        else
        {
            mCleanReload = (status.loaded == mFirstEntrySnapshot.loadedCount &&
                            status.failed == 0 &&
                            mFirstEntrySnapshot.allSucceeded);
        }

        DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule: all loaded at frame %u (entry %u, loaded=%u, total=%u, failed=%u)",
            GetFrameCount(), GetEntryCount(), status.loaded, status.total, status.failed);

        if (mMetricLoadCount)     mMetricLoadCount->Set(static_cast<double>(status.loaded));
        if (mMetricActiveHandles) mMetricActiveHandles->Set(static_cast<double>(status.loaded));
        if (mMetricLoadTimeMs)
        {
            double elapsedMs = static_cast<double>(GetFrameCount() - mLoadStartFrame) * (1000.0 / 30.0);
            mMetricLoadTimeMs->Set(elapsedMs);
        }

        const bool allCheckpointsPassed = (GetEntryCount() == 1) ? true : mCleanReload;
        if (allCheckpointsPassed)
            ReportPassed();
    }
}

void TestAssetRuntimeStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAssetLoadStatusStream.Connect(app);
}

} // namespace CluicheTest

namespace { using TestAssetRuntimeStageModule_ = CluicheTest::TestAssetRuntimeStageModule; }
DIA_MODULE(TestAssetRuntimeStageModule_);
