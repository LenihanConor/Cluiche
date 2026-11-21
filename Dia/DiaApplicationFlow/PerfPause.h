#pragma once
// Perf live control (task 3): Debug-only module/domain pause.
//
// Whole feature is compiled out unless _DEBUG is defined (DiaApplicationFlow's vcxproj has no
// DIA_DEBUG). Nothing here touches Module's layout: pausing is decided by the ProcessingUnit
// per forward pass from a cached per-slot table (see ProcessingUnit::RunForwardPass).
//
// Threading: PerfPauseController is Main-thread only (single writer). It publishes an immutable
// PerfPauseSet snapshot to a FrameStream ("$perfpause"); every ProcessingUnit reads the latest
// snapshot once per Update() and never touches the controller (PU boundaries are streams).
//
// Placement: the controller is a plain class owned by Application (framework-owned stream, like
// $lifecycle), not a Module, so no application manifest has to declare it.
#ifdef _DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/FrameStreamStore.h>
#include <cstdint>

namespace Dia { namespace ApplicationFlow {

    enum class PerfPauseKind : uint32_t { kModule = 0, kDomain = 1 };

    // Wire snapshot. Representation choice: modules are identified by instance-id CRC (not by a
    // bit index), because registration order is per-PU while the pause set is global. A PU
    // matches the CRCs against its own modules only when `generation` changes and caches the
    // result as bool-per-slot, so the hot path is one array read per module per pass.
    struct PerfPauseSet
    {
        static constexpr unsigned int kMaxModules = 32;
        static constexpr unsigned int kMaxDomains = 16;
        uint32_t generation = 0;
        uint32_t moduleCount = 0;
        uint32_t moduleCrcs[kMaxModules] = {};
        uint32_t domainCount = 0;
        uint32_t domainCrcs[kMaxDomains] = {};
    };

    struct PerfPauseResult
    {
        bool        ok     = false;
        const char* reason = nullptr;   // static string; null when ok
    };

    struct PerfPauseEntry
    {
        uint32_t      owner = 0;
        PerfPauseKind kind  = PerfPauseKind::kModule;
        uint32_t      crc   = 0;
    };

    class PerfPauseController
    {
    public:
        // Owner 0 is reserved for in-game / test callers; released on stage exit and shutdown.
        static constexpr uint32_t kLocalOwner = 0;
        static constexpr unsigned int kMaxEntries = 64;

        // Maps a module instance id to its type id; false if no such module exists.
        using ModuleResolveFn = bool (*)(void* ctx, const Dia::Core::StringCRC& instanceId, Dia::Core::StringCRC& outTypeId);

        // `store` is not owned; it must outlive the controller. Publishes an empty snapshot.
        explicit PerfPauseController(FrameStreamStore<PerfPauseSet>* store);

        void SetModuleResolver(ModuleResolveFn fn, void* ctx) { mResolve = fn; mResolveCtx = ctx; }

        // Pause is idempotent per (owner, kind, id). Several owners may pause the same id; it
        // stays paused until every owner has resumed it.
        PerfPauseResult Pause(uint32_t owner, PerfPauseKind kind, const Dia::Core::StringCRC& id);
        PerfPauseResult Resume(uint32_t owner, PerfPauseKind kind, const Dia::Core::StringCRC& id);
        void ResumeAll(uint32_t owner);
        void ReleaseOwner(uint32_t owner);   // same effect as ResumeAll; named for lifecycle callers

        [[nodiscard]] unsigned int ActivePauses(PerfPauseEntry* out, unsigned int maxOut) const;
        [[nodiscard]] bool IsPaused(PerfPauseKind kind, const Dia::Core::StringCRC& id) const;
        [[nodiscard]] uint32_t GetGeneration() const { return mGeneration; }
        [[nodiscard]] const FrameStreamStore<PerfPauseSet>* GetStore() const { return mStore; }

        // True if the type id is never pausable (Kernel, DebugServerHost, SimQueryHost).
        [[nodiscard]] static bool IsDenylistedType(const Dia::Core::StringCRC& typeId);

    private:
        [[nodiscard]] bool IdHeldByAnyOwner(PerfPauseKind kind, uint32_t crc) const;
        void Remove(unsigned int index);
        void Publish();

        FrameStreamStore<PerfPauseSet>* mStore;
        ModuleResolveFn mResolve    = nullptr;
        void*           mResolveCtx = nullptr;
        PerfPauseEntry  mEntries[kMaxEntries];
        unsigned int    mEntryCount = 0;
        uint32_t        mGeneration = 0;
    };

}} // namespace Dia::ApplicationFlow

#endif // _DEBUG
