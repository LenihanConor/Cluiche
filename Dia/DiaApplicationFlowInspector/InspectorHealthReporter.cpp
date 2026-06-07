#include "DiaApplicationFlowInspector/InspectorHealthReporter.h"

namespace Dia { namespace Editor {

InspectorHealthReporter::InspectorHealthReporter(const bool& isConnected, const bool& isActive)
    : mIsConnected(isConnected)
    , mIsActive(isActive)
{}

Dia::Core::StringCRC InspectorHealthReporter::GetReporterName() const
{
    return Dia::Core::StringCRC("DiaApplicationFlowInspector");
}

Dia::Observation::Health::Health InspectorHealthReporter::Report() const
{
    using namespace Dia::Observation::Health;

    if (!mIsConnected)
    {
        return Health{ HealthStatus::kFailing, 1u, 0u, Dia::Core::StringCRC("Disconnected") };
    }

    if (!mIsActive)
    {
        return Health{ HealthStatus::kDegraded, 0u, 1u, Dia::Core::StringCRC("ConnectedNoData") };
    }

    return Health{ HealthStatus::kOK, 0u, 0u, Dia::Core::StringCRC("") };
}

}} // namespace Dia::Editor
