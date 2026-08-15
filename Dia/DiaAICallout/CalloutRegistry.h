#pragma once

#include <DiaAICallout/Callout.h>
#include <DiaAICallout/CalloutHandle.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::AICallout {

    // Filter passed to Query(). All matching callouts must satisfy every criterion.
    struct QueryFilter {
        Dia::Core::StringCRC  kind;     // required — callout.kind must equal this
        Dia::Maths::Vector2D  origin;   // centre of the search area
        float                 radius;   // world-unit search radius
        Dia::Core::StringCRC  faction;  // StringCRC::kZero = accept any faction
    };

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

        // Append handles for every unclaimed live callout that matches filter.
        // Results are unordered — caller sorts by their own heuristic (SD-005).
        // outResults is appended to, never cleared (caller's responsibility).
        template <unsigned int N>
        void Query(const QueryFilter& filter,
                   Dia::Core::Containers::DynamicArrayC<CalloutHandle, N>& outResults) const;
    };

    // --- template implementation -----------------------------------------------

    template <unsigned int N>
    void CalloutRegistry::Query(const QueryFilter& filter,
                                Dia::Core::Containers::DynamicArrayC<CalloutHandle, N>& outResults) const
    {
        const float radiusSq = filter.radius * filter.radius;

        for (uint32_t i = 0u; i < kMaxCallouts; ++i)
        {
            const CalloutSlot& slot = mSlots[i];

            if (!slot.live)                           continue; // slot vacant
            if (slot.claimed)                         continue; // SD-004: claimed = invisible
            if (slot.callout.kind != filter.kind)     continue; // kind mismatch

            // Distance check — squared to avoid sqrt
            const float distSq = slot.callout.position.SquareDistanceTo(filter.origin);
            if (distSq > radiusSq)                    continue;

            // Faction check: pass when filter=any, exact match, or callout=any
            const bool filterAny  = (filter.faction          == Dia::Core::StringCRC::kZero);
            const bool exactMatch = (slot.callout.faction     == filter.faction);
            const bool calloutAny = (slot.callout.faction     == Dia::Core::StringCRC::kZero);
            if (!filterAny && !exactMatch && !calloutAny)     continue;

            outResults.Add(CalloutHandle(i, slot.generation, this));
        }
    }

} // namespace Dia::AICallout
