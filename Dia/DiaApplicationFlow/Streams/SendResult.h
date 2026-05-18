#pragma once

namespace Dia { namespace ApplicationFlow {

// SendResult — returned by EventStreamWriter::Send().
// Callers that don't need to distinguish modes can treat
// (result != kFailLoudRejected) as the success predicate.
enum class SendResult
{
    kDelivered,         // event was delivered to all active readers
    kDroppedOldest,     // buffer full (drop-oldest policy): oldest event discarded to make room
    kDroppedNewest,     // buffer full (drop-newest policy): incoming event discarded
    kBlockedThenDropped,// block policy: writer waited blockTimeoutMs, then dropped
    kFailLoudRejected,  // fail-loud policy: DIA_ASSERT in Debug, returns this code in Release
};

inline bool IsSuccess(SendResult r)
{
    return r != SendResult::kFailLoudRejected;
}

}} // namespace Dia::ApplicationFlow
