#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <Modules/DomainStateBridgeModule.h>
#include <Modules/DomainStateRelayModule.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// DomainStateBridgeModule (Main) + DomainStateRelayModule (Sim): a live two-PU Application ticked inline, joined by the manifest's
// real per-slot FrameStream / EventStream / ServiceStream. Domains are tiny fakes that stamp a marker into their state.

using namespace Dia::ApplicationFlow;
using Cluiche::AppFlow::DomainStateBridgeModule;
using Cluiche::AppFlow::DomainStateChannel;
using Cluiche::AppFlow::DomainStateRelayModule;
using Dia::Core::StringCRC;
using Dia::VisualDebugger::IDebugDomain;

namespace
{
	class FakeDomain : public IDebugDomain
	{
	public:
		FakeDomain(const char* name, const char* marker) : mName(name), mMarker(marker) {}
		StringCRC GetDomainId() const override { return StringCRC(mName.c_str()); }
		const char* GetDisplayName() const override { return mName.c_str(); }
		const char* GetDescription() const override { return "fake"; }
		StringCRC GetGroup() const override { return StringCRC("Test"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value& out) override
		{
			++stateCalls;
			out = Json::Value(Json::objectValue);
			out["marker"] = mMarker;
			out["drawers"] = Json::Value(Json::arrayValue);
			out["stats"] = Json::Value(Json::objectValue);
		}
		void OnCommand(StringCRC, const Json::Value&) override { ++commands; }
		int stateCalls = 0;
		int commands = 0;
	private:
		std::string mName;
		std::string mMarker;
	};

	DomainStateRelayModule* gRelay = nullptr;
	DomainStateBridgeModule* gBridge = nullptr;
	Module* MakeRelay(const StringCRC& id) { gRelay = new DomainStateRelayModule(id); return gRelay; }
	Module* MakeBridge(const StringCRC& id) { gBridge = new DomainStateBridgeModule(id); return gBridge; }

	ModuleDeclaration Mod(const StringCRC& typeId, const char* instance)
	{
		ModuleDeclaration m;
		m.instanceId = StringCRC(instance);
		m.typeId = typeId;
		m.startTimeoutMs = 10000.0f;
		m.stopTimeoutMs = 5000.0f;
		m.stages.Add(StringCRC("Boot"));
		return m;
	}

	void AddStream(ApplicationManifestV3& m, const char* id, const char* kind, const char* from, const char* to, unsigned capacity = 0)
	{
		StreamDeclaration s;
		s.id = StringCRC(id);
		s.kind = StringCRC(kind);
		s.fromPU = StringCRC(from);
		s.toPU = StringCRC(to);
		s.capacity = capacity;
		m.streams.Add(s);
	}

	void Bind(ModuleDeclaration& m, const char* id, const char* role)
	{
		ChannelBinding b;
		b.id = StringCRC(id);
		b.role = StringCRC(role);
		m.channels.Add(b);
	}

	struct Harness
	{
		static constexpr float kDt = 0.0334f;	// just over one 30 Hz sim step: one sim tick per Update
		TypeRegistry reg;
		ApplicationManifestV3 manifest;
		std::unique_ptr<Application> app;

		Harness()
		{
			gRelay = nullptr; gBridge = nullptr;
			reg.Register(DomainStateRelayModule::kTypeId, MakeRelay);
			reg.Register(DomainStateBridgeModule::kTypeId, MakeBridge);

			manifest.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			manifest.stages.Add(boot);
			manifest.initialStage = StringCRC("Boot");

			ModuleDeclaration bridge = Mod(DomainStateBridgeModule::kTypeId, "bridge");
			ModuleDeclaration relay = Mod(DomainStateRelayModule::kTypeId, "relay");
			for (uint32_t i = 0; i < DomainStateBridgeModule::kSlotCount; ++i)
			{
				AddStream(manifest, DomainStateBridgeModule::kSnapshotStreamIds[i], "FrameStream", "SimPU", "MainPU");
				AddStream(manifest, DomainStateBridgeModule::kDemandStreamIds[i], "EventStream", "MainPU", "SimPU", 8);
				AddStream(manifest, DomainStateBridgeModule::kChannelStreamIds[i], "ServiceStream", "MainPU", "SimPU");
				Bind(bridge, DomainStateBridgeModule::kSnapshotStreamIds[i], "reads");
				Bind(bridge, DomainStateBridgeModule::kDemandStreamIds[i], "writes");
				Bind(bridge, DomainStateBridgeModule::kChannelStreamIds[i], "provides");
				Bind(relay, DomainStateBridgeModule::kSnapshotStreamIds[i], "writes");
				Bind(relay, DomainStateBridgeModule::kDemandStreamIds[i], "reads");
				Bind(relay, DomainStateBridgeModule::kChannelStreamIds[i], "consumes");
			}

			ProcessingUnitDeclaration mainPu;
			mainPu.instanceId = StringCRC("MainPU");
			mainPu.frequencyHz = 30.0f;
			mainPu.dedicatedThread = false;
			mainPu.modules.Add(bridge);
			manifest.processingUnits.Add(mainPu);

			ProcessingUnitDeclaration simPu;
			simPu.instanceId = StringCRC("SimPU");
			simPu.frequencyHz = 30.0f;
			simPu.dedicatedThread = false;
			simPu.modules.Add(relay);
			manifest.processingUnits.Add(simPu);

			app.reset(new Application(manifest, reg));
		}

		bool Start()
		{
			if (!app->Start()) return false;
			for (int i = 0; i < 20; ++i) app->Update(kDt);
			return gRelay && gBridge;
		}
		void Pump(int n) { for (int i = 0; i < n; ++i) app->Update(kDt); }
		void Shutdown()
		{
			if (!app) return;
			app->RequestShutdown();
			for (int guard = 0; guard < 1000 && app->Update(kDt); ++guard) {}
			app.reset();
		}
		~Harness() { Shutdown(); }
	};

	// Reads the adapter's state (this also records demand) and returns the marker, or "" while no snapshot has arrived.
	std::string ReadMarker(IDebugDomain& adapter)
	{
		Json::Value out;
		adapter.GetJSONState(out);
		return out.isMember("marker") ? out["marker"].asString() : std::string();
	}

	// Demand a snapshot through `adapter` and pump until it arrives (or give up).
	std::string FetchMarker(Harness& h, IDebugDomain& adapter)
	{
		for (int i = 0; i < 40; ++i)
		{
			const std::string m = ReadMarker(adapter);
			if (!m.empty()) return m;
			h.Pump(1);
		}
		return std::string();
	}
}

TEST(DomainStateModules, TwoAttachedDomainsGetDistinctAdaptersAndSnapshotsDoNotCross)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain a("DomA", "marker-A"), b("DomB", "marker-B");
	IDebugDomain* adA = gRelay->Attach(a);
	IDebugDomain* adB = gRelay->Attach(b);
	ASSERT_NE(adA, nullptr);
	ASSERT_NE(adB, nullptr);
	EXPECT_NE(adA, adB);
	EXPECT_EQ(gRelay->AttachedCount(), 2u);

	EXPECT_EQ(FetchMarker(h, *adA), "marker-A");
	EXPECT_EQ(FetchMarker(h, *adB), "marker-B");
	// Keep demanding on both and confirm neither adapter ever shows the other domain's state.
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_EQ(ReadMarker(*adA), "marker-A");
		EXPECT_EQ(ReadMarker(*adB), "marker-B");
		h.Pump(1);
	}
}

TEST(DomainStateModules, FifthAttachReturnsNullptr)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain d0("D0", "0"), d1("D1", "1"), d2("D2", "2"), d3("D3", "3"), d4("D4", "4");
	EXPECT_NE(gRelay->Attach(d0), nullptr);
	EXPECT_NE(gRelay->Attach(d1), nullptr);
	EXPECT_NE(gRelay->Attach(d2), nullptr);
	EXPECT_NE(gRelay->Attach(d3), nullptr);
	EXPECT_EQ(gRelay->Attach(d4), nullptr);
	EXPECT_EQ(gRelay->AttachedCount(), 4u);
	EXPECT_FALSE(gRelay->IsAttached(d4));
}

TEST(DomainStateModules, DetachFreesTheSlotAndStopsPublishing)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain d0("D0", "0"), d1("D1", "1"), d2("D2", "2"), d3("D3", "3"), d4("D4", "4");
	IDebugDomain* ad0 = gRelay->Attach(d0);
	ASSERT_NE(ad0, nullptr);
	gRelay->Attach(d1); gRelay->Attach(d2); gRelay->Attach(d3);
	ASSERT_EQ(gRelay->Attach(d4), nullptr);

	ASSERT_EQ(FetchMarker(h, *ad0), "0");
	EXPECT_GT(gRelay->PublishCount(d0), 0u);

	gRelay->Detach(d0);
	EXPECT_FALSE(gRelay->IsAttached(d0));
	EXPECT_EQ(gRelay->PublishCount(d0), 0u);
	const int callsAfterDetach = d0.stateCalls;
	h.Pump(20);
	EXPECT_EQ(d0.stateCalls, callsAfterDetach) << "a detached domain must not be polled by the relay";

	EXPECT_NE(gRelay->Attach(d4), nullptr) << "the freed slot must be reusable";
	gRelay->Detach(d0);	// double detach is a no-op
	EXPECT_EQ(gRelay->AttachedCount(), 4u);
}

TEST(DomainStateModules, AttachReturnsNullptrWhenChannelsAreUnavailable)
{
	DomainStateRelayModule relay(StringCRC("standaloneRelay"));	// never connected to an Application: no channel is available
	FakeDomain d("D", "x");
	EXPECT_EQ(relay.Attach(d), nullptr);
	EXPECT_EQ(relay.AttachedCount(), 0u);
	EXPECT_FALSE(relay.IsAttached(d));
}

TEST(DomainStateModules, DemandFromMainArmsPublishingAndNoDemandPublishesNothing)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain a("DomA", "marker-A");
	IDebugDomain* adA = gRelay->Attach(a);
	ASSERT_NE(adA, nullptr);

	h.Pump(30);
	EXPECT_EQ(gRelay->PublishCount(a), 0u) << "nothing publishes without demand";
	EXPECT_EQ(a.stateCalls, 0);

	EXPECT_EQ(FetchMarker(h, *adA), "marker-A");
	EXPECT_GT(gRelay->PublishCount(a), 0u);
	EXPECT_GT(a.stateCalls, 0);
}

TEST(DomainStateModules, AttachingTheSameDomainTwiceReturnsTheExistingAdapter)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain a("DomA", "marker-A");
	IDebugDomain* first = gRelay->Attach(a);
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(gRelay->Attach(a), first);
	EXPECT_EQ(gRelay->AttachedCount(), 1u);
}

TEST(DomainStateModules, DetachWithDemandInFlightIsSafeAndSlotIsReusable)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	FakeDomain a("DomA", "marker-A"), b("DomB", "marker-B");
	IDebugDomain* adA = gRelay->Attach(a);
	ASSERT_NE(adA, nullptr);
	ASSERT_EQ(FetchMarker(h, *adA), "marker-A");

	ReadMarker(*adA);	// Main-side poll raises a demand event that the relay has not drained yet
	gRelay->Detach(a);	// ... and the domain detaches before the next Sim tick
	const int calls = a.stateCalls;
	h.Pump(20);
	EXPECT_EQ(a.stateCalls, calls) << "the in-flight demand must not reach a detached domain";
	EXPECT_EQ(gRelay->AttachedCount(), 0u);

	IDebugDomain* adB = gRelay->Attach(b);
	ASSERT_NE(adB, nullptr);
	// KNOWN LIMITATION (reported in plan task 6): the reused slot's FrameStream still holds the previous occupant's last
	// snapshot, so the new adapter can show it until the first fresh publish. It must converge to the new domain's state.
	std::string marker;
	for (int i = 0; i < 40 && marker != "marker-B"; ++i)
	{
		marker = ReadMarker(*adB);
		h.Pump(1);
	}
	EXPECT_EQ(marker, "marker-B") << "the slot must converge to the new occupant's state";
}

TEST(DomainStateModules, TenAttachDetachRoundTripsDoNotLeakSlots)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	for (int i = 0; i < 10; ++i)
	{
		FakeDomain d("D", "round");
		IDebugDomain* ad = gRelay->Attach(d);
		ASSERT_NE(ad, nullptr) << "round " << i;
		EXPECT_EQ(FetchMarker(h, *ad), "round") << "round " << i;
		gRelay->Detach(d);
		EXPECT_EQ(gRelay->AttachedCount(), 0u) << "round " << i;
	}
	// All four slots are still free.
	FakeDomain d0("D0", "0"), d1("D1", "1"), d2("D2", "2"), d3("D3", "3");
	EXPECT_NE(gRelay->Attach(d0), nullptr);
	EXPECT_NE(gRelay->Attach(d1), nullptr);
	EXPECT_NE(gRelay->Attach(d2), nullptr);
	EXPECT_NE(gRelay->Attach(d3), nullptr);
}

// DomainStateChannel::RequestState runs on the Sim thread (the snapshot publisher packs the registered ViewerStateAdapter) while
// TakeDemand runs on Main. The flag is atomic, so a request made on one thread is seen exactly once on the other.
TEST(DomainStateChannel, DemandHandedAcrossThreadsIsNeverLostOrDuplicated)
{
	DomainStateChannel channel;
	constexpr int kRequests = 20000;
	std::atomic<int> requested{ 0 };
	std::atomic<bool> done{ false };
	int taken = 0;

	std::thread sim([&] {
		for (int i = 0; i < kRequests; ++i)
		{
			channel.RequestState();
			requested.fetch_add(1);
			while (requested.load() - taken > 1000000) {}   // never blocks in practice
		}
		done.store(true);
	});
	while (!done.load())
		if (channel.TakeDemand()) ++taken;
	sim.join();
	if (channel.TakeDemand()) ++taken;

	EXPECT_GE(taken, 1);
	EXPECT_LE(taken, kRequests) << "a request must not be reported twice";
	EXPECT_FALSE(channel.TakeDemand()) << "the flag is cleared by TakeDemand";
	channel.RequestState();
	EXPECT_TRUE(channel.TakeDemand());
	EXPECT_FALSE(channel.TakeDemand());
}

#endif // DIA_DEBUG
