#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include <cstdint>

namespace Dia::RigidBody2D {

// Canonical ordering: lower UID first, so (A,B) == (B,A).
// Uses stable unique IDs instead of pointer addresses to avoid
// ABA problems when bodies are removed and memory is reused.
struct BodyPairKey {
    uint32_t uidLo = 0;
    uint32_t uidHi = 0;
    const Body2DBase* ptrLo = nullptr;
    const Body2DBase* ptrHi = nullptr;

    BodyPairKey() = default;
    BodyPairKey(const Body2DBase* a, const Body2DBase* b)
    {
        uint32_t ua = a ? a->GetUniqueId() : 0;
        uint32_t ub = b ? b->GetUniqueId() : 0;
        if (ua <= ub) { uidLo = ua; uidHi = ub; ptrLo = a; ptrHi = b; }
        else          { uidLo = ub; uidHi = ua; ptrLo = b; ptrHi = a; }
    }

    bool ContainsBody(const Body2DBase* body) const
    {
        uint32_t uid = body ? body->GetUniqueId() : 0;
        return uidLo == uid || uidHi == uid;
    }

    bool operator==(const BodyPairKey& other) const
    {
        return uidLo == other.uidLo && uidHi == other.uidHi;
    }

    unsigned int Value() const
    {
        return static_cast<unsigned int>(
            (static_cast<uint64_t>(uidLo) * 2654435761u) ^
            (static_cast<uint64_t>(uidHi) * 2246822519u));
    }
};

struct CollisionPairState {
    bool wasContacting = false;
};

} // namespace Dia::RigidBody2D
