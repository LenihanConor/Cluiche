#include "DiaMessageBus/BusHealthReporter.h"
#include "DiaMessageBus/Bus.h"

namespace Dia::MessageBus {

namespace {
    // Headroom threshold: flag Degraded once a fixed-capacity table crosses
    // this fraction full, giving a warning window before it actually fails
    // to register/subscribe.
    constexpr float kDegradedOccupancyFraction = 0.9f;
}

BusHealthReporter::BusHealthReporter(Dia::Core::StringCRC reporterName, const Bus& bus)
    : mName(reporterName)
    , mBus(bus)
{
    SetOK();
}

Dia::Core::StringCRC BusHealthReporter::GetReporterName() const
{
    return mName;
}

void BusHealthReporter::Check()
{
    const uint32_t typeCount    = mBus.GetRegisteredTypeCount();
    const uint32_t typeCapacity = mBus.GetTypeCapacity();
    if (typeCapacity > 0 && static_cast<float>(typeCount) >= kDegradedOccupancyFraction * static_cast<float>(typeCapacity))
    {
        SetDegraded(Dia::Core::StringCRC("type_registry_near_capacity"));
        return;
    }

    const uint32_t handlerCount    = mBus.GetHandlerCount();
    const uint32_t handlerCapacity = mBus.GetHandlerCapacity();
    if (handlerCapacity > 0 && static_cast<float>(handlerCount) >= kDegradedOccupancyFraction * static_cast<float>(handlerCapacity))
    {
        SetDegraded(Dia::Core::StringCRC("handler_pool_near_capacity"));
        return;
    }

    if (mBus.GetLastTickLedger().droppedCount > 0)
    {
        IncrementWarnings();
        SetDegraded(Dia::Core::StringCRC("messages_dropped_last_tick"));
        return;
    }

    SetOK();
}

} // namespace Dia::MessageBus
