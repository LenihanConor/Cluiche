#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

struct BlackboardInspectEvent
{
    Json::Value payload;
};

} } // namespace Cluiche::AppFlow
