#pragma once

#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace BehaviourTree
    {
        class IBehaviourTreeEventListener
        {
        public:
            virtual ~IBehaviourTreeEventListener() = default;
            virtual void OnNodeEntered(Dia::Core::StringCRC nodeId) = 0;
            virtual void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) = 0;
            virtual void OnTreeCompleted(NodeResult result) = 0;
        };
    } // namespace BehaviourTree
} // namespace Dia
