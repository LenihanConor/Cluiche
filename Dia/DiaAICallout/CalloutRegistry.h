#pragma once

#include <DiaAICallout/Callout.h>
#include <DiaAICallout/CalloutHandle.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::AICallout {

    // Internal slot — one entry in the fixed pool.
    struct CalloutSlot {
        Callout              callout;
        uint32_t             generation;    // 0 == never used; increments on every reuse
        bool                 live;
        bool                 claimed;
        Dia::Core::StringCRC claimerEntityId;
    };

    // Plain data struct that owns the fixed-capacity pool.
    // CalloutHandle stores a const pointer to this so it can validate its
    // generation and access the Callout without going through the full registry class.
    struct CalloutRegistryData {
        static const uint32_t kMaxCallouts = 64u;
        CalloutSlot mSlots[kMaxCallouts];
    };

    // Registry API: emit, query, claim, release, update.
    // No singletons, no static members — always explicitly constructed.
    class CalloutRegistry : public CalloutRegistryData {
    public:
        CalloutRegistry();

        // Post a callout. Returns a handle the emitter uses to release it.
        // Asserts and returns an invalid handle if the pool is full.
        CalloutHandle Emit(const Callout& callout);

        // Return the number of live (not-yet-expired) callouts.
        int GetLiveCount() const;
    };

} // namespace Dia::AICallout
