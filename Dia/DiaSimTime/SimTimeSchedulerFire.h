#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia::SimTime {

    // SimTimeSchedulerFire
    // -------------------------------------------------------------------------
    // Payload the SimTimeScheduler emits when a scheduled entry becomes due.
    // Kept in its own header (deliberately free of any SimTimeScheduler include)
    // so that EventStreamWriter<SimTimeSchedulerFire> / EventStreamStore<...>
    // (wired up in Task 3.2) can pull in just the payload type without dragging
    // in the whole scheduler.
    //
    // PD-001: identity is carried as StringCRC, never raw strings.
    // Trivially default-constructible so it can live inside a DynamicArrayC.
    struct SimTimeSchedulerFire
    {
        Core::StringCRC eventType;
        Core::StringCRC targetSystemId;
    };

} // namespace Dia::SimTime
