// S-1 Regions, Nodes and Site Hook: plug-in registry for site placers.
// Non-owning: registered placers / guarantees must outlive the registry (or be Unregistered).
// Params note: PlacementContext omits GenParams (Feature 3 not built); add it with that feature.
#pragma once

#include "PlaceResult.h"
#include <DraoiHexWorld/Sites.h>
#include "TerrainTypes.h"
#include "ValidationTypes.h"

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Core/RandomStream.h>

#include <span>
#include <vector>

namespace CoW {
namespace WorldGeneration {

// Read-only view of everything placed so far.
struct PlacementContext
{
    const WorldStructure& world;   // terrain, biomes, regions, built-in nodes, sites of earlier kinds
    GridDimensions        dims;
    int                   attempt = 0;
};

class ISitePlacer
{
public:
    virtual ~ISitePlacer() = default;
    virtual const SiteKindInfo& Info() const = 0;
    // Kinds that must be placed before this one.
    virtual std::span<const Dia::Core::StringCRC> DependsOn() const = 0;
    // Adds sites of this kind to 'out'. 'rng' is reseeded for this kind and this attempt.
    virtual PlaceResult Place(const PlacementContext& ctx, Dia::Maths::RandomStream& rng, SiteSet& out) = 0;
};

class SiteRegistry
{
public:
    // False (registry unchanged): null, duplicate kind id, or a dependency cycle (incl. self).
    // A dependency on a kind not (yet) registered is allowed.
    // A data-declared kind with the same id is replaced by the placer's Info() (the placer attaches to it).
    bool Register(ISitePlacer* placer);
    // Declares a kind with display info only (no placer): returned by FindInfo and ListSiteKinds. False (unchanged):
    // empty id, id already declared, id registered by a placer, or a built-in node kind.
    bool RegisterDeclaredKind(const SiteKindInfo& info);
    // False: null, same pointer already registered, or a guarantee with the same GetId().
    bool RegisterGuarantee(IWorldGuarantee* guarantee);
    // False if the kind is not registered.
    bool Unregister(Dia::Core::StringCRC kind);

    // Dependencies first (pulled forward to the first dependant's turn), then registration order.
    // Unmet dependencies (kind not registered) are ignored. Valid until the next Register/Unregister.
    std::span<ISitePlacer* const> Order() const;
    std::span<IWorldGuarantee* const> Guarantees() const { return { mGuarantees.data(), mGuarantees.size() }; }

    // nullptr if not registered (placer kinds first, then declared kinds); built-in node kinds are not registered and return nullptr.
    const SiteKindInfo* FindInfo(Dia::Core::StringCRC kind) const;

    // Data-declared kinds that have no placer, in declaration order.
    std::span<const SiteKindInfo> Declared() const { return { mDeclared.data(), mDeclared.size() }; }

    int Count() const { return static_cast<int>(mPlacers.size()); }

private:
    int  IndexOf(uint32_t kindCrc) const;
    bool HasCycle() const;

    std::vector<ISitePlacer*>         mPlacers;      // registration order
    std::vector<SiteKindInfo>         mDeclared;     // data-declared kinds without a placer, declaration order
    std::vector<IWorldGuarantee*>     mGuarantees;
    mutable std::vector<ISitePlacer*> mOrder;
    mutable bool                      mOrderValid = false;
};

// Every kind the world can hold, for listings and the Sites overlay: the four built-in node kinds (Corridor, River,
// Hunting, Resource, with BuiltinNodeKindInfo display info), then the data-declared kinds (declaration order, declared first), then
// the placer-registered kinds in Order().
std::vector<SiteKindInfo> ListSiteKinds(const SiteRegistry& registry);

} // namespace WorldGeneration
} // namespace CoW
