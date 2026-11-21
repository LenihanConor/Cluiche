// Generic special-site guarantee rules. See SiteRules.h.
#include <Shared/WorldGeneration/SiteRules.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;
using Dia::Pathfinding::HexTopology;

namespace
{
    int CellX(const WorldStructure& w, uint32_t cell) { return w.dimensions.width > 0 ? static_cast<int>(cell % static_cast<uint32_t>(w.dimensions.width)) : 0; }
    int CellY(const WorldStructure& w, uint32_t cell) { return w.dimensions.width > 0 ? static_cast<int>(cell / static_cast<uint32_t>(w.dimensions.width)) : 0; }
    bool InMap(const WorldStructure& w, uint32_t cell) { return cell < static_cast<uint32_t>(w.CellCount()); }

    // Sentence-start form of a noun: first letter upper-cased.
    std::string Capitalised(const std::string& s)
    {
        std::string out = s;
        if (!out.empty()) { out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0]))); }
        return out;
    }

    void CellsOfStratum(const WorldStructure& world, int zoneIndex, std::vector<uint32_t>& out)
    {
        const Cell* cells = world.Cells();
        for (int i = 0; i < world.CellCount(); ++i)
        {
            if (cells[i].stratum == static_cast<StratumIndex>(zoneIndex)) { out.push_back(static_cast<uint32_t>(i)); }
        }
    }
}

// ---- reach_any ------------------------------------------------------------------------------------------------------

ReachAnyGuarantee::ReachAnyGuarantee(const GuaranteeEntry& entry)
    : mEntry(entry), mId(entry.id.c_str()), mFrom(entry.from.c_str()), mTo(entry.to.c_str())
{
}

GuaranteeResult ReachAnyGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();

    std::vector<uint32_t> targets;
    if (mEntry.toStratumIndex > 0) { CellsOfStratum(world, mEntry.toStratumIndex, targets); }
    else { for (const Site& s : world.sites.Of(mTo)) { targets.push_back(s.cell); } }

    const std::string noun = Capitalised(mEntry.noun);
    for (const Site& source : world.sites.Of(mFrom))
    {
        ProofPath path;
        if (context.navigation.FindProofPathToAny(source.cell, targets, path))
        {
            result.proofPaths.Add(path);
            continue;
        }
        result.violatingCells.Add(source.cell);
        if (targets.empty())
        {
            result.AddDiagnostic("%s at (%d,%d) unreachable: no %s exist", noun.c_str(), CellX(world, source.cell), CellY(world, source.cell), mEntry.toNounPlural.c_str());
        }
        else
        {
            result.AddDiagnostic("%s at (%d,%d) unreachable from any %s", noun.c_str(), CellX(world, source.cell), CellY(world, source.cell), mEntry.toNoun.c_str());
        }
    }

    result.satisfied = result.violatingCells.IsEmpty();
    return result;
}

// ---- min_spacing ----------------------------------------------------------------------------------------------------

MinSpacingGuarantee::MinSpacingGuarantee(const GuaranteeEntry& entry)
    : mEntry(entry), mId(entry.id.c_str()), mKind(entry.kind.c_str())
{
}

GuaranteeResult MinSpacingGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();

    const std::span<const Site> sites = world.sites.Of(mKind);
    if (sites.size() < 2)
    {
        result.satisfied = true;
        return result;
    }

    const std::string noun = Capitalised(mEntry.noun);
    const std::string nounPlural = Capitalised(mEntry.nounPlural);
    const float minimumSpacing = mEntry.minimum;
    const HexTopology topology(std::max(1, world.dimensions.width), std::max(1, world.dimensions.height));
    bool outsideMap = false;
    for (const Site& s : sites)
    {
        if (!InMap(world, s.cell))
        {
            outsideMap = true;
            result.violatingCells.Add(s.cell);
            result.AddDiagnostic("%s at cell %u is outside the map", noun.c_str(), s.cell);
        }
    }

    Measurement minimum;
    minimum.keyCrc = StringCRC(mEntry.minimumKey.c_str()).Value();
    minimum.value = minimumSpacing;
    result.measurements.Add(minimum);

    const uint32_t distanceKey = StringCRC(mEntry.distanceKey.c_str()).Value();
    for (size_t i = 0; i < sites.size(); ++i)
    {
        for (size_t j = i + 1; j < sites.size(); ++j)
        {
            if (!InMap(world, sites[i].cell) || !InMap(world, sites[j].cell)) { continue; }
            const uint32_t distance = HexTopology::Distance(topology.ToCoord(sites[i].cell), topology.ToCoord(sites[j].cell));
            Measurement m;
            m.keyCrc = distanceKey;
            m.a = sites[i].cell;
            m.b = sites[j].cell;
            m.value = static_cast<float>(distance);
            result.measurements.Add(m);
            if (static_cast<float>(distance) < minimumSpacing)
            {
                result.violatingPairs.Add({ sites[i].cell, sites[j].cell });
                result.AddDiagnostic("%s at (%d,%d) and (%d,%d) too close: distance %u < minimum %g",
                                     nounPlural.c_str(),
                                     CellX(world, sites[i].cell), CellY(world, sites[i].cell),
                                     CellX(world, sites[j].cell), CellY(world, sites[j].cell), distance, static_cast<double>(minimumSpacing));
            }
        }
    }

    result.satisfied = result.violatingPairs.IsEmpty() && !outsideMap;
    return result;
}

// ---- variant_cover --------------------------------------------------------------------------------------------------

VariantCoverGuarantee::VariantCoverGuarantee(const GuaranteeEntry& entry)
    : mEntry(entry), mId(entry.id.c_str()), mKind(entry.kind.c_str())
{
}

bool VariantCoverGuarantee::AppliesTo(const WorldStructure& world) const
{
    return !world.sites.Of(mKind).empty();
}

GuaranteeResult VariantCoverGuarantee::Check(const ValidationContext& context) const
{
    const WorldStructure& world = context.world;
    GuaranteeResult result;
    result.idCrc = GetId().Value();
    bool ok = true;

    const int variantCount = static_cast<int>(mEntry.variantCount);
    const std::string& noun = mEntry.noun;
    const std::string& nounPlural = mEntry.nounPlural;
    const std::string& variantNoun = mEntry.variantNoun;
    const std::string nounCap = Capitalised(noun);

    // Existence: exactly variantCount, one per variant.
    const std::span<const Site> sites = world.sites.Of(mKind);
    std::vector<int> perVariant(static_cast<size_t>(variantCount), 0);
    for (const Site& t : sites)
    {
        if (t.variant < variantCount)
        {
            ++perVariant[t.variant];
        }
        else
        {
            ok = false;
            result.AddDiagnostic("%s at (%d,%d) has invalid %s %u", nounCap.c_str(), CellX(world, t.cell), CellY(world, t.cell), variantNoun.c_str(), static_cast<unsigned>(t.variant));
        }
    }
    if (static_cast<int>(sites.size()) != variantCount)
    {
        ok = false;
        result.AddDiagnostic("expected %d %s, found %d", variantCount, nounPlural.c_str(), static_cast<int>(sites.size()));
    }
    const uint32_t countKey = StringCRC(mEntry.countKey.c_str()).Value();
    for (int variant = 0; variant < variantCount; ++variant)
    {
        const int n = perVariant[static_cast<size_t>(variant)];
        Measurement m;
        m.keyCrc = countKey;
        m.tag = variant;
        m.value = static_cast<float>(n);
        result.measurements.Add(m);
        if (n == 0)
        {
            ok = false;
            result.AddDiagnostic("no %s of %s %d", noun.c_str(), variantNoun.c_str(), variant);
        }
        else if (n > 1)
        {
            ok = false;
            result.AddDiagnostic("%d %s of %s %d", n, nounPlural.c_str(), variantNoun.c_str(), variant);
        }
    }

    // Reachability: each site reaches any cell of the target zone.
    std::vector<uint32_t> targets;
    CellsOfStratum(world, mEntry.toStratumIndex, targets);
    Measurement count;
    count.keyCrc = StringCRC(mEntry.targetCountKey.c_str()).Value();
    count.value = static_cast<float>(targets.size());
    result.measurements.Add(count);

    if (targets.empty())
    {
        ok = false;
        result.AddDiagnostic("no %s cells (terrain cause)", mEntry.targetNoun.c_str());
    }
    else
    {
        for (const Site& t : sites)
        {
            ProofPath path;
            if (context.navigation.FindProofPathToAny(t.cell, targets, path))
            {
                result.proofPaths.Add(path);
                continue;
            }
            ok = false;
            result.violatingCells.Add(t.cell);
            result.AddDiagnostic("%s at (%d,%d) unreachable from the %s", nounCap.c_str(), CellX(world, t.cell), CellY(world, t.cell), mEntry.targetNoun.c_str());
        }
    }

    result.satisfied = ok;
    return result;
}

// ---- factory and set ------------------------------------------------------------------------------------------------

std::unique_ptr<IWorldGuarantee> MakeSiteRule(const GuaranteeEntry& entry)
{
    switch (entry.type)
    {
    case GuaranteeRuleType::MinSpacing:   return std::make_unique<MinSpacingGuarantee>(entry);
    case GuaranteeRuleType::VariantCover: return std::make_unique<VariantCoverGuarantee>(entry);
    case GuaranteeRuleType::ReachAny:
    default:                              return std::make_unique<ReachAnyGuarantee>(entry);
    }
}

SpecValidationSet::SpecValidationSet(const GuaranteeSpec& spec, const TerrainRules& rules)
    : mProfile(rules)
{
    mParams.AddProfile(&mProfile);
    for (const GuaranteeEntry& entry : spec.guarantees)
    {
        mRules.push_back(MakeSiteRule(entry));
        mParams.AddGuarantee(mRules.back().get());
    }
}

} // namespace WorldGeneration
} // namespace CoW
