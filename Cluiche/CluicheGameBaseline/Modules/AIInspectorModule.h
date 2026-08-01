#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/EntityModule.h"
#include "Types/DebugServerPushEvent.h"

namespace Dia { namespace DebugServer { class DebugServer; } }
namespace Dia { namespace AIBudget    { class AIBudgetModule; } }

namespace Cluiche { namespace AppFlow {

class AIInspectorModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Pushes AI inspector payloads (budget/utility/rules/HTN) to the debug server";

    explicit AIInspectorModule(const Dia::Core::StringCRC& instanceId);
    ~AIInspectorModule() override;

protected:
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
protected:
    void PushBudget(float deltaTime);

    // AIBudgetModule resolved at DoStart() — uses kInstanceId, not kTypeId,
    // so cannot use ModuleRef<T> default. Resolved manually via PU module search.
    Dia::AIBudget::AIBudgetModule* mBudgetModule = nullptr;

    // Budget: 1 Hz periodic push with 60-frame rolling history
    static constexpr int   kBudgetHistoryDepth = 60;
    static constexpr float kBudgetPeriodSec    = 1.0f;

    struct BudgetFrame
    {
        int   frameIndex      = 0;
        float usedMs          = 0.0f;
        int   systemsRun      = 0;
        int   systemsDeferred = 0;
        float budgetMs        = 0.0f;
    };

    BudgetFrame mBudgetHistory[kBudgetHistoryDepth]{};
    int   mBudgetHistoryHead  = 0;
    int   mBudgetHistoryCount = 0;
    float mBudgetAccSec       = 0.0f;
    int   mBudgetFrameIdx     = 0;

private:
    void PushUtilityAI();
    void PushRules();
    void PushHTN();

    Dia::ApplicationFlow::ModuleRef<EntityModule> mEntityRef{this};

    Dia::ApplicationFlow::EventStreamWriter<DebugServerPushEvent> mAIInspectWriter{
        this, Dia::Core::StringCRC("DebugServerPush")};

    Dia::DebugServer::DebugServer* mDebugServer = nullptr;

    // UtilityAI/Rules/HTN: last hash for change detection
    unsigned int mUtilityAILastHash = 0;
    unsigned int mRulesLastHash     = 0;
    unsigned int mHTNLastHash       = 0;

    uint64_t mFrameCounter = 0;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
