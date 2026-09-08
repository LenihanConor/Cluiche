#pragma once

#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaEntity/Entity.h>

namespace Dia { namespace MessageBus { class Bus; } }

namespace Dia { namespace BehaviourTree {

    // -----------------------------------------------------------------------
    // BehaviourTreeBusAdapter
    //
    // One more IBehaviourTreeEventListener — registered via
    // BehaviourTreeComponent::AddEventListener alongside any other direct
    // listener (e.g. debug logging), it does not replace them. Forwards each
    // synchronous OnNodeEntered/OnNodeCompleted/OnTreeCompleted callback onto
    // the shared DiaMessageBus::Bus via Bus::Post<T>() — never Broadcast<T>().
    //
    // Unlike DiaEconomy's EconomyBusAdapter (global Broadcast — an
    // EconomyInstance is not entity-scoped), a BehaviourTreeComponent belongs
    // to exactly one entity, so every forwarded message is addressed to that
    // specific entity via Dia::Entity::MakeEntityAddress(mOwner).
    //
    // BehaviourTreeComponent's listener interface carries no entity handle by
    // design (see IBehaviourTreeEventListener.h) — the entity identity lives
    // here, supplied at construction by whoever attaches the component to an
    // entity (blueprint loading, spawner, etc.).
    //
    // DiaBehaviourTree's core types (IBehaviourTreeEventListener.h,
    // BehaviourTreeComponent, etc.) have no DiaMessageBus/DiaEntity include
    // or dependency beyond IComponent/Entity — only this adapter depends on
    // DiaMessageBus and Dia::Entity::MakeEntityAddress.
    // -----------------------------------------------------------------------
    class BehaviourTreeBusAdapter : public IBehaviourTreeEventListener
    {
    public:
        BehaviourTreeBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner);

        void OnNodeEntered(Dia::Core::StringCRC nodeId) override;
        void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) override;
        void OnTreeCompleted(NodeResult result) override;

    private:
        Dia::MessageBus::Bus& mBus;
        Dia::Entity::Entity   mOwner;
    };

} } // namespace Dia::BehaviourTree
