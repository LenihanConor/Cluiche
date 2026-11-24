// Generic special-site guarantee rules: the three rule types a guarantee entry can declare (reach_any, min_spacing,
// variant_cover), each an IWorldGuarantee parameterised by a GuaranteeEntry (kinds, nouns, measurement keys, zone,
// minimum). No game vocabulary: the ids, nouns and keys all come from data. Diagnostics and measurements are
// reproduced from the entry's noun/key fields.
// Evidence: reach_any = proof path per reachable source site + violating source cells; min_spacing = violating pairs +
// distance measurement for every pair + one minimum measurement; variant_cover = proof path per reachable site,
// violating sites, count per variant (tag = variant), target cell count.
// Spec: docs/specs/features/draoi/guarantee-set-injection.md
#pragma once

#include "GuaranteeSpec.h"
#include "TerrainTableNavigationProfile.h"
#include "ValidationTypes.h"

#include <memory>
#include <vector>

namespace Draoi {
namespace HexWorld {

// Every 'from' site must reach at least one 'to' site (or any cell of 'toZone') under any injected profile.
class ReachAnyGuarantee final : public IWorldGuarantee
{
public:
    explicit ReachAnyGuarantee(const GuaranteeEntry& entry);
    Dia::Core::StringCRC GetId() const override { return mId; }
    GuaranteeResult Check(const ValidationContext& context) const override;

private:
    GuaranteeEntry       mEntry;
    Dia::Core::StringCRC mId;
    Dia::Core::StringCRC mFrom;
    Dia::Core::StringCRC mTo;
};

// Every pair of 'kind' sites is at least the entry's minimum apart (hex steps). Uses no navigation.
class MinSpacingGuarantee final : public IWorldGuarantee
{
public:
    explicit MinSpacingGuarantee(const GuaranteeEntry& entry);
    Dia::Core::StringCRC GetId() const override { return mId; }
    GuaranteeResult Check(const ValidationContext& context) const override;
    float MinimumSpacing() const { return mEntry.minimum; }

private:
    GuaranteeEntry       mEntry;
    Dia::Core::StringCRC mId;
    Dia::Core::StringCRC mKind;
};

// Exactly variantCount sites of 'kind', one per variant, each reaching some cell of the entry's zone.
class VariantCoverGuarantee final : public IWorldGuarantee
{
public:
    explicit VariantCoverGuarantee(const GuaranteeEntry& entry);
    Dia::Core::StringCRC GetId() const override { return mId; }
    GuaranteeResult Check(const ValidationContext& context) const override;

    Dia::Core::StringCRC Kind() const { return mKind; }
    // False when the world holds no site of the kind (callers may skip the rule then).
    bool AppliesTo(const WorldStructure& world) const;

private:
    GuaranteeEntry       mEntry;
    Dia::Core::StringCRC mId;
    Dia::Core::StringCRC mKind;
};

// The rule for an entry's type; never null.
std::unique_ptr<IWorldGuarantee> MakeSiteRule(const GuaranteeEntry& entry);

// Owns the placeholder terrain-table profile and one rule per spec entry (file order) and wires them into a
// ValidationParams. Not copyable or movable (the params point at its members). Callers append extra profiles/guarantees
// to a copy of GetParams().
class SpecValidationSet
{
public:
    SpecValidationSet(const GuaranteeSpec& spec, const TerrainRules& rules);
    SpecValidationSet(const SpecValidationSet&) = delete;
    SpecValidationSet& operator=(const SpecValidationSet&) = delete;

    const ValidationParams& GetParams() const { return mParams; }

private:
    TerrainTableNavigationProfile                 mProfile;
    std::vector<std::unique_ptr<IWorldGuarantee>> mRules;
    ValidationParams                              mParams;
};

} // namespace HexWorld
} // namespace Draoi
