#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

// Generic push event for SimPU→MainPU debug data forwarding.
// Both EntityInspectorModule and AIInspectorModule write events of this type
// to the "DebugServerPush" stream.  DebugServerHostModule drains them and
// calls NotifySubscribers(dataType, payload) — it need not know the sender.
struct DebugServerPushEvent
{
    Dia::Core::StringCRC dataType;
    Json::Value          payload;
};

} } // namespace Cluiche::AppFlow
