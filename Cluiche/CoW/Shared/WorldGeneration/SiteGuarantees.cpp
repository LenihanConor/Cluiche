// S-1 Reachability Validator: built-in site guarantees. See SiteGuarantees.h.
#include <Shared/WorldGeneration/SiteGuarantees.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;
using Dia::Pathfinding::HexTopology;

namespace SiteKinds
{
    StringCRC Hearth()       { return StringCRC("CoW.Site.Hearth"); }
    StringCRC Nexus()        { return StringCRC("CoW.Site.Nexus"); }
    StringCRC DynastyStart() { return StringCRC("CoW.Site.DynastyStart"); }
    StringCRC WorldThrone()  { return StringCRC("CoW.Site.WorldThrone"); }
}

namespace
{
    int CellX(const WorldStructure& w, uint32_t cell) { return w.dimensions.width > 0 ? static_cast<int>(cell % static_cast<uint32_t>(w.dimensions.width)) : 0; }
    int CellY(const WorldStructure& w, uint32_t cell) { return w.dimensions.width > 0 ? static_cast<int>(cell / static_cast<uint32_t>(w.dimensions.width)) : 0; }
    bool InMap(const WorldStructure& w, uint32_t cell) { return cell < static_cast<uint32_t>(w.CellCount()); }
}

// ---- Hearth <-> Nexus -----------------------------------------------------------------------------------------------

StringCRC HearthNexusReachabilityGuarantee::GetId() const { return StringCRC("CoW.Guarantee.HearthNexus"); }

GuaranteeResult HearthNexusReachabilityGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();

    std::vector<uint32_t> nexuses;
    for (const Site& s : world.sites.Of(SiteKinds::Nexus())) { nexuses.push_back(s.cell); }

    for (const Site& hearth : world.sites.Of(SiteKinds::Hearth()))
    {
        ProofPath path;
        if (context.navigation.FindProofPathToAny(hearth.cell, nexuses, path))
        {
            result.proofPaths.Add(path);
            continue;
        }
        result.violatingCells.Add(hearth.cell);
        if (nexuses.empty())
        {
            result.AddDiagnostic("Hearth at (%d,%d) unreachable: no Nexus Nodes exist", CellX(world, hearth.cell), CellY(world, hearth.cell));
        }
        else
        {
            result.AddDiagnostic("Hearth at (%d,%d) unreachable from any Nexus", CellX(world, hearth.cell), CellY(world, hearth.cell));
        }
    }

    result.satisfied = result.violatingCells.IsEmpty();
    return result;
}

// ---- Dynasty spacing ------------------------------------------------------------------------------------------------

StringCRC DynastySpacingGuarantee::GetId() const { return StringCRC("CoW.Guarantee.DynastySpacing"); }

GuaranteeResult DynastySpacingGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();

    const std::span<const Site> starts = world.sites.Of(SiteKinds::DynastyStart());
    if (starts.size() < 2)
    {
        result.satisfied = true;
        return result;
    }

    const HexTopology topology(std::max(1, world.dimensions.width), std::max(1, world.dimensions.height));
    bool outsideMap = false;
    for (const Site& s : starts)
    {
        if (!InMap(world, s.cell))
        {
            outsideMap = true;
            result.violatingCells.Add(s.cell);
            result.AddDiagnostic("Dynasty start at cell %u is outside the map", s.cell);
        }
    }

    Measurement minimum;
    minimum.keyCrc = MeasurementKeys::MinimumSpacing().Value();
    minimum.value = mMinimumSpacing;
    result.measurements.Add(minimum);

    for (size_t i = 0; i < starts.size(); ++i)
    {
        for (size_t j = i + 1; j < starts.size(); ++j)
        {
            if (!InMap(world, starts[i].cell) || !InMap(world, starts[j].cell)) { continue; }
            const uint32_t distance = HexTopology::Distance(topology.ToCoord(starts[i].cell), topology.ToCoord(starts[j].cell));
            Measurement m;
            m.keyCrc = MeasurementKeys::PairDistance().Value();
            m.a = starts[i].cell;
            m.b = starts[j].cell;
            m.value = static_cast<float>(distance);
            result.measurements.Add(m);
            if (static_cast<float>(distance) < mMinimumSpacing)
            {
                result.violatingPairs.Add({ starts[i].cell, starts[j].cell });
                result.AddDiagnostic("Dynasty starts at (%d,%d) and (%d,%d) too close: distance %u < minimum %g",
                                     CellX(world, starts[i].cell), CellY(world, starts[i].cell),
                                     CellX(world, starts[j].cell), CellY(world, starts[j].cell), distance, static_cast<double>(mMinimumSpacing));
            }
        }
    }

    result.satisfied = result.violatingPairs.IsEmpty() && !outsideMap;
    return result;
}

// ---- World thrones --------------------------------------------------------------------------------------------------

StringCRC WorldThroneGuarantee::GetId() const { return StringCRC("CoW.Guarantee.WorldThrone"); }

GuaranteeResult WorldThroneGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();
    bool ok = true;

    // Existence: exactly six, one per type.
    const std::span<const Site> thrones = world.sites.Of(SiteKinds::WorldThrone());
    int perType[kApotheosisTypeCount] = {};
    for (const Site& t : thrones)
    {
        if (t.variant < kApotheosisTypeCount)
        {
            ++perType[t.variant];
        }
        else
        {
            ok = false;
            result.AddDiagnostic("World-throne at (%d,%d) has invalid Apotheosis type %u", CellX(world, t.cell), CellY(world, t.cell), static_cast<unsigned>(t.variant));
        }
    }
    if (static_cast<int>(thrones.size()) != kApotheosisTypeCount)
    {
        ok = false;
        result.AddDiagnostic("expected %d world-thrones, found %d", kApotheosisTypeCount, static_cast<int>(thrones.size()));
    }
    for (int type = 0; type < kApotheosisTypeCount; ++type)
    {
        Measurement m;
        m.keyCrc = MeasurementKeys::ThronesOfType().Value();
        m.tag = type;
        m.value = static_cast<float>(perType[type]);
        result.measurements.Add(m);
        if (perType[type] == 0)
        {
            ok = false;
            result.AddDiagnostic("no world-throne of Apotheosis type %d", type);
        }
        else if (perType[type] > 1)
        {
            ok = false;
            result.AddDiagnostic("%d world-thrones of Apotheosis type %d", perType[type], type);
        }
    }

    // Reachability: each throne reaches any Ancient Interiors cell.
    std::vector<uint32_t> interiors;
    const Cell* cells = world.Cells();
    for (int i = 0; i < world.CellCount(); ++i)
    {
        if (cells[i].depthZone == DepthZone::AncientInteriors) { interiors.push_back(static_cast<uint32_t>(i)); }
    }
    Measurement count;
    count.keyCrc = MeasurementKeys::AncientInteriorsCells().Value();
    count.value = static_cast<float>(interiors.size());
    result.measurements.Add(count);

    if (interiors.empty())
    {
        ok = false;
        result.AddDiagnostic("no Ancient Interiors cells (terrain cause)");
    }
    else
    {
        for (const Site& t : thrones)
        {
            ProofPath path;
            if (context.navigation.FindProofPathToAny(t.cell, interiors, path))
            {
                result.proofPaths.Add(path);
                continue;
            }
            ok = false;
            result.violatingCells.Add(t.cell);
            result.AddDiagnostic("World-throne at (%d,%d) unreachable from the Ancient Interiors", CellX(world, t.cell), CellY(world, t.cell));
        }
    }

    result.satisfied = ok;
    return result;
}

// ---- DefaultValidationSet -------------------------------------------------------------------------------------------

DefaultValidationSet::DefaultValidationSet(const TerrainRules& rules, float minimumDynastySpacing)
    : mProfile(rules)
    , mDynastySpacing(minimumDynastySpacing)
{
    mParams.AddProfile(&mProfile);
    mParams.AddGuarantee(&mHearthNexus);
    mParams.AddGuarantee(&mDynastySpacing);
    mParams.AddGuarantee(&mWorldThrone);
}

} // namespace WorldGeneration
} // namespace CoW
