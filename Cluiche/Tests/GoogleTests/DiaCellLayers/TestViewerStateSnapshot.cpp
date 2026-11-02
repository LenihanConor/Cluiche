#include <gtest/gtest.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Streams Migration Task 4: the viewer publishes its panel state to a FrameStream slot while demanded; the Main-side adapter reads it.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	std::string SnapCompact(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	// Counting fake domain: GetJSONState writes {"a":1,"padding":"xxx..."}.
	class SnapDomain : public Dia::VisualDebugger::IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("fake.domain"); }
		const char* GetDisplayName() const override { return "Fake"; }
		const char* GetDescription() const override { return "Fake domain"; }
		StringCRC GetGroup() const override { return StringCRC("Spatial"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(1, 2, 3, 255); }
		void GetJSONState(Json::Value& out) override
		{
			++stateCalls;
			out["a"] = 1;
			if (paddingBytes > 0) { out["padding"] = std::string(paddingBytes, 'x'); }
		}
		void OnCommand(StringCRC cmd, const Json::Value&) override { lastCommand = cmd; }
		uint32_t stateCalls = 0;
		uint32_t paddingBytes = 0;
		StringCRC lastCommand;
	};

	class SnapSink : public IViewerStateSink
	{
	public:
		void Write(const char* json, uint32_t length) override { writes.emplace_back(json, length); }
		std::vector<std::string> writes;
	};

	class SnapSource : public IViewerStateSource
	{
	public:
		bool ReadLatest(std::string& json) override { if (!has) { return false; } json = text; return true; }
		bool has = false;
		std::string text;
	};

	class SnapDemand : public IViewerStateDemand
	{
	public:
		void RequestState() override { ++requests; }
		uint32_t requests = 0;
	};

	Json::Value SnapParse(const std::string& text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		EXPECT_TRUE(r->parse(text.data(), text.data() + text.size(), &v, &errs)) << errs;
		return v;
	}
}

// ---- (a) publisher: armed / not armed / expiry ----

TEST(ViewerStateSnapshotTest, NotArmedDoesNotPublishOrCallGetJSONState)
{
	SnapDomain domain;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	for (uint64_t f = 1; f <= 100; ++f) { pub.Update(f); }
	EXPECT_EQ(domain.stateCalls, 0u);
	EXPECT_TRUE(sink.writes.empty());
	EXPECT_EQ(pub.PublishCount(), 0u);
}

TEST(ViewerStateSnapshotTest, ArmedPublishesGetJSONStatePlusSimFrame)
{
	SnapDomain domain;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(10);
	pub.Update(11);
	ASSERT_EQ(sink.writes.size(), 1u);
	EXPECT_EQ(domain.stateCalls, 1u);

	Json::Value expected(Json::objectValue);
	domain.GetJSONState(expected);	// the live output for the same state
	expected["simFrame"] = Json::UInt64(11);
	EXPECT_EQ(SnapCompact(SnapParse(sink.writes[0])), SnapCompact(expected));
	EXPECT_EQ(sink.writes[0].find('\n'), std::string::npos) << "compact writer: no newlines/indentation";
	EXPECT_EQ(SnapParse(sink.writes[0])["simFrame"].asUInt64(), 11u);
}

TEST(ViewerStateSnapshotTest, PublishesEveryFrameWhileArmedThenStopsAfterWindow)
{
	SnapDomain domain;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink, /*armFrames*/ 60);
	pub.Arm(100);
	for (uint64_t f = 101; f <= 160; ++f) { pub.Update(f); }	// frames 101..160 are within 60 frames of the arm
	EXPECT_EQ(sink.writes.size(), 60u);
	pub.Update(161);
	pub.Update(162);
	EXPECT_EQ(sink.writes.size(), 60u) << "window of 60 frames (2 s at 30 Hz) expired";
	EXPECT_EQ(domain.stateCalls, 60u);
	EXPECT_FALSE(pub.IsArmed());
}

TEST(ViewerStateSnapshotTest, ReArmingExtendsTheWindow)
{
	SnapDomain domain;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink, 60);
	pub.Arm(0);
	for (uint64_t f = 1; f <= 50; ++f) { pub.Update(f); }
	pub.Arm(50);
	for (uint64_t f = 51; f <= 110; ++f) { pub.Update(f); }
	EXPECT_EQ(sink.writes.size(), 110u);
	pub.Update(111);
	EXPECT_EQ(sink.writes.size(), 110u);
}

// ---- (b) oversize ----

TEST(ViewerStateSnapshotTest, OversizeBecomesErrorMarkerAndWarnsOnce)
{
	LogCapture log("CEL");
	SnapDomain domain;
	domain.paddingBytes = 5000;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink, 60, /*capacity*/ 1024);
	pub.Arm(1);
	pub.Update(2);
	pub.Update(3);
	pub.Update(4);
	ASSERT_EQ(sink.writes.size(), 3u);
	const Json::Value v = SnapParse(sink.writes[2]);
	EXPECT_EQ(v["error"].asString(), "snapshot too large");
	EXPECT_EQ(v["simFrame"].asUInt64(), 4u);
	EXPECT_LE(sink.writes[2].size(), 1024u);
	EXPECT_EQ(log.Warnings("snapshot"), 1u) << "one warning, not one per frame";
}

TEST(ViewerStateSnapshotTest, ExactlyAtCapacityIsStoredAndOneOverIsNot)
{
	SnapDomain probe;
	Json::Value base(Json::objectValue);
	probe.GetJSONState(base);
	base["simFrame"] = Json::UInt64(5);
	const uint32_t baseBytes = static_cast<uint32_t>(SnapCompact(base).size());

	SnapDomain domain;
	domain.paddingBytes = 100;
	// size with padding 100 = baseBytes + len(',"padding":"') + 100 + len('"') ; measure it instead of computing.
	Json::Value measured(Json::objectValue);
	domain.GetJSONState(measured);
	measured["simFrame"] = Json::UInt64(5);
	const uint32_t full = static_cast<uint32_t>(SnapCompact(measured).size());
	ASSERT_GT(full, baseBytes);

	{
		SnapSink sink;
		ViewerStateSnapshotPublisher pub(domain, sink, 60, full);
		pub.Arm(4);
		pub.Update(5);
		ASSERT_EQ(sink.writes.size(), 1u);
		EXPECT_EQ(SnapParse(sink.writes[0]).isMember("error"), false);
	}
	{
		SnapSink sink;
		ViewerStateSnapshotPublisher pub(domain, sink, 60, full - 1);
		pub.Arm(4);
		pub.Update(5);
		ASSERT_EQ(sink.writes.size(), 1u);
		EXPECT_EQ(SnapParse(sink.writes[0])["error"].asString(), "snapshot too large");
	}
}

// ---- (c) Main adapter ----

TEST(ViewerStateAdapterTest, PendingMarkerBeforeAnySnapshotAndDemandSignalledEveryCall)
{
	SnapDomain live;
	SnapSource source;
	SnapDemand demand;
	ViewerStateAdapter adapter(live, source, demand);

	Json::Value out(Json::objectValue);
	adapter.GetJSONState(out);
	EXPECT_TRUE(out["pending"].asBool());
	EXPECT_TRUE(out["drawers"].isArray()) << "panel contract: drawers + stats are always present";
	EXPECT_TRUE(out["stats"].isObject());
	EXPECT_EQ(demand.requests, 1u);

	Json::Value out2(Json::objectValue);
	adapter.GetJSONState(out2);
	EXPECT_EQ(demand.requests, 2u);
	EXPECT_EQ(live.stateCalls, 0u) << "the adapter never calls the live domain's GetJSONState";
}

TEST(ViewerStateAdapterTest, ReturnsLatestSnapshot)
{
	SnapDomain live;
	SnapSource source;
	SnapDemand demand;
	ViewerStateAdapter adapter(live, source, demand);
	source.has = true;
	source.text = "{\"a\":7,\"simFrame\":42}";
	Json::Value out(Json::objectValue);
	adapter.GetJSONState(out);
	EXPECT_EQ(out["a"].asInt(), 7);
	EXPECT_EQ(out["simFrame"].asUInt64(), 42u);
	EXPECT_FALSE(out.isMember("pending"));
	source.text = "{\"a\":8,\"simFrame\":43}";
	Json::Value out2(Json::objectValue);
	adapter.GetJSONState(out2);
	EXPECT_EQ(out2["simFrame"].asUInt64(), 43u);
	EXPECT_EQ(demand.requests, 2u);
}

TEST(ViewerStateAdapterTest, GarbageSnapshotYieldsErrorNotACrash)
{
	SnapDomain live;
	SnapSource source;
	SnapDemand demand;
	ViewerStateAdapter adapter(live, source, demand);
	source.has = true;
	source.text = "{not json";
	Json::Value out(Json::objectValue);
	adapter.GetJSONState(out);
	EXPECT_TRUE(out.isMember("error"));
	EXPECT_TRUE(out["drawers"].isArray());
}

TEST(ViewerStateAdapterTest, IdentityIsCopiedAndCommandsForwardToTheDelegate)
{
	SnapSource source;
	SnapDemand demand;
	std::unique_ptr<ViewerStateAdapter> adapter;
	SnapDomain target;
	{
		SnapDomain live;
		adapter.reset(new ViewerStateAdapter(live, source, demand,
			ViewerStateAdapter::CommandFn(&target, &SnapDomain::OnCommand)));
	}	// live is gone: the adapter must not read it afterwards
	EXPECT_EQ(adapter->GetDomainId(), StringCRC("fake.domain"));
	EXPECT_STREQ(adapter->GetDisplayName(), "Fake");
	EXPECT_STREQ(adapter->GetDescription(), "Fake domain");
	EXPECT_EQ(adapter->GetGroup(), StringCRC("Spatial"));
	EXPECT_FALSE(adapter->HasWorldDrawers());
	adapter->OnCommand(StringCRC("base"), Json::Value(Json::objectValue));
	EXPECT_EQ(target.lastCommand, StringCRC("base"));
}

TEST(ViewerStateAdapterTest, NoCommandDelegateIsIgnored)
{
	SnapDomain live;
	SnapSource source;
	SnapDemand demand;
	ViewerStateAdapter adapter(live, source, demand);
	adapter.OnCommand(StringCRC("base"), Json::Value(Json::objectValue));	// must not crash
	SUCCEED();
}

// ---- (d) slot size with the real viewer ----

TEST(ViewerStateSnapshotTest, SlotIsThirtyTwoKilobytes)
{
	EXPECT_EQ(ViewerStateSlot::kCapacity, 32u * 1024u);
	EXPECT_GE(sizeof(ViewerStateSlot), 32u * 1024u);
}

TEST(ViewerStateSnapshotTest, RealViewerTwentyLayersFitsTheSlotAndMatchesLiveOutput)
{
	TestGrid grid{ 20, 20 };
	Dia::Graphics::CellFieldRegistry cellField;
	UploaderSchemaFixture fx;
	std::vector<std::unique_ptr<StubProvider>> provs;
	std::vector<std::string> names;
	for (int i = 0; i < 20; ++i)
	{
		names.push_back("layer" + std::to_string(i));
		fx.Add(UploaderSchemaFixture::FloatEntry(names.back().c_str(), 0, 100));
	}
	const Dia::ValueSchema::ValueSchemaService* service = &fx.Publish();
	FakeDebugDraw draw;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, service };
	for (int i = 0; i < 20; ++i)
	{
		provs.emplace_back(new StubProvider());
		provs.back()->values.resize(400);
		for (size_t c = 0; c < 400; ++c) { provs.back()->values[c] = static_cast<float>((c * (i + 3)) % 97) + 0.37f; }
		CellLayerDesc d = provs.back()->MakeDesc(names[i].c_str(), &grid.desc, CellValueKind::Float, ("cells/" + names[i]).c_str());
		ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
	}
	domain.Update(draw);
	CellSelection* sel = domain.ActiveSelection();
	ASSERT_NE(sel, nullptr);
	sel->BeginEdit();
	sel->Set(7, true);	// one cell: the inspector adds its per-cell table (the largest state in Task 1)
	sel->EndEdit();
	domain.Update(draw);

	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(1);
	pub.Update(2);
	ASSERT_EQ(sink.writes.size(), 1u);
	std::cout << "[ INFO ] 20-layer one-cell snapshot: " << sink.writes[0].size() << " bytes (slot " << ViewerStateSlot::kCapacity << ")\n";
	const Json::Value v = SnapParse(sink.writes[0]);
	EXPECT_FALSE(v.isMember("error")) << "worst realistic state must fit the 32 KB slot";
	EXPECT_LE(sink.writes[0].size(), ViewerStateSlot::kCapacity);

	Json::Value live(Json::objectValue);
	domain.GetJSONState(live);
	live["simFrame"] = Json::UInt64(2);
	EXPECT_EQ(SnapCompact(v), SnapCompact(live)) << "snapshot == previous live GetJSONState output + simFrame";
}

TEST(ViewerStateSnapshotTest, ThirtyKilobyteStateFitsAndSlotWriteIsLosslessThroughAdapter)
{
	SnapDomain domain;
	domain.paddingBytes = 30 * 1024;
	SnapSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(0);
	pub.Update(1);
	ASSERT_EQ(sink.writes.size(), 1u);
	EXPECT_GT(sink.writes[0].size(), 16u * 1024u);
	EXPECT_LE(sink.writes[0].size(), ViewerStateSlot::kCapacity);
	EXPECT_FALSE(SnapParse(sink.writes[0]).isMember("error"));

	SnapSource source;
	SnapDemand demand;
	source.has = true;
	source.text = sink.writes[0];
	ViewerStateAdapter adapter(domain, source, demand);
	Json::Value out(Json::objectValue);
	adapter.GetJSONState(out);
	EXPECT_EQ(out["padding"].asString().size(), 30u * 1024u);
}

#endif // DIA_DEBUG
