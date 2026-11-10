#include <gtest/gtest.h>

#include <DiaCore/Json/external/json/json.h>
#include <Modules/DomainRootDelta.h>

using Cluiche::AppFlow::BuildPartialRoot;
using Cluiche::AppFlow::ComputeDomainRootDelta;
using Cluiche::AppFlow::DomainRootDelta;

namespace
{
	Json::Value Root(int a, int b, int frame)
	{
		Json::Value r(Json::objectValue);
		r["A"]["v"] = a;
		r["B"]["v"] = b;
		r["simFrame"] = frame;
		return r;
	}
}

TEST(DomainRootDelta, IdenticalRootsHaveNoChange)
{
	const DomainRootDelta d = ComputeDomainRootDelta(Root(1, 2, 10), Root(1, 2, 10));
	EXPECT_FALSE(d.structureChanged);
	EXPECT_TRUE(d.changed.empty());
}

TEST(DomainRootDelta, SimFrameAloneIsNotAChange)
{
	const DomainRootDelta d = ComputeDomainRootDelta(Root(1, 2, 10), Root(1, 2, 11));
	EXPECT_FALSE(d.structureChanged);
	EXPECT_TRUE(d.changed.empty());
}

TEST(DomainRootDelta, ReportsOnlyChangedDomains)
{
	const DomainRootDelta d = ComputeDomainRootDelta(Root(1, 2, 10), Root(1, 3, 11));
	EXPECT_FALSE(d.structureChanged);
	ASSERT_EQ(d.changed.size(), 1u);
	EXPECT_EQ(d.changed[0], "B");
}

TEST(DomainRootDelta, AddedOrRemovedDomainIsStructural)
{
	Json::Value more = Root(1, 2, 10);
	more["C"]["v"] = 1;
	EXPECT_TRUE(ComputeDomainRootDelta(Root(1, 2, 10), more).structureChanged);
	EXPECT_TRUE(ComputeDomainRootDelta(more, Root(1, 2, 10)).structureChanged);
}

TEST(DomainRootDelta, NonObjectRootsAreStructural)
{
	EXPECT_TRUE(ComputeDomainRootDelta(Json::Value(Json::nullValue), Root(1, 2, 1)).structureChanged);
	EXPECT_TRUE(ComputeDomainRootDelta(Root(1, 2, 1), Json::Value(5)).structureChanged);
}

TEST(DomainRootDelta, BuildPartialRootCarriesOnlyNamedDomainsAndFlag)
{
	const Json::Value next = Root(1, 3, 11);
	std::vector<std::string> names = { "B" };
	const Json::Value p = BuildPartialRoot(next, names);
	EXPECT_TRUE(p["__partial"].asBool());
	EXPECT_EQ(p["B"]["v"].asInt(), 3);
	EXPECT_FALSE(p.isMember("A"));
	EXPECT_FALSE(p.isMember("simFrame"));
}
