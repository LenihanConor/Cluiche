#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace TriggerScript
    {
        struct ActionContext
        {
            Dia::Core::StringCRC triggerId;
            const Json::Value&   params;
        };

        class ITriggerActionHandler
        {
        public:
            virtual ~ITriggerActionHandler() = default;
            virtual void Execute(const ActionContext& ctx) = 0;
        };

    } // namespace TriggerScript
} // namespace Dia
