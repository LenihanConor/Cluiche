// S-1 Regions, Nodes and Site Hook: plug-in registry for site placers.
// Non-owning: registered placers / guarantees must outlive the registry (or be Unregistered).
// Params note: PlacementContext omits GenParams (Feature 2/3 not built); add it with those features.
#pragma once

#include "PlaceResult.h"
#include "Sites.h"
#include "TerrainTypes.h"

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

// Placeholder until Feature 3's IWorldGuarantee exists (plan task 13 swaps it).
class ILayoutGuarantee
{
public:
    virtual ~ILayoutGuarantee() = default;
    virtual const char* Name() const = 0;
};

class SiteRegistry
{
public:
    // False (registry unchanged): null, duplicate kind id, or a dependency cycle (incl. self).
    // A dependency on a kind not (yet) registered is allowed.
    bool Register(ISitePlacer* placer);
    // False: null, same pointer already registered, or a guarantee with the same Name().
    bool RegisterGuarantee(ILayoutGuarantee* guarantee);
    // False if the kind is not registered.
    bool Unregister(Dia::Core::StringCRC kind);

    // Dependencies first (pulled forward to the first dependant's turn), then registration order.
    // Unmet dependencies (kind not registered) are ignored. Valid until the next Register/Unregister.
    std::span<ISitePlacer* const> Order() const;
    std::span<ILayoutGuarantee* const> Guarantees() const { return { mGuarantees.data(), mGuarantees.size() }; }

    // nullptr if not registered; built-in node kinds are not registered and return nullptr.
    const SiteKindInfo* FindInfo(Dia::Core::StringCRC kind) const;

    int Count() const { return static_cast<int>(mPlacers.size()); }

private:
    int  IndexOf(uint32_t kindCrc) const;
    bool HasCycle() const;

    std::vector<ISitePlacer*>         mPlacers;      // registration order
    std::vector<ILayoutGuarantee*>    mGuarantees;
    mutable std::vector<ISitePlacer*> mOrder;
    mutable bool                      mOrderValid = false;
};

} // namespace WorldGeneration
} // namespace CoW
