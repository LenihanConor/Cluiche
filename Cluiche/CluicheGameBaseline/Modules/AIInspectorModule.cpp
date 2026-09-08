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
#include <DiaRules/RuleSetComponent.h>
#include <DiaRules/RuleSet.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEntity/Entity.h>

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Modules/DebugServerHostModule.h"
#include "Modules/EntityModule.h"

// ---------------------------------------------------------------------------
// Server-side HTN plan history (PD-004: STL allowed in .cpp)
// ---------------------------------------------------------------------------
namespace {

struct PlanHistoryEntry
{
    int              planIndex   = 0;
    int              startFrame  = 0;
    int              endFrame    = -1;  // -1 means current / still active
    std::string      reason;            // "initial" or "replan"
    std::vector<uint32_t> taskOperatorCRCs;
};

struct EntityHTNState
{
    std::deque<PlanHistoryEntry> history;   // ring-capped at kMaxDepth
    unsigned int                 lastPlanHash  = 0;
    int                          nextPlanIndex = 0;
    static constexpr int         kMaxDepth     = 8;
};

std::unordered_map<uint32_t, EntityHTNState> sHTNHistory;

} // anonymous namespace

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AIInspectorModule::kTypeId("AIInspectorModule");

AIInspectorModule::AIInspectorModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
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

void AIInspectorModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    const float dt = ctx.gameDt.AsFloatInSeconds();
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

    DIA_ASSERT(mBudgetHistoryHead >= 0 && mBudgetHistoryHead < kBudgetHistoryDepth,
        "AIInspectorModule::PushBudget — mBudgetHistoryHead out of range (%d)", mBudgetHistoryHead);
    DIA_ASSERT(mBudgetHistoryCount >= 0 && mBudgetHistoryCount <= kBudgetHistoryDepth,
        "AIInspectorModule::PushBudget — mBudgetHistoryCount out of range (%d)", mBudgetHistoryCount);

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

    DebugServerPushEvent evt;
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

    DebugServerPushEvent evt;
    evt.dataType            = Dia::Core::StringCRC("ai.utility");
    evt.payload["entities"] = entities;
    evt.payload["frame"]    = static_cast<Json::UInt64>(mFrameCounter);

    mAIInspectWriter.Send(evt);
}

// ---------------------------------------------------------------------------
// PushRules — every frame, change-detected via hash (Task 5)
// ---------------------------------------------------------------------------
void AIInspectorModule::PushRules()
{
    auto* entityMod = mEntityRef.Get();
    if (!entityMod)
        return;

    auto& domain = entityMod->GetDomain();

    Json::Value entities(Json::arrayValue);
    unsigned int hashAccum = 0;

    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity entity = domain.GetAliveEntity(i);
        if (!entity.IsValid())
            continue;

        const Dia::Rules::RuleSetComponent* comp =
            domain.GetComponent<Dia::Rules::RuleSetComponent>(entity);
        if (!comp)
            continue;

        const Dia::Rules::RuleSet* ruleSet = comp->GetRuleSet();
        if (!ruleSet)
            continue;

#ifdef DIA_DEBUG
        Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
        const int firedCount = ruleSet->GetLastFireReport(entries);

        hashAccum ^= static_cast<unsigned int>(entity.GetIndex()) * 2654435761u;
        hashAccum ^= static_cast<unsigned int>(firedCount) * 1234567891u;

        Json::Value entityEntry;
        entityEntry["id"]          = static_cast<Json::UInt>(entity.GetIndex());
        const char* dbgName        = domain.GetDebugName(entity);
        entityEntry["name"]        = dbgName ? dbgName : "";
        entityEntry["rules_fired"] = firedCount;

        Json::Value rulesArr(Json::arrayValue);
        for (uint32_t e = 0; e < entries.Size(); ++e)
        {
            const Dia::Rules::RuleSet::RuleFireEntry& entry = entries[e];

            Json::Value ruleEntry;
            ruleEntry["id"]   = static_cast<Json::UInt>(entry.ruleId.Value());
            ruleEntry["fired"] = true;

            Json::Value actionsArr(Json::arrayValue);
            for (uint32_t a = 0; a < entry.actions.Size(); ++a)
                actionsArr.append(static_cast<Json::UInt>(entry.actions[a].Value()));
            ruleEntry["actions"] = actionsArr;

            rulesArr.append(ruleEntry);
        }
        entityEntry["rules"] = rulesArr;
        entities.append(entityEntry);
#else
        // Release: no fire data, but still count entity for hash stability
        hashAccum ^= static_cast<unsigned int>(entity.GetIndex()) * 2654435761u;
#endif // DIA_DEBUG
    }

    hashAccum ^= static_cast<unsigned int>(entities.size()) * 987654321u;

    if (hashAccum == mRulesLastHash)
        return;
    mRulesLastHash = hashAccum;

    DebugServerPushEvent evt;
    evt.dataType              = Dia::Core::StringCRC("ai.rules");
    evt.payload["frame"]      = static_cast<Json::UInt64>(mFrameCounter);
    evt.payload["debug_only"] = true;
    evt.payload["entities"]   = entities;

    mAIInspectWriter.Send(evt);
}

// ---------------------------------------------------------------------------
// PushHTN — every frame, change-detected via hash (Task 6)
// ---------------------------------------------------------------------------
void AIInspectorModule::PushHTN()
{
    auto* entityMod = mEntityRef.Get();
    if (!entityMod)
        return;

    auto& domain = entityMod->GetDomain();

    Json::Value  entities(Json::arrayValue);
    unsigned int hashAccum = 0;

    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity entity = domain.GetAliveEntity(i);
        if (!entity.IsValid())
            continue;

        const Dia::HTN::HTNPlannerComponent* comp =
            domain.GetComponent<Dia::HTN::HTNPlannerComponent>(entity);
        if (!comp)
            continue;

        const uint32_t entityIdx = entity.GetIndex();
        EntityHTNState& state    = sHTNHistory[entityIdx];

        const bool hasPlan = comp->HasActivePlan();

        if (hasPlan)
        {
            const Dia::HTN::HTNPlan* plan = comp->GetActivePlan();
            const int  taskCount     = plan->GetTaskCount();
            const bool isComplete    = plan->IsComplete();
            const uint32_t currentOpCRC = isComplete
                ? 0u
                : plan->CurrentTask().operatorId.Value();

            const unsigned int planHash =
                static_cast<unsigned int>(taskCount) * 2654435761u
                ^ currentOpCRC * 1234567891u;

            if (planHash != state.lastPlanHash)
            {
                // Archive the current (open) entry if one exists
                if (!state.history.empty() && state.history.back().endFrame == -1)
                    state.history.back().endFrame = static_cast<int>(mFrameCounter);

                PlanHistoryEntry entry;
                entry.planIndex  = state.nextPlanIndex++;
                entry.startFrame = static_cast<int>(mFrameCounter);
                entry.endFrame   = -1;
                entry.reason     = state.history.empty() ? "initial" : "replan";
                entry.taskOperatorCRCs.push_back(currentOpCRC);

                state.history.push_back(std::move(entry));
                if (static_cast<int>(state.history.size()) > EntityHTNState::kMaxDepth)
                    state.history.pop_front();

                state.lastPlanHash = planHash;
            }

            hashAccum ^= entityIdx * 2654435761u ^ planHash;

            // Build entity JSON
            Json::Value entityEntry;
            entityEntry["id"]           = static_cast<Json::UInt>(entityIdx);
            const char* dbgName         = domain.GetDebugName(entity);
            entityEntry["name"]         = dbgName ? dbgName : "";
            entityEntry["has_plan"]     = true;
            entityEntry["task_count"]   = taskCount;
            entityEntry["current_task"] = static_cast<Json::UInt>(currentOpCRC);
            entityEntry["is_complete"]  = isComplete;
            // TODO: diverged requires IConditionContext — not available from inspector;
            //       emit false until HTNPlannerComponent caches diverged state.
            entityEntry["diverged"]     = false;

            Json::Value historyArr(Json::arrayValue);
            for (const PlanHistoryEntry& he : state.history)
            {
                Json::Value h;
                h["plan_index"]  = he.planIndex;
                h["start_frame"] = he.startFrame;
                h["end_frame"]   = he.endFrame;
                h["reason"]      = he.reason;
                h["task_count"]  = static_cast<int>(he.taskOperatorCRCs.size());

                Json::Value tasks(Json::arrayValue);
                for (uint32_t crc : he.taskOperatorCRCs)
                    tasks.append(static_cast<Json::UInt>(crc));
                h["tasks"] = tasks;

                historyArr.append(h);
            }
            entityEntry["plan_history"] = historyArr;
            entities.append(entityEntry);
        }
        else
        {
            // No active plan — close the open history entry if present
            if (!state.history.empty() && state.history.back().endFrame == -1)
                state.history.back().endFrame = static_cast<int>(mFrameCounter);

            hashAccum ^= entityIdx * 2654435761u;

            Json::Value entityEntry;
            entityEntry["id"]       = static_cast<Json::UInt>(entityIdx);
            const char* dbgName     = domain.GetDebugName(entity);
            entityEntry["name"]     = dbgName ? dbgName : "";
            entityEntry["has_plan"] = false;

            Json::Value historyArr(Json::arrayValue);
            for (const PlanHistoryEntry& he : state.history)
            {
                Json::Value h;
                h["plan_index"]  = he.planIndex;
                h["start_frame"] = he.startFrame;
                h["end_frame"]   = he.endFrame;
                h["reason"]      = he.reason;
                h["task_count"]  = static_cast<int>(he.taskOperatorCRCs.size());

                Json::Value tasks(Json::arrayValue);
                for (uint32_t crc : he.taskOperatorCRCs)
                    tasks.append(static_cast<Json::UInt>(crc));
                h["tasks"] = tasks;

                historyArr.append(h);
            }
            entityEntry["plan_history"] = historyArr;
            entities.append(entityEntry);
        }
    }

    const unsigned int newHash = hashAccum ^ static_cast<unsigned int>(entities.size());
    if (newHash == mHTNLastHash)
        return;
    mHTNLastHash = newHash;

    DebugServerPushEvent evt;
    evt.dataType            = Dia::Core::StringCRC("ai.htn");
    evt.payload["frame"]    = static_cast<Json::UInt64>(mFrameCounter);
    evt.payload["entities"] = entities;

    mAIInspectWriter.Send(evt);
}

} } // namespace Cluiche::AppFlow

namespace { using AIInspectorModule_ = Cluiche::AppFlow::AIInspectorModule; }
DIA_MODULE(AIInspectorModule_);
DIA_DESCRIBE(AIInspectorModule_::kTypeId,
    "Pushes AI inspector payloads (budget/utility/rules/HTN) to the debug server");

#endif // DIA_DEBUG
