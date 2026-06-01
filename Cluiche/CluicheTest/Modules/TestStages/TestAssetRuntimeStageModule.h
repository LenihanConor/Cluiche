#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Types/AssetLoadStatus.h"

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace CluicheTest {

class TestAssetRuntimeStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaAssetRuntime: asset loading, handle lifecycle, clean reload";
    explicit TestAssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 240; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool PersistsAcrossEntries() const override { return true; }
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    struct LoadSnapshot {
        unsigned int loadedCount = 0;
        bool allSucceeded = false;
    };

    LoadSnapshot mFirstEntrySnapshot;
    unsigned int mLoadStartFrame = 0;
    bool         mAllLoaded     = false;
    bool         mCleanReload   = false;

    Dia::ApplicationFlow::ServiceStreamReader<Cluiche::AppFlow::AssetLoadStatus> mAssetLoadStatusStream{this, "AssetLoadStatus"};

    Dia::Observation::Metric::Gauge* mMetricLoadCount     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricActiveHandles = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLoadTimeMs    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricEntryCount    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricSnapshotLoaded = nullptr;
};

} // namespace CluicheTest
