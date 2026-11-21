#pragma once
#include <cstdint>

namespace Dia { namespace ApplicationFlow {

// Event<T> — envelope wrapping a discrete typed event.
//
// Design note (hardening vs spec): senderCrc stores the raw 32-bit CRC value
// of the sending module's instanceId, NOT a full StringCRC (84B → ~24B/event).
// GetSenderCrc() returns the raw value for debug display.  StringCRC has no
// constructor from a raw uint32, so we do not reconstruct it here.
//
// sequence: per-stream monotonic counter, starting at 0.
// timestampUs: microseconds from TimeAbsolute::GetSystemTime() at Send().
//
// Trace-link fields (traceId, spanId, writeSteadyNs): writer's trace and span
// id at Send() time. Zero when the tracer is not started or no span is open.
// Always compiled; tracer-gated.
template<typename T>
struct Event
{
    int64_t      timestampUs   = 0;
    unsigned int senderCrc     = 0;
    uint64_t     sequence      = 0;
    uint64_t     traceId       = 0;
    uint64_t     spanId        = 0;
    uint64_t     writeSteadyNs = 0;
    T            payload{};

    unsigned int GetSenderCrc() const { return senderCrc; }
};

}} // namespace Dia::ApplicationFlow
