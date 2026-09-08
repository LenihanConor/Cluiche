#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <optional>

namespace Dia::SimTime {

    // SimTimeDomainRegistry
    // -------------------------------------------------------------------------
    // Manages a *named tree* of SimTimeDomain instances so pause / slow-mo /
    // fast-forward can be applied to any subtree independently of the whole
    // world (e.g. "the boss arena" pauses without pausing the world).
    //
    // World ownership: the SimPU owns exactly ONE world SimTimeDomain instance
    // (ProcessingUnit::GetWorldDomain()); the registry binds to that
    // externally-owned instance via its constructor and NEVER constructs or
    // copies its own world. kWorldId is externally clocked by the owning PU, so
    // TickAll() never advances it (the registry does not own it).
    //
    // Hierarchy semantics:
    //   * Child inherits parent pause: if any ancestor (walking up via parentId,
    //     including kWorldId) is paused, TickAll() does not advance that child
    //     this call, even if the child's own IsPaused() is false.
    //   * Independent scale composes: a child's effective advance-per-tick =
    //     (product of all ancestors' scale, including kWorldId's) x the child's
    //     own locally-set scale. Composition is re-applied to the underlying
    //     SimTimeDomain before each Tick() and then the local scale is restored,
    //     so SimTimeDomain::GetScale() keeps reporting the caller's local intent.
    //     (TimeServer's one-tick SetScale latency is preserved.)
    class SimTimeDomainRegistry
    {
    public:
        // "world" / "" — not literal constexpr StringCRC (CRC::Calc is a runtime
        // table lookup, not constexpr); declared here, defined in the .cpp,
        // matching the StringCRC::kZero precedent.
        static const Core::StringCRC kWorldId;
        static const Core::StringCRC kNoParent;

        static constexpr int kMaxDomains = 32;

        // Binds to the externally-owned world domain (dependency injection).
        explicit SimTimeDomainRegistry(SimTimeDomain& worldDomain);

        // Creates a new NON-world sub-domain. parentId defaults to the world
        // root; kNoParent and kWorldId both mean "parent is the world domain".
        // Ticks at the same granularity (hz) as the world domain and starts at
        // the world's current time. Returns a stable reference (fixed-capacity
        // storage; never relocates).
        SimTimeDomain&  Create(Core::StringCRC id, Core::StringCRC parentId = kNoParent);

        // Returns a pointer to the domain, or nullptr if not found.
        // Find(kWorldId) returns the externally-owned world domain.
        SimTimeDomain*  Find(Core::StringCRC id);

        // Destroys a NON-world sub-domain (no-op / assert for kWorldId — the
        // world root is externally owned and cannot be destroyed here).
        void            Destroy(Core::StringCRC id);

        // Advances all NON-world domains in creation order. kWorldId is
        // externally clocked by the owning ProcessingUnit and is never touched
        // here.
        void            TickAll();

    private:
        struct Entry
        {
            // std::optional<SimTimeDomain> rather than a bare SimTimeDomain:
            // SimTimeDomain is not default-constructible (id + hz are required),
            // but DynamicArrayC's default ctor value-initializes its fixed array.
            // optional defaults to empty and tombstones on Destroy() (which keeps
            // pointer stability and creation order for the surviving entries).
            // Same pattern as DiaStreams FrameStreamStore<T>::Slot.
            Core::StringCRC             id;
            Core::StringCRC             parentId;
            std::optional<SimTimeDomain> domain;
        };

        Entry*  FindEntry(Core::StringCRC id);

        // Walks the ancestor chain of parentId up to (and including) the world
        // domain, accumulating the product of local scales and OR-ing paused.
        void    ComputeAncestorChain(Core::StringCRC parentId, float& outScale, bool& outPaused) const;

        SimTimeDomain&  mWorldDomain;   // externally owned; never ticked here
        Dia::Core::Containers::DynamicArrayC<Entry, kMaxDomains> mEntries;
    };

} // namespace Dia::SimTime
