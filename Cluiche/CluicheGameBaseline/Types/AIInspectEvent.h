#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

struct AIInspectEvent
{
    Dia::Core::StringCRC dataType;
    Json::Value          payload;
};

} } // namespace Cluiche::AppFlow
