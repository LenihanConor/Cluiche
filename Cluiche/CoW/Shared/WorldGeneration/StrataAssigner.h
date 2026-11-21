// Strata assigner seam: how the cells of a stratum set get their stratum.
//
// A stratum set (World::StratumSetSpec, from strata.json) names an assigner by type. The registry maps that type
// name to an IStrataAssigner; the terrain generator's Strata stage looks the depth set's assigner up and runs it.
// `coast_cost_bands` (TerrainStrata.h: coast-cost banding over N strata) is built in and registered by default;
// a game registers its own with Register. Non-owning: a registered assigner must outlive the registry.
//
// Contract every assigner must meet (RunStrataAssigner enforces it after Assign and rejects a violation):
//   - out holds one StratumIndex per cell (width * height), every value in 0..N where N = set.count.
//   - 0 only for ocean (ctx.landmass[cell] == 0); a land cell is never 0, always 1..N.
//   - deterministic from the world and the set's parameters: the same context gives the same output, no hidden
//     state, no randomness it does not seed itself.
//   - no dependence on sites: sites are placed after strata, so none exist when Assign runs.
//
// What the context holds: the Strata stage runs mid-pipeline, so `world` is partial. Only dimensions, mainLandmass
// and per cell landmass (saturating at 255), elevation (the raw pre-valley height), waterSource (River, Lake) and
// the Ridge / Barrier terrain flags are filled; every other Cell field is default. The raw layer arrays are the
// full-precision inputs (landmass ids, coast distance, ridge and barrier masks, river mask) and the two zone-cost
// layers are already computed (Dijkstra over coast distance with ridge, barrier and river corridor costs), so an
// assigner may band on them or ignore them. Assigners must not write the context.
//
// Spec: docs/specs/features/draoi/generic-strata.md (task 2, AC 6)
#pragma once

#include <Shared/World/StratumTable.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainStrata.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct StrataAssignContext
{
    const WorldStructure& world;   // partial, see above
    const TerrainRules&   rules;
    GridDimensions        dims;
    float                 barrier = 0.0f;       // the Barrier slider
    int                   mainLandmass = 0;     // landmass id of the main continent, 0 = none
    const uint32_t*       landmass = nullptr;   // per cell, 0 = ocean
    const uint32_t*       coastDistance = nullptr;
    const float*          ridgeMask = nullptr;
    const float*          barrierMask = nullptr;
    const uint32_t*       riverMask = nullptr;
    const float*          zoneCostBase = nullptr;   // computed, no rivers
    const float*          zoneCost = nullptr;       // computed, rivers as corridors
    Stage8Debug*          debug = nullptr;          // optional: the built-in assigner reports its cuts here
};

class IStrataAssigner
{
public:
    virtual ~IStrataAssigner() = default;
    // Fills out (one entry per cell) for the set. See the contract above.
    virtual void Assign(const StrataAssignContext& ctx, const World::StratumSetSpec& set, std::span<StratumIndex> out) = 0;
};

// The built-in assigner: coast-cost banding over the set's N strata (TerrainStrata.h).
class CoastCostBandsAssigner final : public IStrataAssigner
{
public:
    void Assign(const StrataAssignContext& ctx, const World::StratumSetSpec& set, std::span<StratumIndex> out) override;
};

class StrataAssignerRegistry
{
public:
    static constexpr size_t kMaxTypeNameLength = World::StratumTable::kMaxNameLength;

    StrataAssignerRegistry();
    StrataAssignerRegistry(const StrataAssignerRegistry&) = delete;
    StrataAssignerRegistry& operator=(const StrataAssignerRegistry&) = delete;

    // False (registry unchanged): null assigner, empty or over-long name, or a name already registered (the built-in
    // name included).
    bool Register(const char* typeName, IStrataAssigner* assigner);
    // nullptr when the name is not registered.
    IStrataAssigner* Find(const char* typeName) const;
    bool Has(const char* typeName) const { return Find(typeName) != nullptr; }
    int  Count() const { return static_cast<int>(mEntries.size()); }
    // A check for StratumTable::LoadFromText that accepts exactly the registered type names. Valid while this registry lives.
    World::AssignerTypeCheck TypeCheck() const;

private:
    struct Entry
    {
        std::string      name;
        IStrataAssigner* assigner;
    };
    CoastCostBandsAssigner mBuiltin;
    std::vector<Entry>     mEntries;
};

// Why an assigner's output was rejected.
struct StrataAssignError
{
    std::string assigner;   // the registered type name
    int         cell = -1;  // the first offending cell
    uint32_t    value = 0;  // the value found there
    std::string reason;     // one line naming the assigner, the cell and the violation
};

// Runs the assigner and enforces the contract. out is cleared to 0 first. False on a violation (err filled and an
// error logged); out then holds the rejected output.
bool RunStrataAssigner(const char* typeName, IStrataAssigner& assigner, const StrataAssignContext& ctx,
                       const World::StratumSetSpec& set, std::span<StratumIndex> out, StrataAssignError& err);

} // namespace WorldGeneration
} // namespace CoW
