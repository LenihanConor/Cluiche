#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow {

// OverflowPolicy — controls what happens when an EventStreamStore's ring
// buffer is full and a new event arrives.
enum class OverflowPolicy
{
    kDropOldest,    // default: drop the oldest buffered event to make room
    kDropNewest,    // discard the incoming event; existing buffer unchanged
    kBlock,         // block the writer until space is available or blockTimeoutMs elapses
    kFailLoud,      // DIA_ASSERT in Debug; return kFailLoudRejected in Release
};

// Parse an OverflowPolicy from a manifest string CRC.
// Known values: "drop-oldest", "drop-newest", "block", "fail-loud".
// Returns kDropOldest for any unrecognised value.
inline OverflowPolicy ParseOverflowPolicy(const Dia::Core::StringCRC& name)
{
    static const Dia::Core::StringCRC kDropOldest("drop-oldest");
    static const Dia::Core::StringCRC kDropNewest("drop-newest");
    static const Dia::Core::StringCRC kBlock("block");
    static const Dia::Core::StringCRC kFailLoud("fail-loud");

    if (name == kDropNewest) return OverflowPolicy::kDropNewest;
    if (name == kBlock)      return OverflowPolicy::kBlock;
    if (name == kFailLoud)   return OverflowPolicy::kFailLoud;
    return OverflowPolicy::kDropOldest;
}

}} // namespace Dia::ApplicationFlow
