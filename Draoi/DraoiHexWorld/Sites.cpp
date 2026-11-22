#include <DraoiHexWorld/Sites.h>

#include <algorithm>
#include <cstring>

namespace Draoi {
namespace HexWorld {

namespace
{
void CopyText(char* dst, size_t cap, const char* src)
{
    dst[0] = '\0';
    if (src)
    {
        std::strncpy(dst, src, cap - 1);
        dst[cap - 1] = '\0';
    }
}

bool SiteLess(const Site& s, uint32_t kindCrc, uint32_t cell)
{
    return s.kindCrc < kindCrc || (s.kindCrc == kindCrc && s.cell < cell);
}
}

SiteKindInfo MakeSiteKindInfo(const Dia::Core::StringCRC& kind, const char* displayName, uint32_t debugColour,
                              SiteShape shape, const char* variantLabel)
{
    SiteKindInfo i;
    i.kindCrc = SiteKindId(kind);
    CopyText(i.id, sizeof(i.id), kind.AsChar());
    CopyText(i.displayName, sizeof(i.displayName), displayName);
    i.debugColour = debugColour & 0xFFFFFFu;
    i.debugShape = shape;
    CopyText(i.variantLabel, sizeof(i.variantLabel), variantLabel);
    return i;
}

const char* SiteShapeName(SiteShape shape)
{
    switch (shape)
    {
    case SiteShape::Dot:      return "Dot";
    case SiteShape::Diamond:  return "Diamond";
    case SiteShape::Square:   return "Square";
    case SiteShape::Triangle: return "Triangle";
    case SiteShape::Star:     return "Star";
    case SiteShape::Ring:     return "Ring";
    }
    return "Dot";
}

SiteKindInfo BuiltinNodeKindInfo(const NodeKindIds& ids, const Dia::Core::StringCRC& kind)
{
    if (kind == ids.corridor) { return MakeSiteKindInfo(kind, "Corridor Node",    0xE8A33D, SiteShape::Diamond,  "Corridor"); }
    if (kind == ids.river)    { return MakeSiteKindInfo(kind, "River Node",       0x3D9BE8, SiteShape::Triangle, "");        }
    if (kind == ids.hunting)  { return MakeSiteKindInfo(kind, "Hunting Ground",   0xB5483A, SiteShape::Square,   "");        }
    if (kind == ids.resource) { return MakeSiteKindInfo(kind, "Resource Deposit", 0x9B59D0, SiteShape::Star,     "");        }
    return MakeSiteKindInfo(kind, "", 0xFFFFFF, SiteShape::Dot);
}

bool SiteSet::Add(const Site& site)
{
    auto it = std::lower_bound(mSites.begin(), mSites.end(), site,
        [](const Site& a, const Site& b) { return SiteLess(a, b.kindCrc, b.cell); });
    if (it != mSites.end() && it->kindCrc == site.kindCrc && it->cell == site.cell)
    {
        return false;
    }
    mSites.insert(it, site);
    return true;
}

std::span<const Site> SiteSet::Of(uint32_t kindCrc) const
{
    auto first = std::lower_bound(mSites.begin(), mSites.end(), kindCrc,
        [](const Site& s, uint32_t k) { return s.kindCrc < k; });
    auto last = std::upper_bound(first, mSites.end(), kindCrc,
        [](uint32_t k, const Site& s) { return k < s.kindCrc; });
    if (first == last) { return {}; }
    return { &*first, static_cast<size_t>(last - first) };
}

bool SiteSet::Has(uint32_t cell) const
{
    return std::any_of(mSites.begin(), mSites.end(), [cell](const Site& s) { return s.cell == cell; });
}

SitesAtCell SiteSet::At(uint32_t cell) const
{
    SitesAtCell out;
    for (const Site& s : mSites)
    {
        if (s.cell == cell && !out.IsFull()) { out.Add(s); }
    }
    return out;
}

bool SiteSet::RememberKindName(uint32_t kindCrc, const std::string& name)
{
    if (name.empty() || Dia::Core::StringCRC(name.c_str()).Value() != kindCrc) { return false; }
    for (auto& entry : mKindNames)
    {
        if (entry.first == kindCrc) { entry.second = name; return true; }
    }
    mKindNames.emplace_back(kindCrc, name);
    return true;
}

const char* SiteSet::KindName(uint32_t kindCrc) const
{
    for (const auto& entry : mKindNames)
    {
        if (entry.first == kindCrc) { return entry.second.c_str(); }
    }
    return nullptr;
}

const Site* SiteSet::Find(const Dia::Core::StringCRC& kind, uint32_t cell) const
{
    const uint32_t k = SiteKindId(kind);
    auto it = std::lower_bound(mSites.begin(), mSites.end(), k,
        [cell](const Site& s, uint32_t kk) { return SiteLess(s, kk, cell); });
    return (it != mSites.end() && it->kindCrc == k && it->cell == cell) ? &*it : nullptr;
}

} // namespace HexWorld
} // namespace Draoi
