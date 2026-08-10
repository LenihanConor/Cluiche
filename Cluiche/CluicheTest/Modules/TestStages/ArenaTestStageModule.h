#pragma once

#include "Modules/TestStages/TestStageModuleBase.h"

#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaStateMachine/FlatStateMachine.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaTriggerScript/TriggerScriptModule.h>
#include <DiaTriggerScript/TriggerActionRegistry.h>
#include <DiaTriggerScript/ITriggerActionHandler.h>
#include <DiaObjective/ObjectiveSet.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <functional>
#include <memory>

namespace Dia::Observation::Metric { class Gauge; }

#ifdef DIA_DEBUG
#include <DiaApplicationFlow/ModuleRefV2.h>
#include "Modules/VisualDebuggerModule.h"
#endif

namespace CluicheTest {

// Forward declaration — EnemyAgent stores a back-pointer to its owning module so
// that static (non-capturing) FSM and action callbacks can reach module state.
class ArenaTestStageModule;

// ---------------------------------------------------------------------------
// EnemyAgent
//
// Per-enemy runtime state. Owns its own FSM, blackboard, condition bridge,
// utility scorer, and rule evaluator. Lives in a statically-sized pool
// (DynamicArrayC<EnemyAgent, 32>). All members are default-constructible.
//
// FSM OWNERSHIP NOTE:
//   FlatStateMachine<EnemyAgent> is not default-constructible (requires
//   machineId, StateMachineDefinition&&, and TContext&) and stores a
//   TContext& back into the owning EnemyAgent. It is therefore held via
//   std::unique_ptr and initialised in OnStart per enemy.
//
// DYNAMICARRAYC SAFETY:
//   DynamicArrayC uses memcpy for Add(value) and Assign(). Never call
//   Add(EnemyAgent) — use AddDefault() then configure the resulting slot
//   at mEnemies[mEnemies.Size()-1] directly. The pimpl members (Blackboard,
//   UtilitySet, RuleSet, RuleActionRegistry) and unique_ptrs would be
//   silently corrupted by a bitwise copy.
// ---------------------------------------------------------------------------
struct EnemyAgent
{
    Dia::Maths::Vector2D position;
    float                health    = 1.0f;
    Dia::Core::StringCRC waveTag;              // "enemy_wave1", "enemy_wave2", "enemy_wave3"

    // Back-pointer to the owning ArenaTestStageModule. Set in SpawnWave.
    // Used by static (non-capturing) FSM/action callbacks to reach module-level state.
    ArenaTestStageModule* modulePtr = nullptr;

    // Heap-allocated: not default-constructible; also stores TContext& back to this struct.
    std::unique_ptr<Dia::StateMachine::FlatStateMachine<EnemyAgent>> fsm;

    Dia::Blackboard::Blackboard blackboard;
    // Heap-allocated: requires explicit void* data at construction (wired in OnStart).
    std::unique_ptr<Dia::Condition::ConditionRegistry> conditionRegistry;

    Dia::UtilityAI::UtilitySet    utilitySet;
    Dia::Rules::RuleSet           ruleSet;
    Dia::Rules::RuleActionRegistry ruleActionRegistry;

    bool alive                = true;
    bool desperateChargeFired = false;
};

// ---------------------------------------------------------------------------
// ArenaTestStageModule
//
// Multi-system E2E integration stage. Runs on SimPU.
// Exercises TriggerScript, Objective, Blackboard, StateMachine, UtilityAI,
// and Rules across a three-wave arena scenario.
// ---------------------------------------------------------------------------
class ArenaTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Multi-system AI/progression E2E: TriggerScript + Objective + Blackboard + StateMachine + UtilityAI + Rules";

    explicit ArenaTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~ArenaTestStageModule() override;

    // Called by the static EnemyAction_DespCharge free function (cannot access private members
    // of ArenaTestStageModule; the only caller is the static callback in the .cpp).
    void NotifyDespChargeFired()
    {
        ++mDespChargesFired;
        mDespChargeTriggered = true;
    }

    // -----------------------------------------------------------------------
    // ArenaActionHandler
    //
    // Single-callback ITriggerActionHandler. The module owns four instances
    // (one per action type) and registers them with mTriggerActionRegistry
    // in OnStart. Callbacks capture `this` and delegate to private helpers.
    //
    // Default-constructible (std::function is default-constructible as empty)
    // so instances can be held as plain value members and configured later.
    // -----------------------------------------------------------------------
    class ArenaActionHandler : public Dia::TriggerScript::ITriggerActionHandler
    {
    public:
        using Callback = std::function<void(const Dia::TriggerScript::ActionContext&)>;

        ArenaActionHandler() = default;
        explicit ArenaActionHandler(Callback cb) : mCallback(std::move(cb)) {}

        void SetCallback(Callback cb) { mCallback = std::move(cb); }

        void Execute(const Dia::TriggerScript::ActionContext& ctx) override
        {
            if (mCallback)
                mCallback(ctx);
        }

    private:
        Callback mCallback;
    };

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int         GetBudgetFrames() const override { return 900; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // --- Action handlers (owned by module; registered with mTriggerActionRegistry in OnStart) ---
    // SetCallback() is called in the constructor body to bind lambdas that capture `this`.
    ArenaActionHandler mSpawnHandler;
    ArenaActionHandler mFireEventHandler;
    ArenaActionHandler mChangeObjectiveHandler;
    ArenaActionHandler mGiveResourcesHandler;

    // --- Trigger system ---
    // TriggerScriptModule is default-constructible. SetActionRegistry / SetConditionContext /
    // SetSpatialModule / LoadFromJson are called before Tick() in OnStart.
    Dia::TriggerScript::TriggerScriptModule   mTriggerScript;
    Dia::TriggerScript::TriggerActionRegistry mTriggerActionRegistry;

    // --- Objective system ---
    // ObjectiveSet is default-constructible. Populated via LoadFromJson in OnStart.
    Dia::Objective::ObjectiveSet mObjectives;

    // --- Global blackboard + condition bridge ---
    Dia::Blackboard::Blackboard mGlobalBlackboard;
    // ConditionRegistry requires void* data: initialised in OnStart once blackboard slots are wired.
    std::unique_ptr<Dia::Condition::ConditionRegistry> mGlobalConditionRegistry;

    // --- Player ---
    Dia::Maths::Vector2D mPlayerPosition;
    Dia::Entity::Entity  mPlayerEntity;

    // --- Spatial ---
    // mSpatialDomain is declared first: EntitySpatialModule constructor takes a Domain& reference.
    // EntitySpatialModule is non-default-constructible and non-movable, so it is created in OnStart
    // once the arena world bounds (SquareDef) are known.
    Dia::Entity::Domain                                      mSpatialDomain;
    std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> mEntitySpatialModule;

    // --- Enemy pool ---
    // Fixed-size buffer of 32 EnemyAgents. All slots are default-constructed at module
    // construction time. mEnemyCount tracks how many have been activated via SpawnWave().
    // DynamicArrayC<EnemyAgent> cannot be used here because EnemyAgent's unique_ptr members
    // delete the copy-assign operator that AddDefault() requires.
    static constexpr unsigned int kMaxEnemies = 32;
    EnemyAgent   mEnemies[kMaxEnemies];
    unsigned int mEnemyCount = 0;

    // --- Wave / progression tracking ---
    float mWave1ClearDelay     = -1.f; // countdown in seconds; -1 = not yet started
    float mWave2ClearDelay     = -1.f;
    int   mTotalKills          = 0;
    int   mWavesCompleted      = 0;
    int   mDespChargesFired    = 0;
    bool  mDespChargeTriggered = false;
    bool  mPowerupCollected    = false;
    bool  mVictory             = false;
    bool  mAllPassed           = false;

    // --- Frame counter ---
    unsigned int mFrameCount = 0;

    // --- Metrics ---
    Dia::Observation::Metric::Gauge* mMetricTotalKills          = nullptr;
    Dia::Observation::Metric::Gauge* mMetricWavesCompleted      = nullptr;
    Dia::Observation::Metric::Gauge* mMetricPowerupCollected     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricDespChargesFired     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricTotalFrames          = nullptr;

    // --- Private helpers: setup ---
    void LoadTriggerScript();
    void LoadObjectives();
    void RegisterCheckpoints();
    void RegisterMetrics();

    // --- Private helpers: per-frame ---
    void UpdateEnemyAI(float deltaTime);
    bool AllCheckpointsPassed() const;

    // --- Private helpers: action callbacks (bound to handlers in constructor) ---
    void SpawnWave(Dia::Core::StringCRC tag, int count);
    void OnFireEvent(Dia::Core::StringCRC eventId);
    void OnChangeObjectiveState(Dia::Core::StringCRC objectiveId, Dia::Core::StringCRC state);

#ifdef DIA_DEBUG
    void DrawDebugImGui();
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
#endif
};

} // namespace CluicheTest
