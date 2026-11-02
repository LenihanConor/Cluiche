////////////////////////////////////////////////////////////////////////////////
// Filename: TestDomainStateSnapshot.cpp
// Description: PackDomainStates: whole-domain budget packing of the panel state JSON
//              into a fixed-size DomainStateSnapshot (ACs 3 and 6).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateSnapshot.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>
#include <string>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	// Drawer-only style: no state of its own beyond a toggle.
	// Payload style: `payloadBytes` of filler text in state.
	class FakeDomain : public IDebugDomain
	{
	public:
		FakeDomain(const char* id, const char* display, size_t payloadBytes, const char* group = "Spatial")
			: mId(id), mDisplay(display), mPayload(payloadBytes, 'x'), mGroup(group) {}

		StringCRC GetDomainId() const override { return mId; }
		const char* GetDisplayName() const override { return mDisplay; }
		const char* GetDescription() const override { return "snapshot test domain"; }
		StringCRC GetGroup() const override { return StringCRC(mGroup); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(0x11, 0x22, 0x33, 255); }
		bool HasWorldDrawers() const override { return mPayload.empty(); }
		void OnCommand(StringCRC, const Json::Value&) override {}
		void GetJSONState(Json::Value& out) override
		{
			out["enabled"] = true;
			if (!mPayload.empty())
			{
				out["blob"] = mPayload;
				out["list"] = Json::Value(Json::arrayValue);
				out["list"].append(1);
				out["list"].append("two \"quoted\"");
			}
		}

	private:
		StringCRC mId;
		const char* mDisplay;
		std::string mPayload;
		const char* mGroup;
	};

	Json::Value ParseSnapshot(const DomainStateSnapshot& s)
	{
		Json::Value v;
		Json::Reader reader;
		EXPECT_TRUE(reader.parse(s.json, s.json + s.jsonBytes, v)) << std::string(s.json, s.jsonBytes).substr(0, 200);
		return v;
	}

	struct DomainStateSnapshotTest : public ::testing::Test
	{
		std::unique_ptr<DomainStateSnapshot> snap = std::make_unique<DomainStateSnapshot>();
		DiaDebugDomainRegistry registry;
	};
}

TEST_F(DomainStateSnapshotTest, Golden_MatchesBuildDomainStatesRoot)
{
	FakeDomain drawer("drawer_id", "Drawer Panel", 0, "Rendering");
	FakeDomain medium("medium_id", "Medium Panel", 500, "Physics");
	FakeDomain large("large_id", "Large Panel", 20000, "AIBehavior");
	FakeDomain unknownGroup("odd_id", "Odd Panel", 10, "NotACanonicalGroup");
	registry.Register(drawer);
	registry.Register(medium);
	registry.Register(large);
	registry.Register(unknownGroup);

	Json::Value expected(Json::objectValue);
	BuildDomainStatesRoot(registry, expected, nullptr);

	const PackResult r = PackDomainStates(registry, nullptr, 4242, *snap);

	EXPECT_EQ(ParseSnapshot(*snap), expected);
	EXPECT_EQ(snap->simFrame, 4242u);
	EXPECT_EQ(snap->domainCount, 4u);
	EXPECT_EQ(snap->flags, 0u);
	EXPECT_FALSE(r.truncated);
	EXPECT_EQ(r.droppedCount, 0u);
	EXPECT_EQ(r.includedCount, 4u);
}

TEST_F(DomainStateSnapshotTest, Compact_NoTrailingNewlineOrWhitespaceBetweenTokens)
{
	FakeDomain a("a_id", "A", 5);
	registry.Register(a);
	PackDomainStates(registry, nullptr, 1, *snap);
	EXPECT_EQ(snap->json[0], '{');
	EXPECT_EQ(snap->json[snap->jsonBytes - 1], '}');
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes).find('\n'), std::string::npos);
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes).find(": "), std::string::npos);
}

TEST_F(DomainStateSnapshotTest, EmptyRegistry_EmptyObject)
{
	const PackResult r = PackDomainStates(registry, nullptr, 7, *snap);
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes), "{}");
	EXPECT_EQ(snap->domainCount, 0u);
	EXPECT_EQ(snap->simFrame, 7u);
	EXPECT_EQ(snap->flags, 0u);
	EXPECT_FALSE(r.truncated);
}

TEST_F(DomainStateSnapshotTest, OverBudget_KeepsWholeDomains_FlagsTruncated)
{
	// Each domain is ~40 KB; 128 KB holds three.
	FakeDomain d0("d0", "D0", 40000);
	FakeDomain d1("d1", "D1", 40000);
	FakeDomain d2("d2", "D2", 40000);
	FakeDomain d3("d3", "D3", 40000);
	FakeDomain d4("d4", "D4", 40000);
	registry.Register(d0);
	registry.Register(d1);
	registry.Register(d2);
	registry.Register(d3);
	registry.Register(d4);

	Json::Value full(Json::objectValue);
	BuildDomainStatesRoot(registry, full, nullptr);

	const PackResult r = PackDomainStates(registry, nullptr, 9, *snap);

	EXPECT_TRUE(r.truncated);
	EXPECT_NE(snap->flags & kSnapshotFlagTruncated, 0u);
	EXPECT_EQ(snap->flags & kSnapshotFlagStaleDomainSkipped, 0u);
	EXPECT_LE(snap->jsonBytes, kMaxSnapshotBytes);
	EXPECT_EQ(r.includedCount + r.droppedCount, 5u);
	EXPECT_GT(r.droppedCount, 0u);
	EXPECT_EQ(snap->domainCount, r.includedCount);

	const Json::Value parsed = ParseSnapshot(*snap);
	EXPECT_EQ(parsed.size(), snap->domainCount);
	for (const std::string& name : parsed.getMemberNames())
		EXPECT_EQ(parsed[name], full[name]) << name;   // whole domains, byte-for-byte equal state
}

TEST_F(DomainStateSnapshotTest, SingleDomainLargerThanBudget_DroppedAndFlagged)
{
	FakeDomain huge("huge", "Huge", kMaxSnapshotBytes + 100);
	FakeDomain small("small", "Small", 10);
	registry.Register(huge);
	registry.Register(small);

	const PackResult r = PackDomainStates(registry, nullptr, 3, *snap);

	EXPECT_TRUE(r.truncated);
	EXPECT_EQ(r.droppedCount, 1u);
	EXPECT_EQ(r.includedCount, 1u);
	const Json::Value parsed = ParseSnapshot(*snap);
	EXPECT_TRUE(parsed.isMember("Small"));
	EXPECT_FALSE(parsed.isMember("Huge"));
}

TEST_F(DomainStateSnapshotTest, OnlyDomainLargerThanBudget_EmptyObjectAndFlagged)
{
	FakeDomain huge("huge", "Huge", kMaxSnapshotBytes + 100);
	registry.Register(huge);
	const PackResult r = PackDomainStates(registry, nullptr, 3, *snap);
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes), "{}");
	EXPECT_EQ(snap->domainCount, 0u);
	EXPECT_NE(snap->flags & kSnapshotFlagTruncated, 0u);
	EXPECT_EQ(r.droppedCount, 1u);
}

TEST_F(DomainStateSnapshotTest, ExactlyAtBudget_Fits_OneByteLess_Drops)
{
	FakeDomain a("a_id", "A", 300);
	registry.Register(a);

	// Measure the exact packed size with the full budget.
	PackDomainStates(registry, nullptr, 1, *snap);
	const uint32_t exact = snap->jsonBytes;

	const PackResult fits = PackDomainStates(registry, nullptr, 1, *snap, exact);
	EXPECT_FALSE(fits.truncated);
	EXPECT_EQ(snap->jsonBytes, exact);
	EXPECT_EQ(snap->domainCount, 1u);
	ParseSnapshot(*snap);

	const PackResult drops = PackDomainStates(registry, nullptr, 1, *snap, exact - 1);
	EXPECT_TRUE(drops.truncated);
	EXPECT_EQ(drops.droppedCount, 1u);
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes), "{}");
}

TEST_F(DomainStateSnapshotTest, RepeatedPack_ResetsPreviousContents)
{
	FakeDomain big("big", "Big", 100000);
	FakeDomain big2("big2", "Big2", 100000);
	registry.Register(big);
	registry.Register(big2);
	PackDomainStates(registry, nullptr, 1, *snap);
	EXPECT_NE(snap->flags & kSnapshotFlagTruncated, 0u);

	DiaDebugDomainRegistry empty;
	PackDomainStates(empty, nullptr, 2, *snap);
	EXPECT_EQ(snap->flags, 0u);
	EXPECT_EQ(snap->domainCount, 0u);
	EXPECT_EQ(std::string(snap->json, snap->jsonBytes), "{}");
}

#endif // DIA_DEBUG
