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
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaSimTime/DiaSimTimeModule.h>
#include <DiaSimTime/ISimTimeBudgetedSystem.h>

#include <functional>
#include <memory>
#include <string>

namespace Dia::Observation::Metric { class Gauge; }

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/Camera2DModule.h"
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
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

    // -----------------------------------------------------------------------
    // Budgeted system wrappers
    //
    // Registered with the umbrella DiaSimTimeModule (SimPU sibling module) so
    // TriggerScript/Objective/EnemyAI updates run through the real per-tick
    // budget gate loop instead of being called directly from OnUpdate. Three
    // separate systems sharing DiaSimTimeModule's per-tier pools give genuine
    // multi-system contention with production code (not synthetic stand-ins).
    // Nested classes so they can call the owning module's private helpers.
    // -----------------------------------------------------------------------
    class TriggerScriptBudgetedSystem : public Dia::SimTime::ISimTimeBudgetedSystem
    {
    public:
        explicit TriggerScriptBudgetedSystem(ArenaTestStageModule* owner) : mOwner(owner) {}
        Dia::Core::StringCRC GetSystemId() const override
        {
            static const Dia::Core::StringCRC id("Arena.TriggerScript");
            return id;
        }
        Dia::SimTime::SimTimePriority GetPriority() const override { return Dia::SimTime::SimTimePriority::kHigh; }
        void UpdateBudgeted(float budgetMs) override
        {
            if (budgetMs <= 0.f) return;
            mOwner->mTriggerScript.Tick(mOwner->mLastDeltaTime);
        }
    private:
        ArenaTestStageModule* mOwner;
    };

    class EnemyAIBudgetedSystem : public Dia::SimTime::ISimTimeBudgetedSystem
    {
    public:
        explicit EnemyAIBudgetedSystem(ArenaTestStageModule* owner) : mOwner(owner) {}
        Dia::Core::StringCRC GetSystemId() const override
        {
            static const Dia::Core::StringCRC id("Arena.EnemyAI");
            return id;
        }
        Dia::SimTime::SimTimePriority GetPriority() const override { return Dia::SimTime::SimTimePriority::kNormal; }
        void UpdateBudgeted(float budgetMs) override
        {
            if (budgetMs <= 0.f) return;
            mOwner->UpdateEnemyAI(mOwner->mLastDeltaTime);
        }
    private:
        ArenaTestStageModule* mOwner;
    };

    class ObjectivesBudgetedSystem : public Dia::SimTime::ISimTimeBudgetedSystem
    {
    public:
        explicit ObjectivesBudgetedSystem(ArenaTestStageModule* owner) : mOwner(owner) {}
        Dia::Core::StringCRC GetSystemId() const override
        {
            static const Dia::Core::StringCRC id("Arena.Objectives");
            return id;
        }
        Dia::SimTime::SimTimePriority GetPriority() const override { return Dia::SimTime::SimTimePriority::kBackground; }
        void UpdateBudgeted(float budgetMs) override
        {
            if (budgetMs <= 0.f) return;
            if (mOwner->mGlobalConditionRegistry)
                mOwner->mObjectives.Evaluate(*mOwner->mGlobalConditionRegistry);
        }
    private:
        ArenaTestStageModule* mOwner;
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

    // --- DiaSimTime budget wiring ---
    // Sibling umbrella module (declared before this one in the manifest, with
    // this module depending on it — see arena_test_stage.diaapp). Cached
    // deltaTime: the three wrapped systems run inside DiaSimTimeModule's gate
    // loop (which ticks before this module's own OnUpdate each frame, per the
    // manifest dependency order), one tick behind whatever OnUpdate last saw.
    // SimPU is fixed-timestep, so this is a constant value in practice.
    Dia::ApplicationFlow::ModuleRef<Dia::SimTime::DiaSimTimeModule> mSimTimeRef{this};
    TriggerScriptBudgetedSystem mTriggerScriptBudgeted{this};
    EnemyAIBudgetedSystem       mEnemyAIBudgeted{this};
    ObjectivesBudgetedSystem    mObjectivesBudgeted{this};
    float mLastDeltaTime = 1.0f / 30.0f;
    bool  mSimTimeSystemsRegistered = false;

    // --- Last trigger that fired (displayed in ImGui sidebar) ---
    std::string mLastTriggerLabel;

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
    friend class ArenaDebugLayer;

    class ArenaDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit ArenaDebugLayer(const ArenaTestStageModule* module) : mModule(module) {}

        Dia::Core::StringCRC GetLayerName() const override
        {
            return Dia::Core::StringCRC("CluicheTest.Arena");
        }

        void Draw(Dia::Core::IDebugDraw& draw) override;

    private:
        const ArenaTestStageModule* mModule = nullptr;
    };
    ArenaDebugLayer mDebugLayer{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};

    // Arena's world is small-unit scale (bounds (-10,-10)->(10,10)); the shared
    // Camera2D default zoom (1.0 = 1 world unit per pixel) renders it as a few
    // pixels wide. Zoomed in for the duration of this stage; restored on exit
    // since Camera2DModule is global (not stage-scoped) and other stages share it.
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Camera2DModule> mCameraRef{this};
    float mSavedCameraZoom = 1.0f;
#endif
};

} // namespace CluicheTest
