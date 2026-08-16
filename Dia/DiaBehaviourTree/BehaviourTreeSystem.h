#pragma once

#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace BehaviourTree
    {
        class BehaviourTreeComponent;

        //-------------------------------------------------------------------------------------------
        // BehaviourTreeSystem
        //
        // IAIBudgetedSystem that drives a pool of BehaviourTreeComponents each frame.
        // Components are ticked in round-robin order; the cursor advances across frames so that
        // every registered component eventually receives CPU time.
        //
        // Budget enforcement: elapsed time is measured per-component-tick via chrono.
        // A component that has !HasAsset() is skipped (index still advances).
        // Never interrupts a component mid-node — budget is checked before each component tick.
        //-------------------------------------------------------------------------------------------
        class BehaviourTreeSystem : public Dia::AIBudget::IAIBudgetedSystem
        {
        public:
            static const Dia::Core::StringCRC kUniqueId;

            BehaviourTreeSystem();
            ~BehaviourTreeSystem();

            void Register(BehaviourTreeComponent* component);
            void Unregister(BehaviourTreeComponent* component);
            int  GetRegisteredCount() const;

            // Tick registered components in round-robin order until budgetMs is exhausted.
            void Update(float budgetMs, float deltaTime);

            // IAIBudgetedSystem — calls Update(budgetMs, mLastDeltaTime).
            Dia::Core::StringCRC GetSystemId() const override;
            void UpdateBudgeted(float budgetMs) override;

            // Must be called before UpdateBudgeted if deltaTime matters for decorator accumulators.
            void SetDeltaTime(float dt);

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace BehaviourTree
} // namespace Dia
