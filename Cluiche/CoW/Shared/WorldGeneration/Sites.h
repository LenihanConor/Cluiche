// S-1 Regions, Nodes and Site Hook: Site records, kind info and SiteSet.
// Kind ids are stored as the StringCRC value (uint32_t) so Site / SiteKindInfo stay trivially
// copyable (StringCRC holds a 64-byte string and has user-provided copies). StringCRC is used at
// the API edges only. Sites are ordered by (kindCrc, cell): CRC order, not name order.
#pragma once

#include "TerrainEnums.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>
#include <span>
#include <vector>

namespace CoW {
namespace WorldGeneration {

// StringCRC -> storable kind id.
inline uint32_t SiteKindId(const Dia::Core::StringCRC& kind) { return kind.Value(); }

struct Site
{
    uint32_t kindCrc = 0;
    uint32_t cell    = 0;
    RegionId region  = kNoRegion;
    uint16_t variant = 0;
    uint16_t flags   = 0;

    bool operator==(const Site&) const = default;
};

enum class SiteShape : uint8_t { Dot, Diamond, Square, Triangle, Star, Ring };

struct SiteKindInfo
{
    uint32_t  kindCrc         = 0;
    char      id[64]          = {};      // the kind id string (hashes to kindCrc)
    char      displayName[32] = {};
    uint32_t  debugColour     = 0xFFFFFF; // 0xRRGGBB
    SiteShape debugShape      = SiteShape::Dot;
    char      variantLabel[24] = {};

    Dia::Core::StringCRC Kind() const { return Dia::Core::StringCRC(id); }
};

// Fills a SiteKindInfo (strings truncated to fit).
SiteKindInfo MakeSiteKindInfo(const Dia::Core::StringCRC& kind, const char* displayName, uint32_t debugColour,
                              SiteShape shape, const char* variantLabel = "");

namespace BuiltinNodeKinds
{
    Dia::Core::StringCRC Corridor();   // "CoW.Node.Corridor"
    Dia::Core::StringCRC River();      // "CoW.Node.River"
    Dia::Core::StringCRC Hunting();    // "CoW.Node.Hunting"
    Dia::Core::StringCRC Resource();   // "CoW.Node.Resource"
}

// Display info for the four built-in kinds; for any other kind returns an info with the
// right kindCrc/id and an empty displayName.
SiteKindInfo BuiltinNodeKindInfo(const Dia::Core::StringCRC& kind);

// Sites at one cell (a cell can hold several kinds).
using SitesAtCell = Dia::Core::Containers::DynamicArrayC<Site, 8>;

// Flat storage sorted by (kindCrc, cell). At most one site per (kind, cell).
// Heap-backed (site counts are unbounded for registered kinds); the public API returns spans.
class SiteSet
{
public:
    // False (set unchanged) if a site of that kind already exists at the cell.
    bool Add(const Site& site);

    std::span<const Site> All() const { return { mSites.data(), mSites.size() }; }
    std::span<const Site> Of(const Dia::Core::StringCRC& kind) const { return Of(SiteKindId(kind)); }
    std::span<const Site> Of(uint32_t kindCrc) const;

    int  Count() const { return static_cast<int>(mSites.size()); }
    int  Count(const Dia::Core::StringCRC& kind) const { return static_cast<int>(Of(kind).size()); }

    bool Has(uint32_t cell) const;                               // any kind
    SitesAtCell At(uint32_t cell) const;                         // truncated at 8 kinds per cell
    const Site* Find(const Dia::Core::StringCRC& kind, uint32_t cell) const;

    void Clear() { mSites.clear(); }

    bool operator==(const SiteSet& o) const { return mSites == o.mSites; }

private:
    std::vector<Site> mSites;
};

} // namespace WorldGeneration
} // namespace CoW
