// Terrain field contribution: registers the committed terrain layers of a generated world into a WorldCellFields.
// Spec: docs/specs/features/draoi/worldsim-kit-draoi.md (AC 10a). Plain function, no bus/event/module knowledge:
// who calls Clear and re-registers after a regenerate is the application's.
//
// Slot "terrain". One float field per generator layer, named after the layer in snake_case (elevation, flow_direction
// (hex direction 0..5, >= 6 none), water_source, river_mask, stratum, terrain_type, ...; the biomeIndex layer is
// biome.index and is not repeated), plus terrain.region (Cell::region), terrain.is_river (river cell) and
// terrain.is_protected. Bool fields supply both getBool and getFloat (1.0/0.0).
// terrain.is_protected is true on cells with the Pass or RiverCrossesBarrier TerrainFlag bit, and on cells holding a
// site of the session registry's corridor node kind or of a caller-added protected kind. The protected kinds are
// data (site kinds named in guarantees.json), so the caller adds them; this library names no kind.
// Lifetime: the caller keeps the TerrainFieldSource and the WorldGenSession alive and at a fixed address until
// WorldCellFields::Clear (the registered readers point into the source). Readers see the session's current committed world.
#pragma once

#include <DraoiHexWorld/WorldCellFields.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {

class WorldGenSession;
struct GuaranteeSpec;

class TerrainFieldSource
{
public:
    static constexpr uint32_t kMaxLayers         = 64;
    static constexpr uint32_t kMaxProtectedKinds = 16;

    explicit TerrainFieldSource(const WorldGenSession& session) : mSession(&session) {}
    TerrainFieldSource(const TerrainFieldSource&) = delete;
    TerrainFieldSource& operator=(const TerrainFieldSource&) = delete;

    // Adds a data-declared protected site kind (SiteKindId of its id). False when full; duplicates are ignored (true).
    bool AddProtectedKind(uint32_t kindCrc);

    // Adds every site kind of the spec flagged "protected": true. False when full (kinds added so far stay).
    bool AddProtectedKindsFrom(const GuaranteeSpec& spec);

    struct LayerBinding
    {
        const TerrainFieldSource* owner = nullptr;
        int                       layer = -1;
    };

    const WorldGenSession& Session() const { return *mSession; }
    bool                   IsProtectedKind(uint32_t kindCrc) const;
    LayerBinding&          Binding(uint32_t i) { return mLayers[i]; }

private:
    const WorldGenSession* mSession;
    uint32_t               mProtected[kMaxProtectedKinds] = {};
    uint32_t               mProtectedCount = 0;
    LayerBinding           mLayers[kMaxLayers];
};

void RegisterTerrainFields(WorldCellFields& fields, TerrainFieldSource& source);

} // namespace HexWorld
} // namespace Draoi
