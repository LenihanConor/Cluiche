// S-1 Reachability Validator: the three site-based built-in guarantees and the default validation set.
// They are generic over SiteSet kinds. S-1 registers no such kinds and has no placer for them: the dragon layer
// registers the kinds through the site hook (Site::cell = cell index, Site::variant = Apotheosis type 0..5 for
// world-thrones). Their ACs run on hand-authored worlds.
// Evidence per guarantee (spec table): HearthNexus = proof path per reachable Hearth + violating Hearth cells;
// DynastySpacing = violating pairs + pairDistance for every pair + one minimumSpacing; WorldThrone = proof path per
// reachable throne, violating thrones, thronesOfType (tag = type) per type, ancientInteriorsCells.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Built-in Guarantees)
#pragma once

#include "TerrainTableNavigationProfile.h"
#include "ValidationTypes.h"

namespace CoW {
namespace WorldGeneration {

namespace SiteKinds
{
    Dia::Core::StringCRC Hearth();         // "CoW.Site.Hearth"
    Dia::Core::StringCRC Nexus();          // "CoW.Site.Nexus"
    Dia::Core::StringCRC DynastyStart();   // "CoW.Site.DynastyStart"
    Dia::Core::StringCRC WorldThrone();    // "CoW.Site.WorldThrone"
}

constexpr int kApotheosisTypeCount = 6;
constexpr float kDefaultMinimumDynastySpacing = 10.0f;   // hex steps; placeholder (GDD open question 1)

// Every Hearth site must reach at least one Nexus site under any injected profile.
class HearthNexusReachabilityGuarantee final : public IWorldGuarantee
{
public:
    Dia::Core::StringCRC GetId() const override;   // "CoW.Guarantee.HearthNexus"
    GuaranteeResult Check(const ValidationContext& context) const override;
};

// Every pair of dynasty starts is at least minimumSpacing hex steps apart. Uses no navigation.
class DynastySpacingGuarantee final : public IWorldGuarantee
{
public:
    explicit DynastySpacingGuarantee(float minimumSpacing = kDefaultMinimumDynastySpacing) : mMinimumSpacing(minimumSpacing) {}
    Dia::Core::StringCRC GetId() const override;   // "CoW.Guarantee.DynastySpacing"
    GuaranteeResult Check(const ValidationContext& context) const override;
    float MinimumSpacing() const { return mMinimumSpacing; }

private:
    float mMinimumSpacing;
};

// Exactly six world-thrones, one per Apotheosis type, each reaching some Ancient Interiors cell.
class WorldThroneGuarantee final : public IWorldGuarantee
{
public:
    Dia::Core::StringCRC GetId() const override;   // "CoW.Guarantee.WorldThrone"
    GuaranteeResult Check(const ValidationContext& context) const override;
};

// Owns the placeholder terrain-table profile and the three built-ins and wires them into a ValidationParams.
// Not copyable or movable (the params point at its members). Callers append extra profiles/guarantees
// (e.g. B2's body-plan profiles) to a copy of GetParams().
class DefaultValidationSet
{
public:
    explicit DefaultValidationSet(float minimumDynastySpacing = kDefaultMinimumDynastySpacing);
    DefaultValidationSet(const DefaultValidationSet&) = delete;
    DefaultValidationSet& operator=(const DefaultValidationSet&) = delete;

    const ValidationParams& GetParams() const { return mParams; }

private:
    TerrainTableNavigationProfile        mProfile;
    HearthNexusReachabilityGuarantee     mHearthNexus;
    DynastySpacingGuarantee              mDynastySpacing;
    WorldThroneGuarantee                 mWorldThrone;
    ValidationParams                     mParams;
};

} // namespace WorldGeneration
} // namespace CoW
