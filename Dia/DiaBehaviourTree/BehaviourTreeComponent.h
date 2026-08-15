#pragma once

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/DecoratorRegistry.h>

// Forward declarations
namespace Dia { namespace Blackboard { class Blackboard; } }

namespace Dia
{
    namespace BehaviourTree
    {
        class IBehaviourTreeEventListener;  // defined in IBehaviourTreeEventListener.h (Task 8)

        //-------------------------------------------------------------------------------------------
        // BehaviourTreeComponent
        //
        // IComponent that drives per-entity behaviour tree evaluation.
        // Owns the execution cursor (stack + per-node state). Shared asset must outlive component.
        //
        // PD-001: kUniqueId uses StringCRC (via DIA_COMPONENT macro).
        // PD-004: No STL in public API.
        // AD-003: Namespace Dia::BehaviourTree::.
        // AD-004: Proper IComponent — not a singleton.
        //-------------------------------------------------------------------------------------------
        class BehaviourTreeComponent : public Dia::Entity::IComponent
        {
            DIA_COMPONENT(BehaviourTreeComponent, "behaviour-tree-component", 1)
            DIA_READONLY

        public:
            BehaviourTreeComponent();
            ~BehaviourTreeComponent();

            // Shared asset — must outlive the component. Caller retains ownership.
            void SetAsset(const BehaviourTreeAsset* asset);

            // Blackboard — must outlive the component.
            void SetBlackboard(Dia::Blackboard::Blackboard* blackboard);

            // Action registry — must outlive the component.
            void SetActionRegistry(const ActionRegistry* registry);

            // Context passed verbatim to ActionFn.
            void SetActionContext(void* context);

            // Decorator registry — optional; for custom decorators.
            void SetDecoratorRegistry(const DecoratorRegistry* registry);

            // Listener management — no-ops until Task 8 implements IBehaviourTreeEventListener.
            void AddEventListener(IBehaviourTreeEventListener* listener);
            void RemoveEventListener(IBehaviourTreeEventListener* listener);

            // Advance evaluation by one node. Returns kRunning while mid-execution.
            NodeResult Tick(float deltaTime);

            // Restart evaluation from the root. Clears cursor and all decorator state.
            void Reset();

            bool       IsComplete()  const;   // root returned kSuccess or kFailure last tick
            NodeResult LastResult()  const;
            bool       HasAsset()    const;

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace BehaviourTree
} // namespace Dia
