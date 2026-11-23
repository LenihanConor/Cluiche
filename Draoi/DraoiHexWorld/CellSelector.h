// Named cell selectors (DiaCondition expressions) and derived-field definitions, loaded from one JSON block shared by
// presets, scenarios and T-2. This header covers load, validation and the data model; Resolve and derived-field
// computing are separate (later) work.
#pragma once

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>

#include <cstdint>

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
    };

    Selector     mSelectors[kMaxSelectors + 1];
    DerivedField mDerived[kMaxDerived];
    uint32_t     mOrder[kMaxDerived] = {};
    uint32_t     mCount = 1;          // `all` is always present
    uint32_t     mDerivedCount = 0;
};

} // namespace HexWorld
} // namespace Draoi
