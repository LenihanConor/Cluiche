#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaTriggerScript/TriggerDef.h>

namespace Dia
{
    namespace TriggerScript
    {
        struct TriggerFiredEvent
        {
            Dia::Core::StringCRC triggerId;
            TriggerType          type = TriggerType::kState;
        };

    } // namespace TriggerScript
} // namespace Dia
