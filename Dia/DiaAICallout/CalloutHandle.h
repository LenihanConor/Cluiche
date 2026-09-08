#pragma once

#include <cstdint>

namespace Dia::AICallout {

    struct Callout;
    struct CalloutRegistryData; // defined by the registry implementation

    class CalloutRegistry; // forward-declare for friend access

    // Owning, safe reference to a posted callout.
    // IsValid() returns false once the callout has expired or been cancelled.
    // Only CalloutRegistry may construct a valid handle.
    class CalloutHandle {
    public:
        static const uint32_t kInvalidIndex      = 0xFFFFFFFFu;
        static const uint32_t kInvalidGeneration = 0u;

        // Default-constructed handle is always invalid.
        CalloutHandle();

        // Returns false if the handle is stale (expired, cancelled, or default-constructed).
        bool IsValid() const;

        // Returns true if another entity has exclusively claimed this callout.
        // Returns false if the handle is invalid.
        bool IsClaimed() const;

        // Returns the callout data, or nullptr if the handle is invalid.
        const Callout* Get() const;

        uint32_t GetIndex() const;
        uint32_t GetGeneration() const;

    private:
        // Only CalloutRegistry may construct a valid handle.
        CalloutHandle(uint32_t index, uint32_t generation, const CalloutRegistryData* registry);

        friend class CalloutRegistry;

        uint32_t                    mIndex;
        uint32_t                    mGeneration;
        const CalloutRegistryData*  mRegistry;
    };

} // namespace Dia::AICallout
