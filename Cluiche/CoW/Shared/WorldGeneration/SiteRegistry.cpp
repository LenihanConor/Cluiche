#include "SiteRegistry.h"

#include <cstring>

namespace CoW {
namespace WorldGeneration {

namespace
{
    uint32_t KindOf(const ISitePlacer* p) { return p->Info().kindCrc; }
}

int SiteRegistry::IndexOf(uint32_t kindCrc) const
{
    for (size_t i = 0; i < mPlacers.size(); ++i)
        if (KindOf(mPlacers[i]) == kindCrc) return static_cast<int>(i);
    return -1;
}

bool SiteRegistry::HasCycle() const
{
    // 0 = unvisited, 1 = in progress, 2 = done. Only registered kinds are considered.
    std::vector<uint8_t> state(mPlacers.size(), 0);
    struct Visitor
    {
        const SiteRegistry& r; std::vector<uint8_t>& st;
        bool Visit(int i)
        {
            if (st[i] == 2) return false;
            if (st[i] == 1) return true;
            st[i] = 1;
            for (const Dia::Core::StringCRC& d : r.mPlacers[i]->DependsOn())
            {
                const int j = r.IndexOf(d.Value());
                if (j >= 0 && Visit(j)) return true;
            }
            st[i] = 2;
            return false;
        }
    } v{ *this, state };
    for (size_t i = 0; i < mPlacers.size(); ++i)
        if (v.Visit(static_cast<int>(i))) return true;
    return false;
}

bool SiteRegistry::Register(ISitePlacer* placer)
{
    if (!placer) return false;
    if (IndexOf(KindOf(placer)) >= 0) return false;
    mPlacers.push_back(placer);
    if (HasCycle())
    {
        mPlacers.pop_back();
        return false;
    }
    mOrderValid = false;
    return true;
}

bool SiteRegistry::RegisterGuarantee(ILayoutGuarantee* guarantee)
{
    if (!guarantee) return false;
    const char* name = guarantee->Name();
    for (const ILayoutGuarantee* g : mGuarantees)
    {
        if (g == guarantee) return false;
        const char* other = g->Name();
        if (name && other && std::strcmp(name, other) == 0) return false;
    }
    mGuarantees.push_back(guarantee);
    return true;
}

bool SiteRegistry::Unregister(Dia::Core::StringCRC kind)
{
    const int i = IndexOf(kind.Value());
    if (i < 0) return false;
    mPlacers.erase(mPlacers.begin() + i);
    mOrderValid = false;
    return true;
}

std::span<ISitePlacer* const> SiteRegistry::Order() const
{
    if (!mOrderValid)
    {
        mOrder.clear();
        std::vector<uint8_t> done(mPlacers.size(), 0);
        struct Emitter
        {
            const SiteRegistry& r; std::vector<uint8_t>& done;
            void Emit(int i)
            {
                if (done[i]) return;
                done[i] = 1; // registry is cycle-free by construction; mark first as a guard
                for (const Dia::Core::StringCRC& d : r.mPlacers[i]->DependsOn())
                {
                    const int j = r.IndexOf(d.Value());
                    if (j >= 0) Emit(j);
                }
                r.mOrder.push_back(r.mPlacers[i]);
            }
        } e{ *this, done };
        for (size_t i = 0; i < mPlacers.size(); ++i) e.Emit(static_cast<int>(i));
        mOrderValid = true;
    }
    return { mOrder.data(), mOrder.size() };
}

const SiteKindInfo* SiteRegistry::FindInfo(Dia::Core::StringCRC kind) const
{
    const int i = IndexOf(kind.Value());
    return i < 0 ? nullptr : &mPlacers[i]->Info();
}

std::vector<SiteKindInfo> ListSiteKinds(const SiteRegistry& registry)
{
    std::vector<SiteKindInfo> out;
    for (const Dia::Core::StringCRC& k : { BuiltinNodeKinds::Corridor(), BuiltinNodeKinds::River(), BuiltinNodeKinds::Hunting(),
                                           BuiltinNodeKinds::Resource() })
    {
        out.push_back(BuiltinNodeKindInfo(k));
    }
    for (ISitePlacer* p : registry.Order())
    {
        out.push_back(p->Info());
    }
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
