#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/Bus.h>
#include <DiaMessageBus/BusHealthReporter.h>
#ifdef DIA_DEBUG
#include <DiaMessageBus/LedgerHistory.h>
#endif

namespace Dia::MessageBus {

// ---------------------------------------------------------------------------
// MessageBusModule
//
// Thin Dia::ApplicationFlow::Module wrapper around Bus. Owns one Bus
// instance and drives its flush from DoUpdate(). Follows the same
// implementation/module split as EntitySpawnerModule + EntitySpawnerImpl:
// almost all behaviour lives in Bus (directly unit-testable without the
// Module framework); this class only forwards the three lifecycle hooks.
// ---------------------------------------------------------------------------
class MessageBusModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kInstanceId;

    MessageBusModule();

    Bus&       GetBus();
    const Bus& GetBus() const;

#ifdef DIA_DEBUG
    // Debug-only: last kLedgerCapacity completed-tick ledger snapshots, for a
    // future visual debugger's History tab. Absent entirely in Release.
    const LedgerHistory& GetLedgerHistory() const;
#endif

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    Bus mBus;
    BusHealthReporter mHealthReporter{Dia::Core::StringCRC("MessageBusModule"), mBus};
#ifdef DIA_DEBUG
    LedgerHistory mLedgerHistory;
#endif
};

} // namespace Dia::MessageBus
