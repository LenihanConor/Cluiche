#include "DiaBlackboard/BlackboardComponent.h"

namespace Dia { namespace Blackboard {

    const Dia::Core::StringCRC BlackboardComponent::kUniqueId("BlackboardComponent");

    Blackboard& BlackboardComponent::GetBlackboard()
    {
        return mBlackboard;
    }

    const Blackboard& BlackboardComponent::GetBlackboard() const
    {
        return mBlackboard;
    }

}}
