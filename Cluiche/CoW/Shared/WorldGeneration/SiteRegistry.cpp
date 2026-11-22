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
    // A declared kind with this id is superseded by the placer's info.
    for (size_t i = 0; i < mDeclared.size(); ++i)
    {
        if (mDeclared[i].kindCrc == KindOf(placer))
        {
            mDeclared.erase(mDeclared.begin() + i);
            break;
        }
    }
    mOrderValid = false;
    return true;
}

bool SiteRegistry::RegisterDeclaredKind(const SiteKindInfo& info)
{
    if (info.id[0] == '\0') return false;
    if (IndexOf(info.kindCrc) >= 0) return false;
    for (const SiteKindInfo& d : mDeclared)
        if (d.kindCrc == info.kindCrc) return false;
    if (mNodeKinds.IsBuiltin(info.kindCrc)) return false;
    mDeclared.push_back(info);
    return true;
}

bool SiteRegistry::RegisterGuarantee(IWorldGuarantee* guarantee)
{
    if (!guarantee) return false;
    const uint32_t id = guarantee->GetId().Value();
    for (const IWorldGuarantee* g : mGuarantees)
    {
        if (g == guarantee) return false;
        if (g->GetId().Value() == id) return false;
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
    if (i >= 0) return &mPlacers[i]->Info();
    for (const SiteKindInfo& d : mDeclared)
        if (d.kindCrc == kind.Value()) return &d;
    return nullptr;
}

std::vector<SiteKindInfo> ListSiteKinds(const SiteRegistry& registry)
{
    std::vector<SiteKindInfo> out;
    const NodeKindIds& ids = registry.NodeKinds();
    for (const Dia::Core::StringCRC& k : { ids.corridor, ids.river, ids.hunting, ids.resource })
    {
        out.push_back(BuiltinNodeKindInfo(ids, k));
    }
    for (const SiteKindInfo& d : registry.Declared())
    {
        out.push_back(d);
    }
    for (ISitePlacer* p : registry.Order())
    {
        out.push_back(p->Info());
    }
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
