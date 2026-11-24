#ifdef _DEBUG

#include <DiaApplicationFlow/PerfPause.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Perf/PerfMarkers.h>

namespace Dia { namespace ApplicationFlow {

    using Dia::Core::StringCRC;

    PerfPauseController::PerfPauseController(FrameStreamStore<PerfPauseSet>* store)
        : mStore(store)
    {
        Publish();   // empty snapshot so PUs never see a no-data stream
    }

    bool PerfPauseController::IsDenylistedType(const StringCRC& typeId)
    {
        // Type ids of app-layer modules that must keep running (window/kernel, debug server
        // host, sim query host). Names only: the Dia layer must not include app headers.
        static const StringCRC kKernel("KernelModule");
        static const StringCRC kDebugServerHost("DebugServerHostModule");
        static const StringCRC kSimQueryHost("SimQueryHostModule");
        return typeId == kKernel || typeId == kDebugServerHost || typeId == kSimQueryHost;
    }

    bool PerfPauseController::IdHeldByAnyOwner(PerfPauseKind kind, uint32_t crc) const
    {
        for (unsigned int i = 0; i < mEntryCount; ++i)
            if (mEntries[i].kind == kind && mEntries[i].crc == crc)
                return true;
        return false;
    }

    bool PerfPauseController::IsPaused(PerfPauseKind kind, const StringCRC& id) const
    {
        return IdHeldByAnyOwner(kind, id.Value());
    }

    PerfPauseResult PerfPauseController::Pause(uint32_t owner, PerfPauseKind kind, const StringCRC& id)
    {
        const uint32_t crc = id.Value();
        if (kind == PerfPauseKind::kModule)
        {
            StringCRC typeId;
            if (!mResolve || !mResolve(mResolveCtx, id, typeId))
                return { false, "unknown module" };
            if (IsDenylistedType(typeId))
                return { false, "module is on the pause denylist" };
        }

        for (unsigned int i = 0; i < mEntryCount; ++i)
            if (mEntries[i].owner == owner && mEntries[i].kind == kind && mEntries[i].crc == crc)
                return { true, nullptr };   // already held by this owner

        const bool newId = !IdHeldByAnyOwner(kind, crc);
        if (mEntryCount >= kMaxEntries)
            return { false, "pause table full" };
        if (newId)
        {
            unsigned int uniq = 0;
            for (unsigned int i = 0; i < mEntryCount; ++i)
            {
                if (mEntries[i].kind != kind) continue;
                bool seen = false;
                for (unsigned int j = 0; j < i; ++j)
                    if (mEntries[j].kind == kind && mEntries[j].crc == mEntries[i].crc) { seen = true; break; }
                if (!seen) ++uniq;
            }
            const unsigned int cap = (kind == PerfPauseKind::kModule) ? PerfPauseSet::kMaxModules : PerfPauseSet::kMaxDomains;
            if (uniq >= cap)
                return { false, "pause set full" };
        }

        mEntries[mEntryCount++] = { owner, kind, crc };
        if (newId)
        {
            DIA_PERF_MARK_VALUE("perf.pause", crc);
            Publish();
        }
        return { true, nullptr };
    }

    void PerfPauseController::Remove(unsigned int index)
    {
        const PerfPauseEntry removed = mEntries[index];
        mEntries[index] = mEntries[--mEntryCount];   // order is irrelevant
        if (!IdHeldByAnyOwner(removed.kind, removed.crc))
        {
            DIA_PERF_MARK_VALUE("perf.resume", removed.crc);
            Publish();
        }
    }

    PerfPauseResult PerfPauseController::Resume(uint32_t owner, PerfPauseKind kind, const StringCRC& id)
    {
        const uint32_t crc = id.Value();
        for (unsigned int i = 0; i < mEntryCount; ++i)
        {
            if (mEntries[i].owner == owner && mEntries[i].kind == kind && mEntries[i].crc == crc)
            {
                Remove(i);
                return { true, nullptr };
            }
        }
        return { false, "not paused by this owner" };
    }

    void PerfPauseController::ResumeAll(uint32_t owner)
    {
        for (unsigned int i = 0; i < mEntryCount; )
        {
            if (mEntries[i].owner == owner) Remove(i);   // swaps a new entry into i
            else ++i;
        }
    }

    void PerfPauseController::ReleaseOwner(uint32_t owner) { ResumeAll(owner); }

    unsigned int PerfPauseController::ActivePauses(PerfPauseEntry* out, unsigned int maxOut) const
    {
        unsigned int n = 0;
        for (unsigned int i = 0; i < mEntryCount && n < maxOut; ++i)
            out[n++] = mEntries[i];
        return n;
    }

    void PerfPauseController::Publish()
    {
        PerfPauseSet s;
        s.generation = ++mGeneration;
        for (unsigned int i = 0; i < mEntryCount; ++i)
        {
            const PerfPauseEntry& e = mEntries[i];
            uint32_t* list  = (e.kind == PerfPauseKind::kModule) ? s.moduleCrcs : s.domainCrcs;
            uint32_t& count = (e.kind == PerfPauseKind::kModule) ? s.moduleCount : s.domainCount;
            bool dup = false;
            for (uint32_t k = 0; k < count; ++k) if (list[k] == e.crc) { dup = true; break; }
            if (!dup) list[count++] = e.crc;   // capacity enforced in Pause()
        }
        if (mStore)
            mStore->Write(s, Dia::Core::TimeAbsolute::GetSystemTime());
    }

}} // namespace Dia::ApplicationFlow

#endif // _DEBUG
