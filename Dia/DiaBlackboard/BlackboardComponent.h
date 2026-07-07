#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaBlackboard/Blackboard.h"

namespace Dia { namespace Blackboard {

    class BlackboardComponent
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        Blackboard& GetBlackboard();
        const Blackboard& GetBlackboard() const;

    private:
        Blackboard mBlackboard;
    };

}}
