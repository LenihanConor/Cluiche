#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMessageBus/BusTypes.h>

namespace Dia::MessageBus {

    // One row per (type, pass) combination that flowed during a tick.
    struct LedgerMessageEntry {
        Dia::Core::StringCRC typeId;       // T::kTypeId — display only, NOT the routing key
        Dia::Core::StringCRC routerId;     // which router resolved this type's messages
        uint32_t             count      = 0;   // messages of this type processed this tick
        uint32_t             deliveries = 0;   // total handler invocations this tick
        Pass                  pass       = Pass::Primary;
    };

    // Snapshot of one completed tick's message flow. Bus double-buffers two of
    // these: one being built during the current flush, one exposed as the
    // last-completed tick via Bus::GetLastTickLedger().
    struct LedgerSnapshot {
        uint64_t tickIndex    = 0;
        uint64_t timestampUs  = 0;
        // 32 types x 2 passes max — correctly sized, do not change to 32.
        Dia::Core::Containers::DynamicArrayC<LedgerMessageEntry, 64> entries;
        uint32_t droppedCount = 0;
    };

} // namespace Dia::MessageBus
