#include <DiaCore/SimTime/SimTimeDomainRegistry.h>

#include <DiaCore/Core/Assert.h>

namespace Dia::SimTime {

    // "world" / "" hashed once at static-init (CRC::Calc is a runtime lookup).
    const Core::StringCRC SimTimeDomainRegistry::kWorldId("world");
    const Core::StringCRC SimTimeDomainRegistry::kNoParent("");

    SimTimeDomainRegistry::SimTimeDomainRegistry(SimTimeDomain& worldDomain)
        : mWorldDomain(worldDomain)
    {
    }

    SimTimeDomain& SimTimeDomainRegistry::Create(Core::StringCRC id, Core::StringCRC parentId)
    {
        DIA_ASSERT(id != kWorldId, "SimTimeDomainRegistry::Create — the world domain is externally owned and cannot be created here");
        DIA_ASSERT(FindEntry(id) == nullptr, "SimTimeDomainRegistry::Create — a domain with this id already exists");
        DIA_ASSERT(!mEntries.IsFull(), "SimTimeDomainRegistry::Create — registry is at capacity (%d domains)", kMaxDomains);

        // Every domain ticks at the same granularity as the world domain, so
        // TickAll() (called once per world tick) keeps the tree in lockstep.
        const float hz = 1.0f / mWorldDomain.Step().AsFloatInSeconds();

        mEntries.AddDefault();
        Entry& entry = mEntries.Back();
        entry.id       = id;
        entry.parentId = parentId;
        entry.domain.emplace(id, hz, mWorldDomain.Now());

        return *entry.domain;
    }

    SimTimeDomainRegistry::Entry* SimTimeDomainRegistry::FindEntry(Core::StringCRC id)
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            Entry& e = mEntries.At(i);
            if (e.domain.has_value() && e.id == id)
            {
                return &e;
            }
        }
        return nullptr;
    }

    SimTimeDomain* SimTimeDomainRegistry::Find(Core::StringCRC id)
    {
        if (id == kWorldId)
        {
            return &mWorldDomain;
        }

        Entry* e = FindEntry(id);
        return e ? &(*e->domain) : nullptr;
    }

    void SimTimeDomainRegistry::Destroy(Core::StringCRC id)
    {
        DIA_ASSERT(id != kWorldId, "SimTimeDomainRegistry::Destroy — the world domain is externally owned and cannot be destroyed here");
        if (id == kWorldId)
        {
            return;
        }

        Entry* e = FindEntry(id);
        if (e)
        {
            // Tombstone: reset the optional in place rather than shifting the
            // array. Preserves pointer stability and creation order for the
            // surviving entries.
            e->domain.reset();
        }
    }

    void SimTimeDomainRegistry::ComputeAncestorChain(Core::StringCRC parentId, float& outScale, bool& outPaused) const
    {
        outScale  = 1.0f;
        outPaused = false;

        Core::StringCRC cur = parentId;
        while (true)
        {
            // kNoParent / kWorldId both terminate at the externally-owned world
            // root, whose scale and pause always participate in composition.
            if (cur == kNoParent || cur == kWorldId)
            {
                outScale *= mWorldDomain.GetScale();
                if (mWorldDomain.IsPaused())
                {
                    outPaused = true;
                }
                break;
            }

            // const walk over a mutable container: resolve without FindEntry's
            // non-const return.
            const Entry* found = nullptr;
            for (unsigned int i = 0; i < mEntries.Size(); ++i)
            {
                const Entry& e = mEntries.At(i);
                if (e.domain.has_value() && e.id == cur)
                {
                    found = &e;
                    break;
                }
            }

            if (found == nullptr)
            {
                // Dangling parent (e.g. an ancestor was Destroy()'d): stop the
                // walk here.
                break;
            }

            outScale *= found->domain->GetScale();
            if (found->domain->IsPaused())
            {
                outPaused = true;
            }
            cur = found->parentId;
        }
    }

    void SimTimeDomainRegistry::TickAll()
    {
        // Advance every non-world domain once, in creation order. kWorldId is
        // externally clocked by the owning ProcessingUnit and is never stored in
        // mEntries, so it is inherently a no-op here.
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            Entry& entry = mEntries.At(i);
            if (!entry.domain.has_value())
            {
                continue;   // tombstoned (Destroy'd)
            }

            SimTimeDomain& domain = *entry.domain;

            float ancestorScale = 1.0f;
            bool  ancestorPaused = false;
            ComputeAncestorChain(entry.parentId, ancestorScale, ancestorPaused);

            if (ancestorPaused)
            {
                // An ancestor is paused: freeze this child even though its own
                // pause flag may be clear.
                continue;
            }

            // GetScale() reports the caller's local intent (restored at the end
            // of each pass). Compose with the ancestor product, apply, tick,
            // then restore local so future reads / composition stay correct.
            const float localScale = domain.GetScale();
            domain.SetScale(ancestorScale * localScale);
            domain.Tick();                  // no-op internally if locally paused
            domain.SetScale(localScale);
        }
    }

} // namespace Dia::SimTime
