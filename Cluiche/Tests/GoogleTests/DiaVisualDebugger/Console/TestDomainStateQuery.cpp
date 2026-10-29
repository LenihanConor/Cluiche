////////////////////////////////////////////////////////////////////////////////
// Filename: TestDomainStateQuery.cpp
// Description: The `debug.domain_state` DebugServer query: one domain's panel state by id,
//              or the whole BuildDomainStatesRoot object when no id is given.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateQuery.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	class QueryDomain : public IDebugDomain
	{
	public:
		QueryDomain(const char* id, const char* display, int value)
			: mId(id), mDisplay(display), mValue(value) {}

		StringCRC GetDomainId() const override { return mId; }
		const char* GetDisplayName() const override { return mDisplay; }
		const char* GetDescription() const override { return "query test domain"; }
		StringCRC GetGroup() const override { return StringCRC("Spatial"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(0x10, 0x20, 0x30, 255); }
		bool HasWorldDrawers() const override { return false; }
		void OnCommand(StringCRC, const Json::Value&) override {}
		void GetJSONState(Json::Value& out) override { out["value"] = mValue; ++getCalls; }

		int getCalls = 0;

	private:
		StringCRC mId;
		const char* mDisplay;
		int mValue;
	};

	struct Fixture : public ::testing::Test
	{
		QueryDomain alpha{"alpha_id", "Alpha Panel", 1};
		QueryDomain beta{"beta_id", "Beta Panel", 2};
		DiaDebugDomainRegistry registry;

		void SetUp() override
		{
			registry.Register(alpha);
			registry.Register(beta);
		}
	};
}

TEST_F(Fixture, KnownId_ReturnsThatDomainsState)
{
	Json::Value args(Json::objectValue);
	args["id"] = "beta_id";
	const Json::Value r = QueryDomainState(registry, nullptr, args);
	EXPECT_EQ(r["_id"].asString(), "beta_id");
	EXPECT_EQ(r["_displayName"].asString(), "Beta Panel");
	EXPECT_EQ(r["value"].asInt(), 2);
	EXPECT_FALSE(r.isMember("error"));
}

TEST_F(Fixture, KnownId_MatchesBuildDomainStatesRootEntry)
{
	Json::Value root(Json::objectValue);
	BuildDomainStatesRoot(registry, root, nullptr);

	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	EXPECT_EQ(QueryDomainState(registry, nullptr, args), root["Alpha Panel"]);
}

TEST_F(Fixture, KnownId_OnlyQueriesThatDomain)
{
	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	QueryDomainState(registry, nullptr, args);
	EXPECT_EQ(alpha.getCalls, 1);
	EXPECT_EQ(beta.getCalls, 0);
}

TEST_F(Fixture, UnknownId_ReturnsErrorAndQueriesNothing)
{
	Json::Value args(Json::objectValue);
	args["id"] = "nope";
	const Json::Value r = QueryDomainState(registry, nullptr, args);
	ASSERT_TRUE(r.isMember("error"));
	EXPECT_NE(r["error"].asString().find("nope"), std::string::npos);
	EXPECT_EQ(alpha.getCalls, 0);
	EXPECT_EQ(beta.getCalls, 0);
}

TEST_F(Fixture, NonStringId_ReturnsError)
{
	Json::Value args(Json::objectValue);
	args["id"] = 5;
	EXPECT_TRUE(QueryDomainState(registry, nullptr, args).isMember("error"));
}

TEST_F(Fixture, NoId_ReturnsWholeRootKeyedByDisplayName)
{
	Json::Value root(Json::objectValue);
	BuildDomainStatesRoot(registry, root, nullptr);

	const Json::Value r = QueryDomainState(registry, nullptr, Json::Value(Json::objectValue));
	EXPECT_EQ(r, root);
	EXPECT_TRUE(r.isMember("Alpha Panel"));
	EXPECT_TRUE(r.isMember("Beta Panel"));
}

TEST_F(Fixture, NullArgs_ReturnsWholeRoot)
{
	const Json::Value r = QueryDomainState(registry, nullptr, Json::Value());
	EXPECT_EQ(r.getMemberNames().size(), 2u);
}

TEST_F(Fixture, RegisteredOnQueryRegistry_RoundTrips)
{
	Dia::DebugServer::QueryRegistry queries;
	queries.Register(StringCRC(kDomainStateQueryName),
		[this](const Json::Value& a) { return QueryDomainState(registry, nullptr, a); });

	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	const Json::Value r = queries.Execute(StringCRC(kDomainStateQueryName), args);
	EXPECT_EQ(r["value"].asInt(), 1);
}

#endif // DIA_DEBUG
