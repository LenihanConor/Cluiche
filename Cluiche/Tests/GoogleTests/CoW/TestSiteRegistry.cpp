// S-1 Regions, Nodes and Site Hook: SiteRegistry (AC9).
#include <gtest/gtest.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>

#include <set>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{
    std::string Names(const SiteRegistry& r)
    {
        std::string s;
        for (ISitePlacer* p : r.Order()) s += p->Info().id[5];   // "Test.X" -> 'X'
        return s;
    }
}

TEST(SiteRegistry, EmptyOrderIsEmpty)
{
    CoWSiteRegistry r;
    EXPECT_TRUE(r.Order().empty());
}

TEST(SiteRegistry, OrderABCInRegistrationOrder)
{
    TestPlacerSet t; CoWSiteRegistry r;
    EXPECT_TRUE(r.Register(&t.a)); EXPECT_TRUE(r.Register(&t.b)); EXPECT_TRUE(r.Register(&t.c));
    EXPECT_EQ(Names(r), "ABC");
}

TEST(SiteRegistry, DependencyPlacedBeforeDependantWhenRegisteredLater)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.b); r.Register(&t.a); r.Register(&t.c);
    EXPECT_EQ(Names(r), "ABC");
}

TEST(SiteRegistry, BCAOrdersDependencyForward)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.b); r.Register(&t.c); r.Register(&t.a);
    EXPECT_EQ(Names(r), "ABC");
}

TEST(SiteRegistry, CBAKeepsCFirst)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.c); r.Register(&t.b); r.Register(&t.a);
    EXPECT_EQ(Names(r), "CAB");
}

TEST(SiteRegistry, ACBKeepsRegistrationOrderOfRest)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.a); r.Register(&t.c); r.Register(&t.b);
    EXPECT_EQ(Names(r), "ACB");
}

TEST(SiteRegistry, CycleRejectedAndRegistryUnchanged)
{
    TestPlacer a("Test.A", { "Test.B" });
    TestPlacer b("Test.B", { "Test.A" });
    CoWSiteRegistry r;
    EXPECT_TRUE(r.Register(&a));            // B not registered yet: allowed
    EXPECT_FALSE(r.Register(&b));           // closes the cycle
    EXPECT_EQ(r.Count(), 1);
    EXPECT_EQ(r.FindInfo(StringCRC("Test.B")), nullptr);
    EXPECT_EQ(Names(r), "A");
}

TEST(SiteRegistry, LongerCycleRejected)
{
    TestPlacer a("Test.A", { "Test.B" }), b("Test.B", { "Test.C" }), c("Test.C", { "Test.A" });
    CoWSiteRegistry r;
    EXPECT_TRUE(r.Register(&a)); EXPECT_TRUE(r.Register(&b));
    EXPECT_FALSE(r.Register(&c));
    EXPECT_EQ(r.Count(), 2);
}

TEST(SiteRegistry, SelfDependencyRejected)
{
    TestPlacer a("Test.A", { "Test.A" });
    CoWSiteRegistry r;
    EXPECT_FALSE(r.Register(&a));
    EXPECT_EQ(r.Count(), 0);
}

TEST(SiteRegistry, DuplicateKindRejected)
{
    TestPlacer a1("Test.A"), a2("Test.A");
    CoWSiteRegistry r;
    EXPECT_TRUE(r.Register(&a1));
    EXPECT_FALSE(r.Register(&a2));
    EXPECT_FALSE(r.Register(&a1));
    EXPECT_EQ(r.Count(), 1);
}

TEST(SiteRegistry, NullRegisterRejected)
{
    CoWSiteRegistry r;
    EXPECT_FALSE(r.Register(nullptr));
    EXPECT_FALSE(r.RegisterGuarantee(nullptr));
}

TEST(SiteRegistry, OrderCacheInvalidatedByRegisterAndUnregister)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.a);
    EXPECT_EQ(Names(r), "A");
    r.Register(&t.b);
    EXPECT_EQ(Names(r), "AB");
    r.Register(&t.c);
    EXPECT_EQ(Names(r), "ABC");
    EXPECT_TRUE(r.Unregister(StringCRC("Test.A")));
    EXPECT_EQ(Names(r), "BC");
    EXPECT_TRUE(r.Register(&t.a));
    EXPECT_EQ(Names(r), "ABC");
}

TEST(SiteRegistry, RejectedRegisterKeepsOrder)
{
    TestPlacerSet t; TestPlacer dup("Test.A");
    CoWSiteRegistry r;
    r.Register(&t.a); r.Register(&t.b);
    EXPECT_EQ(Names(r), "AB");
    EXPECT_FALSE(r.Register(&dup));
    EXPECT_EQ(Names(r), "AB");
}

TEST(SiteRegistry, UnregisterPresentAndAbsent)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.a);
    EXPECT_FALSE(r.Unregister(StringCRC("Test.Nope")));
    EXPECT_TRUE(r.Unregister(StringCRC("Test.A")));
    EXPECT_FALSE(r.Unregister(StringCRC("Test.A")));
    EXPECT_EQ(r.Count(), 0);
}

TEST(SiteRegistry, UnmetDependencyIsSkippedSilently)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.b);                 // depends on A, which is not registered
    r.Register(&t.c);
    EXPECT_EQ(Names(r), "BC");
    r.Register(&t.a);                 // now met
    EXPECT_EQ(Names(r), "ABC");
}

TEST(SiteRegistry, UnregisteringDependencyLeavesDependantOrdered)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.a); r.Register(&t.b);
    r.Unregister(StringCRC("Test.A"));
    EXPECT_EQ(Names(r), "B");
}

TEST(SiteRegistry, FindInfo)
{
    TestPlacerSet t; CoWSiteRegistry r;
    r.Register(&t.a);
    const SiteKindInfo* info = r.FindInfo(StringCRC("Test.A"));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info, &t.a.Info());
    EXPECT_EQ(r.FindInfo(StringCRC("Test.B")), nullptr);
    r.Unregister(StringCRC("Test.A"));
    EXPECT_EQ(r.FindInfo(StringCRC("Test.A")), nullptr);
}

TEST(SiteRegistry, FindInfoDoesNotResolveBuiltInNodeKinds)
{
    CoWSiteRegistry r;
    EXPECT_EQ(r.FindInfo(BuiltinNodeKinds::Corridor()), nullptr);
    EXPECT_EQ(r.FindInfo(BuiltinNodeKinds::River()), nullptr);
    EXPECT_EQ(r.FindInfo(BuiltinNodeKinds::Hunting()), nullptr);
    EXPECT_EQ(r.FindInfo(BuiltinNodeKinds::Resource()), nullptr);
}

TEST(SiteRegistry, RegisterGuarantee)
{
    TestGuarantee g1("one"), g1dup("one"), g2("two");
    CoWSiteRegistry r;
    EXPECT_TRUE(r.Guarantees().empty());
    EXPECT_TRUE(r.RegisterGuarantee(&g1));
    EXPECT_FALSE(r.RegisterGuarantee(&g1));      // same pointer
    EXPECT_FALSE(r.RegisterGuarantee(&g1dup));   // same name
    EXPECT_TRUE(r.RegisterGuarantee(&g2));
    ASSERT_EQ(r.Guarantees().size(), 2u);
    EXPECT_EQ(r.Guarantees()[0], &g1);
    EXPECT_EQ(r.Guarantees()[1], &g2);
}

TEST(SiteRegistry, TestPlacerPlacesOrFails)
{
    WorldStructure w; w.Resize({ 4, 4 });
    PlacementContext ctx{ w, w.dimensions, 0 };
    Dia::Maths::RandomStream rng(1);
    SiteSet out;
    TestPlacer ok("Test.A", {}, 5);
    EXPECT_TRUE(ok.Place(ctx, rng, out).IsOk());
    EXPECT_EQ(out.Count(StringCRC("Test.A")), 1);
    TestPlacer retry("Test.R", {}, 0, PlaceResult::Code::Retryable);
    EXPECT_EQ(retry.Place(ctx, rng, out).code, PlaceResult::Code::Retryable);
    TestPlacer terr("Test.T", {}, 0, PlaceResult::Code::TerrainCause);
    EXPECT_EQ(terr.Place(ctx, rng, out).code, PlaceResult::Code::TerrainCause);
}

// ---- Regions plan task 10: kind listing for the Sites overlay ----

TEST(SiteRegistry, ListSiteKindsIsTheFourBuiltinsThenRegisteredInOrder)
{
    CoWSiteRegistry r;
    std::vector<SiteKindInfo> k = ListSiteKinds(r);
    ASSERT_EQ(k.size(), 4u);
    EXPECT_EQ(k[0].kindCrc, SiteKindId(BuiltinNodeKinds::Corridor()));
    EXPECT_EQ(k[1].kindCrc, SiteKindId(BuiltinNodeKinds::River()));
    EXPECT_EQ(k[2].kindCrc, SiteKindId(BuiltinNodeKinds::Hunting()));
    EXPECT_EQ(k[3].kindCrc, SiteKindId(BuiltinNodeKinds::Resource()));
    for (const SiteKindInfo& i : k)
        EXPECT_NE(i.displayName[0], '\0') << i.id << ": built-ins carry display info";

    TestPlacerSet p;
    ASSERT_TRUE(r.Register(&p.b));
    ASSERT_TRUE(r.Register(&p.c));
    ASSERT_TRUE(r.Register(&p.a));
    k = ListSiteKinds(r);
    ASSERT_EQ(k.size(), 7u);
    EXPECT_STREQ(k[4].id, "Test.A");   // Order(): A pulled forward before its dependant B
    EXPECT_STREQ(k[5].id, "Test.B");
    EXPECT_STREQ(k[6].id, "Test.C");
    EXPECT_EQ(k[4].debugColour, 0xFF00FFu);
    EXPECT_EQ(k[4].debugShape, SiteShape::Dot);
}

TEST(SiteRegistry, SiteShapeNamesAreDistinct)
{
    std::set<std::string> names;
    for (SiteShape s : { SiteShape::Dot, SiteShape::Diamond, SiteShape::Square, SiteShape::Triangle, SiteShape::Star, SiteShape::Ring })
        names.insert(SiteShapeName(s));
    EXPECT_EQ(names.size(), 6u);
    EXPECT_STREQ(SiteShapeName(SiteShape::Star), "Star");
}
