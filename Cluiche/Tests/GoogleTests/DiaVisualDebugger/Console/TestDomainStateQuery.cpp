////////////////////////////////////////////////////////////////////////////////
// Filename: TestDomainStateQuery.cpp
// Description: The `debug.domain_state` DebugServer query: one domain's panel state by id,
//              or the whole BuildDomainStatesRoot object when no id is given.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateQuery.h>
#include <DiaVisualDebugger/Console/DomainStateSnapshot.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <memory>

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
		std::unique_ptr<DomainStateSnapshot> snapshot{new DomainStateSnapshot()};	// ~128 KB: heap
		Json::Value root{Json::objectValue};	// what Main holds: the parsed snapshot

		void SetUp() override
		{
			registry.Register(alpha);
			registry.Register(beta);
			Pack(11);
		}

		void Pack(uint64_t simFrame)
		{
			PackDomainStates(registry, nullptr, simFrame, *snapshot);
			ASSERT_TRUE(ParseDomainStateSnapshot(*snapshot, root));
		}
	};
}

TEST_F(Fixture, KnownId_ReturnsThatDomainsState)
{
	Json::Value args(Json::objectValue);
	args["id"] = "beta_id";
	const Json::Value r = QueryDomainState(&root, args);
	EXPECT_EQ(r["_id"].asString(), "beta_id");
	EXPECT_EQ(r["_displayName"].asString(), "Beta Panel");
	EXPECT_EQ(r["value"].asInt(), 2);
	EXPECT_FALSE(r.isMember("error"));
}

TEST_F(Fixture, KnownId_MatchesBuildDomainStatesRootEntry)
{
	Json::Value direct(Json::objectValue);
	BuildDomainStatesRoot(registry, direct, nullptr);

	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	EXPECT_EQ(QueryDomainState(&root, args), direct["Alpha Panel"]);
}

TEST_F(Fixture, Query_NeverCallsTheDomains)
{
	const int alphaCalls = alpha.getCalls;
	const int betaCalls = beta.getCalls;
	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	QueryDomainState(&root, args);
	QueryDomainState(&root, Json::Value(Json::objectValue));
	EXPECT_EQ(alpha.getCalls, alphaCalls);
	EXPECT_EQ(beta.getCalls, betaCalls);
}

TEST_F(Fixture, UnknownId_ReturnsError)
{
	Json::Value args(Json::objectValue);
	args["id"] = "nope";
	const Json::Value r = QueryDomainState(&root, args);
	ASSERT_TRUE(r.isMember("error"));
	EXPECT_NE(r["error"].asString().find("nope"), std::string::npos);
}

TEST_F(Fixture, NonStringId_ReturnsError)
{
	Json::Value args(Json::objectValue);
	args["id"] = 5;
	EXPECT_TRUE(QueryDomainState(&root, args).isMember("error"));
}

TEST_F(Fixture, NoId_ReturnsWholeRootKeyedByDisplayNameWithSimFrame)
{
	Json::Value direct(Json::objectValue);
	BuildDomainStatesRoot(registry, direct, nullptr);
	direct["simFrame"] = Json::Value(Json::UInt64(11));

	const Json::Value r = QueryDomainState(&root, Json::Value(Json::objectValue));
	EXPECT_EQ(r, direct);
	EXPECT_TRUE(r.isMember("Alpha Panel"));
	EXPECT_TRUE(r.isMember("Beta Panel"));
	EXPECT_EQ(r["simFrame"].asUInt64(), 11u);
}

TEST_F(Fixture, NullArgs_ReturnsWholeRoot)
{
	const Json::Value r = QueryDomainState(&root, Json::Value());
	EXPECT_EQ(r.getMemberNames().size(), 3u);	// two domains + simFrame
}

TEST_F(Fixture, SimFrameKeyIsNotADomainId)
{
	Json::Value args(Json::objectValue);
	args["id"] = "simFrame";
	EXPECT_TRUE(QueryDomainState(&root, args).isMember("error"));
}

TEST_F(Fixture, NoSnapshotYet_ReturnsError)
{
	const Json::Value r = QueryDomainState(nullptr, Json::Value(Json::objectValue));
	ASSERT_TRUE(r.isMember("error"));
	EXPECT_EQ(r["error"].asString(), "no domain snapshot yet");

	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	EXPECT_EQ(QueryDomainState(nullptr, args)["error"].asString(), "no domain snapshot yet");
}

TEST_F(Fixture, ParseCarriesSimFrameAndRepeatedPackUpdatesIt)
{
	EXPECT_EQ(root["simFrame"].asUInt64(), 11u);
	Pack(12);
	EXPECT_EQ(root["simFrame"].asUInt64(), 12u);
}

TEST_F(Fixture, ParseFailure_ReturnsFalseAndKeepsLastGoodRoot)
{
	DomainStateSnapshot bad;
	bad.simFrame = 99;
	bad.jsonBytes = 5;
	memcpy(bad.json, "{oops", 5);
	EXPECT_FALSE(ParseDomainStateSnapshot(bad, root));
	EXPECT_EQ(root["simFrame"].asUInt64(), 11u);
	EXPECT_TRUE(root.isMember("Alpha Panel"));
}

TEST_F(Fixture, ParseRejectsBytesBeyondTheBufferAndNonObjectJson)
{
	DomainStateSnapshot bad;
	bad.jsonBytes = kMaxSnapshotBytes + 1;
	EXPECT_FALSE(ParseDomainStateSnapshot(bad, root));

	bad.jsonBytes = 2;
	memcpy(bad.json, "[]", 2);
	EXPECT_FALSE(ParseDomainStateSnapshot(bad, root));
}

TEST_F(Fixture, DuplicateDisplayNamesInSnapshotJson_ParserKeepsTheLast)
{
	DomainStateSnapshot dup;
	dup.simFrame = 1;
	const char* text = "{\"Same\":{\"v\":1,\"_id\":\"a\"},\"Same\":{\"v\":2,\"_id\":\"b\"}}";
	dup.jsonBytes = static_cast<uint32_t>(strlen(text));
	memcpy(dup.json, text, dup.jsonBytes);
	Json::Value parsed(Json::objectValue);
	ASSERT_TRUE(ParseDomainStateSnapshot(dup, parsed));
	EXPECT_EQ(parsed["Same"]["v"].asInt(), 2);	// matches BuildDomainStatesRoot overwriting
}

namespace
{
	struct FakeResolver : public ISchemaKeyResolver
	{
		bool available = true;
		bool IsAvailable() const override { return available; }
		bool Resolves(StringCRC) const override { return true; }
		unsigned int Version() const override { return 7; }
	};
}

TEST(ApplySchemaVersionTest, StampsOnlyDomainsWithControlsAndNoVersionYet)
{
	Json::Value root(Json::objectValue);
	root["WithControls"]["controls"] = Json::Value(Json::arrayValue);
	root["Versioned"]["controls"] = Json::Value(Json::arrayValue);
	root["Versioned"]["schemaVersion"] = 3;
	root["NoControls"]["value"] = 1;
	root["simFrame"] = 5;

	FakeResolver resolver;
	ApplySchemaVersion(root, &resolver);

	EXPECT_EQ(root["WithControls"]["schemaVersion"].asUInt(), 7u);
	EXPECT_EQ(root["Versioned"]["schemaVersion"].asInt(), 3);
	EXPECT_FALSE(root["NoControls"].isMember("schemaVersion"));
	EXPECT_EQ(root["simFrame"].asInt(), 5);
}

TEST(ApplySchemaVersionTest, NullOrUnavailableResolverChangesNothing)
{
	Json::Value root(Json::objectValue);
	root["D"]["controls"] = Json::Value(Json::arrayValue);
	ApplySchemaVersion(root, nullptr);
	EXPECT_FALSE(root["D"].isMember("schemaVersion"));

	FakeResolver resolver;
	resolver.available = false;
	ApplySchemaVersion(root, &resolver);
	EXPECT_FALSE(root["D"].isMember("schemaVersion"));
}

TEST_F(Fixture, RegisteredOnQueryRegistry_RoundTrips)
{
	Dia::DebugServer::QueryRegistry queries;
	queries.Register(StringCRC(kDomainStateQueryName),
		[this](const Json::Value& a) { return QueryDomainState(&root, a); });

	Json::Value args(Json::objectValue);
	args["id"] = "alpha_id";
	const Json::Value r = queries.Execute(StringCRC(kDomainStateQueryName), args);
	EXPECT_EQ(r["value"].asInt(), 1);
}

#endif // DIA_DEBUG
