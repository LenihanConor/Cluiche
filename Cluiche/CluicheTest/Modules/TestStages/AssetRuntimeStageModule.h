#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/AutomationModule.h"

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace CluicheTest {

class AssetRuntimeStageModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RegisterCheckpoints();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};

    struct LoadSnapshot {
        unsigned int loadedCount = 0;
        bool allSucceeded = false;
    };

    LoadSnapshot mFirstEntrySnapshot;
    unsigned int mEntryCount    = 0;
    unsigned int mFrameCount    = 0;
    unsigned int mLoadStartFrame = 0;
    bool         mAllLoaded     = false;
    bool         mCleanReload   = false;

    Dia::Observation::Metric::Gauge* mMetricLoadCount     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricActiveHandles = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLoadTimeMs    = nullptr;
};

} // namespace CluicheTest
