// Generic special sites / POIs: the data a game declares for the world generator. Site kinds (display info),
// ordered guarantee entries over those kinds (three rule types: reach_any, min_spacing, variant_cover) and an
// optional marker overlay, loaded from Global/World/guarantees.json plus the sidecar guarantees.schema.json
// (a Dia parameter file that owns numeric parameters such as the minimum spacing).
// This header only parses and validates; the rule classes that execute the entries live elsewhere.
// No include from Modules/. std::string errors (String32.h collides with protobuf in some test TUs).
#pragma once

#include <DraoiHexWorld/StratumTable.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <DraoiHexWorld/Sites.h>
#include <Shared/WorldGeneration/HexWorldAccess.h>

#include <cstdint>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct SiteKindDecl
{
    std::string id;
    std::string displayName;
    uint32_t    colour = 0xFFFFFF;   // 0xRRGGBB
    SiteShape   shape = SiteShape::Dot;
    std::string variantLabel;
    uint32_t    variantCount = 0;    // 0 = the kind has no variants

    SiteKindInfo ToInfo() const;
};

enum class GuaranteeRuleType : uint8_t { ReachAny, MinSpacing, VariantCover };

// "reach_any" / "min_spacing" / "variant_cover".
const char* GuaranteeRuleTypeName(GuaranteeRuleType type);

// One guarantee entry. Fields are per type; unused ones stay empty/zero. Nouns and measurement key names are data so
// the rule classes can reproduce the diagnostics byte for byte.
//   reach_any:     from, to (kind id) or toStratum; noun (the from kind, e.g. "Camp"), toNoun ("Outpost"), toNounPlural ("Outposts")
//   min_spacing:   kind, minimum (sidecar value), noun ("start"), nounPlural ("starts"),
//                  minimumKey ("minimumSpacing"), distanceKey ("pairDistance")
//   variant_cover: kind, variantCount, toStratum, noun ("landmark"), nounPlural ("landmarks"),
//                  variantNoun ("variant"), targetNoun ("deep cells"),
//                  countKey ("itemsOfVariant"), targetCountKey ("deepCells")
struct GuaranteeEntry
{
    std::string       id;
    std::string       label;
    GuaranteeRuleType type = GuaranteeRuleType::ReachAny;

    std::string from;
    std::string to;
    std::string kind;
    std::string toStratum;            // stratum name, resolved against the StratumTable at load
    int         toStratumIndex = 0;   // 1..Count(); 0 when no stratum is used
    float       minimum = 0.0f;
    uint32_t    variantCount = 0;
    std::string noun;
    std::string nounPlural;
    std::string toNoun;
    std::string toNounPlural;
    std::string variantNoun;
    std::string targetNoun;
    std::string minimumKey;
    std::string distanceKey;
    std::string countKey;
    std::string targetCountKey;
};

struct MarkerOverlaySpec
{
    std::string overlayId;
    std::string label;
    std::string kind;
    std::string spacingGuarantee;   // id of a min_spacing guarantee entry
    float       fallbackRing = 0.0f;
};

struct GuaranteeSpec
{
    std::vector<SiteKindDecl>   siteKinds;
    std::vector<GuaranteeEntry> guarantees;   // file order is the validation order
    bool                        hasMarkerOverlay = false;
    MarkerOverlaySpec           markerOverlay;

    const SiteKindDecl*   FindKind(const std::string& id) const;
    const GuaranteeEntry* FindGuarantee(const std::string& id) const;
};

struct GuaranteeSpecError
{
    std::string entry;    // the entry that failed: "guarantees[Some.Guarantee.Id]", "site_kinds[i]", "document", ...
    std::string reason;
};

// All-or-nothing: on failure 'out' is unchanged and 'err' names the entry. Strata are resolved against 'strata'.
// Fails on: unknown rule type, unknown stratum, kind id referenced but not declared, duplicate kind or guarantee id,
// bad param (missing, wrong type, unknown field, sidecar key absent), bad shape.
bool LoadGuaranteeSpec(const char* text, const char* sidecarText, const Draoi::HexWorld::StratumTable& strata, GuaranteeSpec& out,
                       GuaranteeSpecError& err);

// Registers every declared kind of the spec as a data-declared kind (no placer). False (stopping at the failing
// kind) when a kind id is already a placer kind or already declared. With 'skipExisting' such ids are skipped
// instead (the placer / earlier declaration keeps owning the id); other failures (built-in node kind, empty id)
// still return false.
bool InstallSiteKinds(const GuaranteeSpec& spec, SiteRegistry& registry, bool skipExisting = false);

} // namespace WorldGeneration
} // namespace CoW
