#pragma once

#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::MessageBus { class Bus; }

namespace Dia::MessageBus {

    // -----------------------------------------------------------------------
    // BusHealthReporter
    //
    // Watches a Bus for the two observable failure modes fixed-capacity
    // registration tables can hit: the type registry (RegisterType<T>,
    // capacity Bus::GetTypeCapacity()) or the handler pool (Subscribe<T>,
    // capacity Bus::GetHandlerCapacity()) approaching full — plus sustained
    // message drops (GetLastTickLedger().droppedCount), which usually means
    // an OverflowPolicy::DropOldest type is being posted faster than it's
    // drained.
    //
    // Call Check() once per tick (e.g. from whoever drives Bus::Update(),
    // such as MessageBusModule::DoUpdate) to re-evaluate. Degraded/Failing
    // status reflects only the most recently Check()-ed tick — it clears
    // back to OK on a tick with no drops and headroom in both tables,
    // matching GetLastTickLedger()'s own last-tick-only semantics.
    // -----------------------------------------------------------------------
    class BusHealthReporter : public Dia::Observation::Health::HealthReporterBase
    {
    public:
        explicit BusHealthReporter(Dia::Core::StringCRC reporterName, const Bus& bus);
        ~BusHealthReporter() override = default;

        Dia::Core::StringCRC GetReporterName() const override;

        // Call once per sim tick (typically right after Bus::Update()) to
        // re-evaluate capacity headroom and last-tick drops.
        void Check();

    private:
        Dia::Core::StringCRC mName;
        const Bus&            mBus;
    };

} // namespace Dia::MessageBus
