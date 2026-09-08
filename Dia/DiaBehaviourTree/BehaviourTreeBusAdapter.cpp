#include <DiaBehaviourTree/BehaviourTreeBusAdapter.h>
#include <DiaBehaviourTree/Messages/behaviourtree_messages.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaMessageBus/Bus.h>

namespace Dia { namespace BehaviourTree {

    BehaviourTreeBusAdapter::BehaviourTreeBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner)
        : mBus(bus)
        , mOwner(owner)
    {
        // Registers all three message types + producer metadata. RegisterType
        // is a no-op (returns false, no assert) if already registered, so
        // this is safe even if something else already registered these types.
        Messages::RegisterMessages(mBus, Messages::Handlers{});
    }

    void BehaviourTreeBusAdapter::OnNodeEntered(Dia::Core::StringCRC nodeId)
    {
        Messages::NodeEnteredEvent msg;
        msg.nodeId = nodeId;
        mBus.Post<Messages::NodeEnteredEvent>(Dia::Entity::MakeEntityAddress(mOwner), msg);
    }

    void BehaviourTreeBusAdapter::OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result)
    {
        Messages::NodeCompletedEvent msg;
        msg.nodeId = nodeId;
        msg.result = result;
        mBus.Post<Messages::NodeCompletedEvent>(Dia::Entity::MakeEntityAddress(mOwner), msg);
    }

    void BehaviourTreeBusAdapter::OnTreeCompleted(NodeResult result)
    {
        Messages::TreeCompletedEvent msg;
        msg.result = result;
        mBus.Post<Messages::TreeCompletedEvent>(Dia::Entity::MakeEntityAddress(mOwner), msg);
    }

} } // namespace Dia::BehaviourTree
