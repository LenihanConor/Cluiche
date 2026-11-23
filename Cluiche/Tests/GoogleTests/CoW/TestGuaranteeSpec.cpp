// Guarantee spec: data-declared site kinds, guarantee entries and marker overlay; fail-loud all-or-nothing loader;
// SiteRegistry data-declared kinds.
#include <gtest/gtest.h>

#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorldTerrain/GuaranteeSpec.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DraoiHexWorldTerrain/Testing/TestPlacers.h>

#include <set>
#include <string>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using Draoi::HexWorld::Testing::ShippedStratumTable;
using Dia::Core::StringCRC;

namespace {

std::string ShippedText() { return Draoi::HexWorld::Testing::Detail::ReadFileText(ShippedGuaranteesPath()); }
std::string Sidecar() { return Draoi::HexWorld::Testing::Detail::ReadFileText(ShippedGuaranteesSidecarPath()); }

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const size_t at = text.find(from);
    EXPECT_NE(at, std::string::npos) << "fixture text '" << from << "' not found";
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

// Loads 'text' into a spec pre-filled with the shipped one; on failure it must be left untouched.
bool LoadFaulty(const std::string& text, GuaranteeSpecError& err)
{
    GuaranteeSpec out = ShippedGuaranteeSpec();
    const size_t kinds = out.siteKinds.size();
    const size_t gs = out.guarantees.size();
    const bool ok = LoadGuaranteeSpec(text.c_str(), Sidecar().c_str(), ShippedStratumTable(), out, err);
    if (!ok)
    {
        EXPECT_EQ(out.siteKinds.size(), kinds);
        EXPECT_EQ(out.guarantees.size(), gs);
        EXPECT_EQ(out.guarantees[0].id, "CoW.Guarantee.HearthNexus");
    }
    return ok;
}

void ExpectFails(const std::string& text, const char* entry)
{
    GuaranteeSpecError err;
    EXPECT_FALSE(LoadFaulty(text, err));
    EXPECT_NE(err.entry.find(entry), std::string::npos) << "entry was '" << err.entry << "' reason '" << err.reason << "'";
    EXPECT_FALSE(err.reason.empty());
}

} // namespace

TEST(GuaranteeSpec, ShippedFileLoads)
{
    const GuaranteeSpec& s = ShippedGuaranteeSpec();
    ASSERT_EQ(s.siteKinds.size(), 4u);
    EXPECT_EQ(s.siteKinds[0].id, "CoW.Site.Hearth");
    EXPECT_EQ(s.siteKinds[3].id, "CoW.Site.WorldThrone");
    EXPECT_EQ(s.siteKinds[3].variantLabel, "Apotheosis");
    EXPECT_EQ(s.siteKinds[3].variantCount, 6u);
    EXPECT_EQ(s.siteKinds[3].shape, SiteShape::Star);

    ASSERT_EQ(s.guarantees.size(), 3u);
    EXPECT_EQ(s.guarantees[0].id, "CoW.Guarantee.HearthNexus");
    EXPECT_EQ(s.guarantees[0].type, GuaranteeRuleType::ReachAny);
    EXPECT_EQ(s.guarantees[0].from, "CoW.Site.Hearth");
    EXPECT_EQ(s.guarantees[0].to, "CoW.Site.Nexus");
    EXPECT_EQ(s.guarantees[0].noun, "Hearth");
    EXPECT_EQ(s.guarantees[0].toNounPlural, "Nexus Nodes");
    EXPECT_EQ(s.guarantees[1].id, "CoW.Guarantee.DynastySpacing");
    EXPECT_EQ(s.guarantees[1].type, GuaranteeRuleType::MinSpacing);
    EXPECT_EQ(s.guarantees[1].kind, "CoW.Site.DynastyStart");
    EXPECT_EQ(s.guarantees[1].minimumKey, "minimumSpacing");
    EXPECT_EQ(s.guarantees[1].distanceKey, "pairDistance");
    EXPECT_EQ(s.guarantees[2].id, "CoW.Guarantee.WorldThrone");
    EXPECT_EQ(s.guarantees[2].type, GuaranteeRuleType::VariantCover);
    EXPECT_EQ(s.guarantees[2].variantCount, 6u);
    EXPECT_EQ(s.guarantees[2].toStratum, "AncientInteriors");
    EXPECT_EQ(s.guarantees[2].toStratumIndex, static_cast<int>(ShippedStratumTable().Last()));
    EXPECT_EQ(s.guarantees[2].countKey, "thronesOfType");
    EXPECT_EQ(s.guarantees[2].targetCountKey, "ancientInteriorsCells");

    ASSERT_TRUE(s.hasMarkerOverlay);
    EXPECT_EQ(s.markerOverlay.overlayId, "dynasty.starts");
    EXPECT_EQ(s.markerOverlay.label, "Dynasty starts");
    EXPECT_EQ(s.markerOverlay.kind, "CoW.Site.DynastyStart");
    EXPECT_EQ(s.markerOverlay.spacingGuarantee, "CoW.Guarantee.DynastySpacing");
    EXPECT_EQ(s.markerOverlay.fallbackRing, 10.0f);
}

TEST(GuaranteeSpec, SidecarDefaultMinimumSpacingIsTen)
{
    EXPECT_EQ(ShippedGuaranteeSpec().guarantees[1].minimum, 10.0f);
}

TEST(GuaranteeSpec, EmptyDocumentLoadsEmptySpec)
{
    GuaranteeSpec out = ShippedGuaranteeSpec();
    GuaranteeSpecError err;
    const std::string text = "{\"format\":\"cow.guarantees\",\"version\":1}";
    ASSERT_TRUE(LoadGuaranteeSpec(text.c_str(), Sidecar().c_str(), ShippedStratumTable(), out, err));
    EXPECT_TRUE(out.siteKinds.empty());
    EXPECT_TRUE(out.guarantees.empty());
    EXPECT_FALSE(out.hasMarkerOverlay);
}

TEST(GuaranteeSpec, UnknownRuleTypeFailsNamingEntry)
{
    ExpectFails(Replace(ShippedText(), "\"min_spacing\"", "\"min_spacingx\""), "CoW.Guarantee.DynastySpacing");
}

TEST(GuaranteeSpec, UnknownZoneFailsNamingEntry)
{
    ExpectFails(Replace(ShippedText(), "\"AncientInteriors\"", "\"Nowhere\""), "CoW.Guarantee.WorldThrone");
}

TEST(GuaranteeSpec, NoneIsNotAZoneTarget)
{
    ExpectFails(Replace(ShippedText(), "\"AncientInteriors\"", "\"None\""), "CoW.Guarantee.WorldThrone");
}

TEST(GuaranteeSpec, UndeclaredKindInGuaranteeFails)
{
    ExpectFails(Replace(ShippedText(), "\"to\": \"CoW.Site.Nexus\"", "\"to\": \"CoW.Site.Nowhere\""), "CoW.Guarantee.HearthNexus");
    ExpectFails(Replace(ShippedText(), "\"kind\": \"CoW.Site.WorldThrone\",\n      \"variant_count\"",
                        "\"kind\": \"CoW.Site.Nowhere\",\n      \"variant_count\""), "CoW.Guarantee.WorldThrone");
}

TEST(GuaranteeSpec, UndeclaredKindInOverlayFails)
{
    ExpectFails(Replace(ShippedText(), "\"kind\": \"CoW.Site.DynastyStart\",\n    \"spacing_guarantee\"",
                        "\"kind\": \"CoW.Site.Nowhere\",\n    \"spacing_guarantee\""), "marker_overlay");
}

TEST(GuaranteeSpec, OverlaySpacingGuaranteeMustBeMinSpacing)
{
    ExpectFails(Replace(ShippedText(), "\"spacing_guarantee\": \"CoW.Guarantee.DynastySpacing\"",
                        "\"spacing_guarantee\": \"CoW.Guarantee.HearthNexus\""), "marker_overlay");
}

TEST(GuaranteeSpec, DuplicateKindIdFails)
{
    ExpectFails(Replace(ShippedText(), "\"id\": \"CoW.Site.Nexus\"", "\"id\": \"CoW.Site.Hearth\""), "CoW.Site.Hearth");
}

TEST(GuaranteeSpec, DuplicateGuaranteeIdFails)
{
    ExpectFails(Replace(ShippedText(), "\"id\": \"CoW.Guarantee.DynastySpacing\"", "\"id\": \"CoW.Guarantee.HearthNexus\""),
                "CoW.Guarantee.HearthNexus");
}

TEST(GuaranteeSpec, BadShapeFails)
{
    ExpectFails(Replace(ShippedText(), "\"Diamond\"", "\"Blob\""), "CoW.Site.Nexus");
}

TEST(GuaranteeSpec, BadColourFails)
{
    ExpectFails(Replace(ShippedText(), "\"#5B8DEF\"", "\"blue\""), "CoW.Site.Nexus");
}

TEST(GuaranteeSpec, BadParamsFail)
{
    // Missing noun, wrong type, unknown field, unknown sidecar key, both/neither of to and to_stratum.
    ExpectFails(Replace(ShippedText(), "\"noun\": \"Hearth\",", ""), "CoW.Guarantee.HearthNexus");
    ExpectFails(Replace(ShippedText(), "\"variant_count\": 6,\n      \"to_stratum\"", "\"variant_count\": \"six\",\n      \"to_stratum\""),
                "CoW.Guarantee.WorldThrone");
    ExpectFails(Replace(ShippedText(), "\"label\": \"Dynasty spacing\",", "\"label\": \"Dynasty spacing\", \"bogus\": 1,"),
                "CoW.Guarantee.DynastySpacing");
    ExpectFails(Replace(ShippedText(), "kDynastySpacingMinimum", "kNotInSidecar"), "CoW.Guarantee.DynastySpacing");
    ExpectFails(Replace(ShippedText(), "\"to\": \"CoW.Site.Nexus\",", "\"to\": \"CoW.Site.Nexus\", \"to_stratum\": \"AncientInteriors\","),
                "CoW.Guarantee.HearthNexus");
    ExpectFails(Replace(ShippedText(), "\"to\": \"CoW.Site.Nexus\",", ""), "CoW.Guarantee.HearthNexus");
}

TEST(GuaranteeSpec, MalformedDocumentFails)
{
    ExpectFails("not json", "document");
    ExpectFails("{\"format\":\"cow.strata\",\"version\":1}", "format");
    ExpectFails("{\"format\":\"cow.guarantees\",\"version\":2}", "version");
}

TEST(GuaranteeSpec, NullTextsFail)
{
    GuaranteeSpec out;
    GuaranteeSpecError err;
    EXPECT_FALSE(LoadGuaranteeSpec(nullptr, Sidecar().c_str(), ShippedStratumTable(), out, err));
    EXPECT_FALSE(LoadGuaranteeSpec("{}", nullptr, ShippedStratumTable(), out, err));
    EXPECT_EQ(err.entry, "sidecar");
}

TEST(GuaranteeSpec, SidecarMinimumBelowRangeFailsLoud)
{
    const std::string bad = Replace(Sidecar(), "\"default\": 10.0", "\"default\": 0.0");
    GuaranteeSpec out;
    GuaranteeSpecError err;
    EXPECT_FALSE(LoadGuaranteeSpec(ShippedText().c_str(), bad.c_str(), ShippedStratumTable(), out, err));
    EXPECT_EQ(err.entry, "sidecar");
}

TEST(GuaranteeSpec, InstallSiteKindsListsDataOnlyKinds)
{
    CoWSiteRegistry registry;
    ASSERT_TRUE(InstallSiteKinds(ShippedGuaranteeSpec(), registry));
    EXPECT_EQ(registry.Count(), 0);

    const SiteKindInfo* info = registry.FindInfo(StringCRC("CoW.Site.WorldThrone"));
    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->displayName, "World Throne");
    EXPECT_EQ(info->debugColour, 0xB07CD8u);
    EXPECT_EQ(info->debugShape, SiteShape::Star);
    EXPECT_STREQ(info->variantLabel, "Apotheosis");

    const std::vector<SiteKindInfo> kinds = ListSiteKinds(registry);
    ASSERT_EQ(kinds.size(), 8u);   // four built-in nodes, then four declared
    EXPECT_STREQ(kinds[4].id, "CoW.Site.Hearth");
    EXPECT_STREQ(kinds[7].id, "CoW.Site.WorldThrone");
}

TEST(GuaranteeSpec, InstallRejectsKindOwnedByPlacer)
{
    TestPlacer p("CoW.Site.Nexus");
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&p));
    EXPECT_FALSE(InstallSiteKinds(ShippedGuaranteeSpec(), registry));
}

TEST(GuaranteeSpec, InstallSkipsKindsAPlacerOwnsWhenAsked)
{
    TestPlacer p("CoW.Site.Nexus");
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&p));
    ASSERT_TRUE(InstallSiteKinds(ShippedGuaranteeSpec(), registry, true));
    EXPECT_EQ(ListSiteKinds(registry).size(), 8u);   // four built-in, the placer's Nexus, three declared
    EXPECT_TRUE(InstallSiteKinds(ShippedGuaranteeSpec(), registry, true)) << "idempotent";
    EXPECT_EQ(ListSiteKinds(registry).size(), 8u);
}

TEST(SiteRegistry, DeclaredKindListedBeforePlacerKindsAndFound)
{
    CoWSiteRegistry r;
    TestPlacer a("Test.A");
    ASSERT_TRUE(r.Register(&a));
    ASSERT_TRUE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.Site.Camp"), "Camp", 0x112233, SiteShape::Ring, "Tier")));
    const auto kinds = ListSiteKinds(r);
    ASSERT_EQ(kinds.size(), 6u);
    EXPECT_STREQ(kinds[4].id, "Test.Site.Camp");   // declared first
    EXPECT_STREQ(kinds[5].id, "Test.A");
    const SiteKindInfo* f = r.FindInfo(StringCRC("Test.Site.Camp"));
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->debugColour, 0x112233u);
    EXPECT_EQ(f->debugShape, SiteShape::Ring);
    EXPECT_STREQ(f->variantLabel, "Tier");
    EXPECT_EQ(r.Count(), 1);   // no placer
}

TEST(SiteRegistry, DeclaredKindRejections)
{
    CoWSiteRegistry r;
    TestPlacer a("Test.A");
    ASSERT_TRUE(r.Register(&a));
    EXPECT_FALSE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.A"), "A", 1, SiteShape::Dot)));          // placer-owned
    ASSERT_TRUE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.D"), "D", 1, SiteShape::Dot)));
    EXPECT_FALSE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.D"), "D2", 2, SiteShape::Dot)));         // duplicate
    EXPECT_FALSE(r.RegisterDeclaredKind(SiteKindInfo{}));                                                         // empty id
    EXPECT_FALSE(r.RegisterDeclaredKind(BuiltinNodeKindInfo(CoWNodeKindIds(), BuiltinNodeKinds::River())));                        // built-in
    EXPECT_STREQ(r.FindInfo(StringCRC("Test.D"))->displayName, "D");
}

TEST(SiteRegistry, PlacerAttachesToDeclaredKind)
{
    CoWSiteRegistry r;
    ASSERT_TRUE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.A"), "Declared A", 0x010203, SiteShape::Star)));
    TestPlacer a("Test.A");
    ASSERT_TRUE(r.Register(&a));
    EXPECT_TRUE(r.Declared().empty());
    const SiteKindInfo* info = r.FindInfo(StringCRC("Test.A"));
    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->displayName, "Test.A");   // the placer's Info() replaces the declaration
    EXPECT_EQ(ListSiteKinds(r).size(), 5u);       // listed once
    EXPECT_FALSE(r.RegisterDeclaredKind(MakeSiteKindInfo(StringCRC("Test.A"), "again", 1, SiteShape::Dot)));
}

TEST(GuaranteeSpec, ProtectedFlagTrueLoads)
{
    GuaranteeSpec out;
    GuaranteeSpecError err;
    const std::string text = Replace(ShippedText(), "\"id\": \"CoW.Site.DynastyStart\",", "\"id\": \"CoW.Site.DynastyStart\", \"protected\": true,");
    ASSERT_TRUE(LoadGuaranteeSpec(text.c_str(), Sidecar().c_str(), ShippedStratumTable(), out, err)) << err.entry << ": " << err.reason;
    ASSERT_NE(out.FindKind("CoW.Site.DynastyStart"), nullptr);
    EXPECT_TRUE(out.FindKind("CoW.Site.DynastyStart")->isProtected);
}

TEST(GuaranteeSpec, ProtectedFlagAbsentDefaultsFalse)
{
    GuaranteeSpec out;
    GuaranteeSpecError err;
    const std::string text = "{\"format\":\"cow.guarantees\",\"version\":1,\"site_kinds\":[{\"id\":\"K.A\",\"display_name\":\"A\",\"colour\":\"#112233\",\"shape\":\"Dot\"}]}";
    ASSERT_TRUE(LoadGuaranteeSpec(text.c_str(), Sidecar().c_str(), ShippedStratumTable(), out, err));
    ASSERT_EQ(out.siteKinds.size(), 1u);
    EXPECT_FALSE(out.siteKinds[0].isProtected);
}

TEST(GuaranteeSpec, ProtectedFlagWrongTypeFailsNamingKind)
{
    ExpectFails(Replace(ShippedText(), "\"id\": \"CoW.Site.DynastyStart\",", "\"id\": \"CoW.Site.DynastyStart\", \"protected\": \"yes\","), "site_kinds[CoW.Site.DynastyStart]");
}

TEST(GuaranteeSpec, ShippedProtectedKindsAreHearthNexusWorldThrone)
{
    std::set<std::string> prot;
    for (const SiteKindDecl& k : ShippedGuaranteeSpec().siteKinds) { if (k.isProtected) { prot.insert(k.id); } }
    const std::set<std::string> expected = { "CoW.Site.Hearth", "CoW.Site.Nexus", "CoW.Site.WorldThrone" };
    EXPECT_EQ(prot, expected);
}
