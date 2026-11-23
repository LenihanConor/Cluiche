// Named cell selectors (DiaCondition expressions) and derived-field definitions, loaded from one JSON block shared by
// presets, scenarios and T-2. This header covers load, validation, the data model and Resolve over base
// fields; derived-field computing is separate (later) work.
#pragma once

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>

#include <cstdint>
#include <memory>

namespace Dia { namespace Pathfinding { class HexTopology; } }

namespace Draoi {
namespace HexWorld {

class WorldCellFields;

enum class DerivedKind : uint8_t { Distance, Upstream };

// First failure of a Load. kind: "block", "selectors", "selector", "derived". item: the offending selector or derived
// field name (or the block name for shape and cap failures).
struct SelectorError
{
    Dia::Core::Containers::String32  kind;
    Dia::Core::Containers::String64  item;
    Dia::Core::Containers::String256 reason;
};

// distance {from}: multi-source hex-step distance from the cells of selector `sourceName`.
// upstream {of, maxSteps}: cells within maxSteps flow steps upstream of selector `sourceName`.
struct DerivedField
{
    Dia::Core::StringCRC             id;
    Dia::Core::Containers::String64  name;
    DerivedKind                      kind = DerivedKind::Distance;
    Dia::Core::Containers::String64  sourceName;     // selector named by from/of
    uint32_t                         sourceSelector = 0;  // index into CellSelectorSet selectors
    int32_t                          maxSteps = 0;   // Upstream only; >= 1
    uint16_t                         dependsMask = 0; // bit i set: depends (via its source selector) on derived field i

    bool DependsOn(uint32_t derivedIndex) const { return (dependsMask >> derivedIndex) & 1u; }
};

class CellSelectorSet
{
public:
    static constexpr int kMaxSelectors = 32;   // declared selectors; the predefined `all` is not counted
    static constexpr int kMaxDerived   = 16;

    CellSelectorSet();
    CellSelectorSet(CellSelectorSet&&) noexcept = default;
    CellSelectorSet& operator=(CellSelectorSet&&) noexcept = default;
    CellSelectorSet(const CellSelectorSet&) = delete;
    CellSelectorSet& operator=(const CellSelectorSet&) = delete;

    // All-or-nothing: on failure *this is unchanged and err names the first failing item. `baseFields` answers Has for
    // everything except slot "derived", which answers true for the derived names declared in the block.
    bool Load(const Json::Value& block, const WorldCellFields& baseFields, SelectorError& err);

    // Selector index 0 is always the predefined `all` (no expression; selects every cell). Declared selectors follow
    // in name order.
    uint32_t SelectorCount() const { return mCount; }
    int      FindSelector(Dia::Core::StringCRC id) const;   // -1 when absent
    bool     IsAll(uint32_t selector) const { return selector == 0; }
    const char*                              SelectorName(uint32_t selector) const { return mSelectors[selector].name.AsCStr(); }
    const Dia::Condition::ConditionExpr&     SelectorExpr(uint32_t selector) const { return mSelectors[selector].expr; }

    // Resolve: evaluates every selector over cells 0..cellCount-1 (`all` selects every cell) and stores each result as a
    // strictly increasing cell-index list in one pooled array owned by the set. Lists are deterministic (same fields,
    // byte-identical lists). Re-resolving replaces all results; pointers are valid until the next Resolve or Load.
    // An empty list is valid ("empty where used" is the consumer's check). Memory: SelectorCount() * cellCount uint32_t.
    //
    // D9 seam: this overload only knows base fields. A selector whose expression reads slot "derived" cannot be
    // evaluated correctly without computed derived values, so Resolve returns false (err.kind "resolve", err.item the
    // selector name) and leaves no results (all counts 0) rather than produce wrong data. D9 adds an overload taking
    // the grid topology that, before selector evaluation, computes each derived field in DerivedOrder() order (a
    // derived field's source selector is resolved first, then its values are published to a context wrapper that
    // answers slot "derived"); selectors are then evaluated through that wrapper. UsesDerived() tells which selectors
    // need it.
    bool Resolve(WorldCellFields& fields, uint32_t cellCount, SelectorError& err);

    // Resolve with derived fields (D9). cellCount = topology.CellCount(). Order: selectors that do not read derived
    // fields are evaluated first; then each derived field in DerivedOrder(): its source selector is evaluated if it has
    // not been yet (its own derived inputs were computed earlier), the field is computed with the DiaPathfinding
    // MultiSourceSteps helper and published; finally the remaining selectors are evaluated. Selectors read slot
    // "derived" through a context wrapper over `fields`; every other slot delegates to `fields`. On failure no results
    // are kept (all counts 0, DerivedValues null).
    //
    // Derived value encoding (float per cell, DerivedValues(i)[cell]):
    //   distance {from}: hex-step count over all cells (every cell passable, HexNeighbours), 0 on a `from` cell,
    //                    +infinity when no `from` cell is reachable (empty source selector).
    //   upstream {of, maxSteps}: 1.0 when following flow from the cell (cell -> outflow neighbour -> ...) reaches a cell
    //                    of the `of` selector within maxSteps steps INCLUSIVE, else 0.0. Cells of the `of` selector
    //                    are step 0 and therefore always 1.
    //
    // Flow encoding (terrain.flow_direction, read as a float and rounded to nearest): a hex direction index 0..5 in
    // HexTopology order (0=NE 1=N 2=NW 3=SW 4=S 5=SE) naming the neighbour the cell drains into. Anything else
    // (the terrain layer's 6 = "no outflow", negative, > 5.5) means no outflow; an outflow off the grid is dropped.
    // Matches TerrainDrainage / TerrainRivers (flowDirection is uint32, >= 6 = sink or grid edge). The upstream search
    // runs MultiSourceSteps from the `of` cells over the reversed flow graph (AdjacencyGraph, edge outflow -> cell).
    // A set with an upstream field requires terrain.flow_direction to be registered (err.kind "resolve" otherwise).
    bool Resolve(WorldCellFields& fields, const Dia::Pathfinding::HexTopology& topology, SelectorError& err);

    // Computed derived array for derivedIndex (cellCount floats) after a successful topology Resolve; nullptr otherwise.
    const float* DerivedValues(uint32_t derivedIndex) const
    {
        return mDerivedPool ? mDerivedPool.get() + static_cast<size_t>(derivedIndex) * mDerivedCells : nullptr;
    }

    uint32_t        ResolvedCount(uint32_t selector) const { return mResolved[selector].count; }
    // Never null once Resolve has succeeded (count may be 0); valid for ResolvedCount(selector) entries.
    const uint32_t* ResolvedCells(uint32_t selector) const { return mPool.get() + mResolved[selector].offset; }
    bool            UsesDerived(uint32_t selector) const { return mSelectors[selector].usesDerived; }

    uint32_t            DerivedCount() const { return mDerivedCount; }
    int                 FindDerived(Dia::Core::StringCRC id) const;   // -1 when absent
    const DerivedField& Derived(uint32_t index) const { return mDerived[index]; }
    // Evaluation order: DerivedOrder(0..DerivedCount()-1) are derived indices, every field after the fields it depends on.
    uint32_t            DerivedOrder(uint32_t position) const { return mOrder[position]; }

private:
    struct Selector
    {
        Dia::Core::StringCRC             id;
        Dia::Core::Containers::String64  name;
        Dia::Condition::ConditionExpr    expr;
        bool                             usesDerived = false;
    };

    struct Resolved
    {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    void ClearResolved();
    bool ResolveImpl(WorldCellFields& fields, uint32_t cellCount, const Dia::Pathfinding::HexTopology* topology, SelectorError& err);

    Selector     mSelectors[kMaxSelectors + 1];
    DerivedField mDerived[kMaxDerived];
    uint32_t     mOrder[kMaxDerived] = {};
    Resolved     mResolved[kMaxSelectors + 1];
    std::unique_ptr<uint32_t[]> mPool;   // SelectorCount() * cellCount entries; list s starts at s * cellCount
    std::unique_ptr<float[]> mDerivedPool;   // DerivedCount() * cellCount entries (null until a topology Resolve)
    uint32_t     mDerivedCells = 0;
    uint32_t     mCount = 1;         // `all` is always present
    uint32_t     mDerivedCount = 0;
};

} // namespace HexWorld
} // namespace Draoi
