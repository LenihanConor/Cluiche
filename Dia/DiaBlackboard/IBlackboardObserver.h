#pragma once
#include "DiaCore/CRC/StringCRC.h"

namespace Dia { namespace Blackboard {
    class IBlackboardObserver {
    public:
        virtual ~IBlackboardObserver() = default;
        virtual void OnSlotRegistered(Dia::Core::StringCRC key) = 0;
        virtual void OnSlotUnregistered(Dia::Core::StringCRC key) = 0;
    };
}}
