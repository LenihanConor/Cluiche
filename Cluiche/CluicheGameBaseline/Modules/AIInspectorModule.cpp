#include "Modules/AIInspectorModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaEntity/Domain.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaAIBudget/AIBudgetModule.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEntity/Entity.h>

#include "Modules/DebugServerHostModule.h"
#include "Modules/EntityModule.h"

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AIInspectorModule::kTypeId("AIInspectorModule");

AIInspectorModule::AIInspectorModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

AIInspectorModule::~AIInspectorModule() = default;

void AIInspectorModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAIInspectWriter.Connect(app);
}

Dia::ApplicationFlow::StartResult AIInspectorModule::DoStart()
{
    DIA_LOG_INFO("Application", "AIInspectorModule::DoStart");

    // Resolve the DebugServer pointer via the cross-PU service stream.
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(GetApplication());
    if (app)
    {
        auto* store = app->FindStream(
            Dia::Core::StringCRC(DebugServerHostModule::kServiceStreamId));
        auto* typed = dynamic_cast<
            Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
        if (typed)
            mDebugServer = typed->Get();
        else
            DIA_LOG_WARNING("Application", "AIInspectorModule: DebugServerService stream not found or wrong type");
    }

    if (!mDebugServer)
        DIA_LOG_WARNING("Application", "AIInspectorModule: no DebugServer — AI data will still be streamed");

    // Resolve AIBudgetModule from the same PU (graceful — may not be present on all stages).
    // AIBudgetModule uses kInstanceId rather than kTypeId, so we use FindModule directly.
    {
        Dia::ApplicationFlow::ProcessingUnit* pu = GetProcessingUnit();
        if (pu)
        {
            auto* found = pu->FindModule(Dia::AIBudget::AIBudgetModule::kInstanceId);
            if (found)
                mBudgetModule = static_cast<Dia::AIBudget::AIBudgetModule*>(found);
            else
                DIA_LOG_WARNING("Application", "AIInspectorModule: AIBudgetModule not found — budget source disabled");
        }
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void AIInspectorModule::DoUpdate(float dt)
{
    DIA_TRACE_ZONE("AIInspectorModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    ++mFrameCounter;

    PushBudget(dt);
    PushUtilityAI();
    PushRules();
    PushHTN();
}

Dia::ApplicationFlow::StopResult AIInspectorModule::DoStop()
{
    DIA_LOG_INFO("Application", "AIInspectorModule::DoStop");
    mDebugServer  = nullptr;
    mBudgetModule = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

// ---------------------------------------------------------------------------
// PushBudget — 1 Hz periodic push with rolling 60-frame history
// ---------------------------------------------------------------------------
void AIInspectorModule::PushBudget(float deltaTime)
{
    auto* budgetMod = mBudgetModule;
    if (!budgetMod)
        return;

    // Record this frame into the ring buffer
    const Dia::AIBudget::AIBudgetResult& result = budgetMod->GetLastResult();
    const float budgetMs = budgetMod->GetBudgetMs();

    BudgetFrame& slot = mBudgetHistory[mBudgetHistoryHead];
    slot.frameIndex      = mBudgetFrameIdx++;
    slot.usedMs          = result.usedMs;
    slot.systemsRun      = result.systemsRun;
    slot.systemsDeferred = result.systemsDeferred;
    slot.budgetMs        = budgetMs;

    mBudgetHistoryHead = (mBudgetHistoryHead + 1) % kBudgetHistoryDepth;
    if (mBudgetHistoryCount < kBudgetHistoryDepth)
        ++mBudgetHistoryCount;

    // Periodic push at kBudgetPeriodSec
    mBudgetAccSec += deltaTime;
    if (mBudgetAccSec < kBudgetPeriodSec)
        return;

    mBudgetAccSec -= kBudgetPeriodSec;

    // Build JSON payload from ring buffer (oldest first)
    Json::Value frames(Json::arrayValue);
    const int count = mBudgetHistoryCount;
    const int head  = mBudgetHistoryHead;
    for (int i = 0; i < count; ++i)
    {
        const int idx = (head - count + i + kBudgetHistoryDepth) % kBudgetHistoryDepth;
        const BudgetFrame& f = mBudgetHistory[idx];
        Json::Value entry;
        entry["frameIndex"]      = f.frameIndex;
        entry["usedMs"]          = f.usedMs;
        entry["systemsRun"]      = f.systemsRun;
        entry["systemsDeferred"] = f.systemsDeferred;
        entry["budgetMs"]        = f.budgetMs;
        frames.append(entry);
    }

    AIInspectEvent evt;
    evt.dataType = Dia::Core::StringCRC("ai.budget");
    evt.payload["frames"]       = frames;
    evt.payload["frameCount"]   = count;
    evt.payload["budgetMs"]     = budgetMs;
    evt.payload["registeredSystems"] = budgetMod->GetScheduler().GetRegisteredCount();

    mAIInspectWriter.Send(evt);
}

// ---------------------------------------------------------------------------
// PushUtilityAI — every frame, change-detected via hash
// ---------------------------------------------------------------------------
void AIInspectorModule::PushUtilityAI()
{
    auto* entityMod = mEntityRef.Get();
    if (!entityMod)
        return;

    auto& domain = entityMod->GetDomain();

    // Iterate all alive entities and collect those with a UtilitySetComponent.
    Json::Value entities(Json::arrayValue);
    unsigned int hashAccum = 0;

    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity entity = domain.GetAliveEntity(i);
        if (!entity.IsValid())
            continue;

        const Dia::UtilityAI::UtilitySetComponent* comp =
            domain.GetComponent<Dia::UtilityAI::UtilitySetComponent>(entity);
        if (!comp)
            continue;

        // Collect last-frame scores for change detection and payload
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> ids;
        Dia::Core::Containers::DynamicArrayC<float, 32> scores;
        const Dia::UtilityAI::UtilitySet* utilitySet = comp->GetUtilitySet();
        if (utilitySet)
            utilitySet->GetLastFrameScores(ids, scores);

        // Accumulate hash: entity index + winner CRC
        hashAccum ^= static_cast<unsigned int>(entity.GetIndex()) * 2654435761u;
        if (ids.Size() > 0)
        {
            // The winner is highest score — accumulate winner CRC into hash
            int winnerIdx = 0;
            float winnerScore = scores.Size() > 0 ? scores[0] : 0.0f;
            for (uint32_t s = 1; s < scores.Size(); ++s)
            {
                if (scores[s] > winnerScore)
                {
                    winnerScore = scores[s];
                    winnerIdx = static_cast<int>(s);
                }
            }
            if (winnerIdx < static_cast<int>(ids.Size()))
                hashAccum ^= ids[winnerIdx].Value() * 1234567891u;
        }

        // Build entity entry
        Json::Value entityEntry;
        entityEntry["index"]     = static_cast<int>(entity.GetIndex());

        const char* dbgName = domain.GetDebugName(entity);
        entityEntry["name"] = dbgName ? dbgName : "";

        Json::Value actionsArr(Json::arrayValue);
        for (uint32_t s = 0; s < ids.Size() && s < scores.Size(); ++s)
        {
            Json::Value action;
            action["id"]    = ids[s].Value();
            action["score"] = scores[s];
            actionsArr.append(action);
        }
        entityEntry["actions"] = actionsArr;

        entities.append(entityEntry);
    }

    // Change detection: only push if something changed
    const unsigned int newHash = hashAccum ^ static_cast<unsigned int>(entities.size());
    if (newHash == mUtilityAILastHash)
        return;
    mUtilityAILastHash = newHash;

    AIInspectEvent evt;
    evt.dataType            = Dia::Core::StringCRC("ai.utility");
    evt.payload["entities"] = entities;
    evt.payload["frame"]    = static_cast<Json::UInt64>(mFrameCounter);

    mAIInspectWriter.Send(evt);
}

// ---------------------------------------------------------------------------
// PushRules — stub (Task 5)
// ---------------------------------------------------------------------------
void AIInspectorModule::PushRules()
{
    // TODO: Task 5 — implement Rules inspector source
}

// ---------------------------------------------------------------------------
// PushHTN — stub (Task 5)
// ---------------------------------------------------------------------------
void AIInspectorModule::PushHTN()
{
    // TODO: Task 5 — implement HTN inspector source
}

} } // namespace Cluiche::AppFlow

namespace { using AIInspectorModule_ = Cluiche::AppFlow::AIInspectorModule; }
DIA_MODULE(AIInspectorModule_);
DIA_DESCRIBE(AIInspectorModule_::kTypeId,
    "Pushes AI inspector payloads (budget/utility/rules/HTN) to the debug server");

#endif // DIA_DEBUG
