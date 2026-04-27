#pragma once

#include <cstdint>

namespace Dia::RigidBody2D {

struct TriggerPairKey {
    uint32_t triggerUid = 0;
    uint32_t bodyUid    = 0;

    TriggerPairKey() = default;
    TriggerPairKey(uint32_t tUid, uint32_t bUid) : triggerUid(tUid), bodyUid(bUid) {}

    bool operator==(const TriggerPairKey& other) const
    {
        return triggerUid == other.triggerUid && bodyUid == other.bodyUid;
    }

    unsigned int Value() const
    {
        return static_cast<unsigned int>(
            (static_cast<uint64_t>(triggerUid) * 2654435761u) ^
            (static_cast<uint64_t>(bodyUid) * 2246822519u));
    }
};

} // namespace Dia::RigidBody2D
