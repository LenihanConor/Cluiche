#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <Modules/DebugServerHostModule.h>
#include <Modules/SimQueryHostModule.h>
#include <Types/SimAnnounceTracker.h>
#include <Types/SimRegistrationMessage.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Sim-Hosted Announcement Resync, Task 5: a real Sim-side SimQueryHostModule and a real Main-side DebugServerHostModule
// (announcement handling + DebugServer) joined by two in-memory queues that stand in for the SimQueryReply / SimQueryRequest
// EventStreams so individual messages can be dropped deterministically. The Main clock is injected (34 ms per frame), so the
// 500 ms / 1000 ms rules need no sleeping. The real-EventStream half is the churn test in TestCellLayersIntegrationStreams.cpp.

namespace Dia
{
	namespace DebugServer
	{
		// Friend of DebugServer. Must stay token-identical to the definition in TestSimHostedDispatch.cpp (ODR).
		struct DebugServerTestAccess
		{
			static void HandleCommand(DebugServer& s, int connId, const dia::debug::DebugMessage& m) { s.HandleCommand(connId, m); }
			static void HandleConnection(DebugServer& s, int connId, bool connected) { s.HandleConnection(connId, connected); }
			static void Sweep(DebugServer& s) { s.SweepSimTimeouts(); }
		};
	}
}

namespace
{
	using Cluiche::AppFlow::DebugServerHostModule;
	using Cluiche::AppFlow::SimAnnounceTracker;
	using Cluiche::AppFlow::SimQueryHostModule;
	using Cluiche::AppFlow::SimRegistration;
	using Cluiche::AppFlow::SimRegistrationOp;
	using Dia::ApplicationFlow::SendResult;
	using Dia::Core::StringCRC;
	using Dia::DebugServer::DebugServerTestAccess;
	using Dia::DebugServer::SimKind;
	using Dia::DebugServer::SimReplyEvent;
	using Dia::DebugServer::SimRequestEvent;

	uint64_t gNowMs = 0;
	uint64_t FakeNow() { return gNowMs; }

	struct Sent { int connId; dia::debug::DebugMessage msg; };
	void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg) { static_cast<std::vector<Sent>*>(ctx)->push_back(Sent{ connId, msg }); }

	Json::Value Ok(const Json::Value&)
	{
		Json::Value v;
		v["ok"] = true;
		return v;
	}

	struct Rig;

	class SimSide : public SimQueryHostModule
	{
	public:
		explicit SimSide(Rig& r) : SimQueryHostModule(StringCRC("ResyncItSim")), rig(r) {}

		void Frame()
		{
			Dia::SimTime::SimTimeContext ctx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::CreateFromSeconds(0.016f), 0, 1.0f, false };
			DoUpdate(ctx);
		}

		void StartForTest() { DoStart(); }

		bool TryPop(SimRequestEvent& out) override;
		SendResult SendReply(const SimReplyEvent& reply) override;
		Rig& rig;
	};

	class MainSide : public DebugServerHostModule
	{
	public:
		explicit MainSide(Rig& r) : DebugServerHostModule(StringCRC("ResyncItMain")), rig(r) {}
		using DebugServerHostModule::HandleSimReply;
		using DebugServerHostModule::TickAnnounce;
		using DebugServerHostModule::RegisterAnnounceMetrics;
		using DebugServerHostModule::PublishAnnounceMetrics;
		using DebugServerHostModule::IgnoredForeignAnnounceCount;
		using DebugServerHostModule::GetAnnounceTracker;
		bool PostRequest(const SimRequestEvent& r) override;
		Rig& rig;
	};

	// The request path the DebugServer itself uses for a client command that names a sim-hosted entry.
	class ClientSink : public Dia::DebugServer::ISimRequestSink
	{
	public:
		explicit ClientSink(Rig& r) : rig(r) {}
		bool PostRequest(const SimRequestEvent& r) override;
		Rig& rig;
	};

	struct Rig
	{
		Rig() : sim(*this), main(*this), sink(*this)
		{
			gNowMs = 10000;
			Dia::DebugServer::DebugServer& s = *main.GetServer();
			s.SetNowMsFunction(&FakeNow);
			s.SetSendHook(&CaptureSend, &sent);
			s.SetSimRequestSink(&sink);
		}

		SimSide sim;
		MainSide main;
		ClientSink sink;
		std::vector<Sent> sent;

		std::deque<std::unique_ptr<SimReplyEvent>> toMain;     // Sim -> Main (SimQueryReply)
		std::deque<std::unique_ptr<SimRequestEvent>> toSim;    // Main -> Sim (SimQueryRequest)

		std::function<bool(const SimRegistration&)> drop;       // true: the Sim->Main announce message is lost
		std::function<void(const SimRegistration&)> onDelivered; // after Main handled an announce message
		int resyncPosts = 0;       // control (Resync) requests Main posted
		int clientPosts = 0;       // client requests routed to Sim
		int digestsDelivered = 0;
		int dropped = 0;

		void Step(int n = 1)
		{
			for (int i = 0; i < n; ++i)
			{
				sim.Frame();
				while (!toMain.empty())
				{
					std::unique_ptr<SimReplyEvent> m = std::move(toMain.front());
					toMain.pop_front();
					const SimRegistration reg = Cluiche::AppFlow::ParseSimRegistrationReply(*m);
					if (reg.op == SimRegistrationOp::Digest) ++digestsDelivered;
					main.HandleSimReply(*m, gNowMs, sim.GetInstanceId().Value());
					if (onDelivered && reg.op != SimRegistrationOp::NotRegistration) onDelivered(reg);
				}
				main.TickAnnounce(gNowMs);
				main.PublishAnnounceMetrics();
				gNowMs += 34;
			}
		}

		template <typename Pred>
		bool StepUntil(Pred p, int limit)
		{
			for (int i = 0; i < limit; ++i) { if (p()) return true; Step(); }
			return p();
		}

		void Add(const char* name, bool command = false)
		{
			ASSERT_TRUE(command ? sim.RegisterCommand(name, &Ok) : sim.RegisterQuery(name, &Ok)) << name;
		}

		void Send(int conn, const char* name)
		{
			dia::debug::DebugMessage m;
			m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
			m.mutable_command_request()->set_command(name);
			DebugServerTestAccess::HandleCommand(*main.GetServer(), conn, m);
		}

		// True if a client request for `name` is routed to Sim (not answered "unknown" by Main).
		bool Routes(const char* name)
		{
			const int before = clientPosts;
			Send(900, name);
			return clientPosts > before;
		}

		// Full round trip: request, step both sides, true if answered with success. False for unknown or error.
		bool Answers(const char* name)
		{
			const size_t before = sent.size();
			Send(901, name);
			for (int i = 0; i < 8 && sent.size() == before; ++i) Step();
			return sent.size() > before && sent[before].msg.command_response().success();
		}

		uint32_t MainCount()
		{
			uint32_t c = 0, h = 0;
			main.GetServer()->GetSimHostedDigest(c, h);
			return c;
		}

		const SimAnnounceTracker& Tracker() const { return main.GetAnnounceTracker(); }
	};

	bool SimSide::TryPop(SimRequestEvent& out)
	{
		if (rig.toSim.empty())
			return false;
		out = *rig.toSim.front();
		rig.toSim.pop_front();
		return true;
	}

	SendResult SimSide::SendReply(const SimReplyEvent& reply)
	{
		const SimRegistration reg = Cluiche::AppFlow::ParseSimRegistrationReply(reply);
		if (reg.op != SimRegistrationOp::NotRegistration && rig.drop && rig.drop(reg))
		{
			++rig.dropped;
			return SendResult::kDroppedOldest;
		}
		rig.toMain.push_back(std::unique_ptr<SimReplyEvent>(new SimReplyEvent(reply)));
		return SendResult::kDelivered;
	}

	bool MainSide::PostRequest(const SimRequestEvent& r)
	{
		if (r.kind == static_cast<uint32_t>(SimKind::Resync))
			++rig.resyncPosts;
		rig.toSim.push_back(std::unique_ptr<SimRequestEvent>(new SimRequestEvent(r)));
		return true;
	}

	bool ClientSink::PostRequest(const SimRequestEvent& r)
	{
		++rig.clientPosts;
		rig.toSim.push_back(std::unique_ptr<SimRequestEvent>(new SimRequestEvent(r)));
		return true;
	}

	// Registers the common starting set and steps until Main is in sync with it.
	void SyncedStart(Rig& r, int count)
	{
		for (int i = 0; i < count; ++i) r.Add(("it.n" + std::to_string(i)).c_str(), i % 2 == 1);
		r.Step(3);
		ASSERT_EQ(r.MainCount(), static_cast<uint32_t>(count));
		ASSERT_TRUE(r.Tracker().InSync());
	}
}

// AC 3: the last announcement of a burst is lost; nothing else follows, so only the digest can notice.
TEST(SimAnnounceResyncIntegration, TailLossIsRepairedByTheDigestWithinAboutThirtySimFrames)
{
	Rig r;
	SyncedStart(r, 4);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.tail") == 0; };
	r.Add("it.tail");
	r.Step(3);
	EXPECT_EQ(r.dropped, 1);
	EXPECT_GE(r.sim.AnnounceOverflowCount(), 1u);
	EXPECT_FALSE(r.Routes("it.tail")) << "Main never heard about it";
	EXPECT_EQ(r.Tracker().AnnounceGaps(), 0u) << "tail loss leaves no gap to see";
	EXPECT_EQ(r.resyncPosts, 0);

	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 45)) << "digest + resync must land within ~30 Sim frames plus the replay";
	EXPECT_GE(r.Tracker().DigestMismatches(), 1u);
	EXPECT_EQ(r.resyncPosts, 1);
	EXPECT_TRUE(r.Tracker().InSync());
	EXPECT_EQ(r.MainCount(), 5u);
	EXPECT_TRUE(r.Answers("it.tail"));
}

// AC 8: a register in the middle of a burst is lost; the following seq shows the gap at once and resync restores the name.
TEST(SimAnnounceResyncIntegration, LostRegisterIsRoutableAfterResync)
{
	Rig r;
	SyncedStart(r, 2);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.lost") == 0; };
	r.Add("it.lost");
	r.drop = nullptr;
	r.Add("it.after");
	r.Step(1);
	EXPECT_GE(r.Tracker().AnnounceGaps(), 1u);
	EXPECT_EQ(r.resyncPosts, 1) << "a gap requests a resync immediately, no waiting for the digest";

	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 10));
	EXPECT_EQ(r.MainCount(), 4u);
	EXPECT_TRUE(r.Answers("it.lost"));
	EXPECT_TRUE(r.Answers("it.after"));
	EXPECT_TRUE(r.Answers("it.n0"));
}

// AC 8: an unregister is lost; Main keeps routing a name Sim no longer hosts until the resync removes it.
TEST(SimAnnounceResyncIntegration, LostUnregisterIsGoneAfterResyncAndARequestIsThenUnknown)
{
	Rig r;
	SyncedStart(r, 4);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Unregister; };
	r.sim.UnregisterQuery("it.n0");
	r.drop = nullptr;
	r.Step(2);
	EXPECT_EQ(r.dropped, 1);
	EXPECT_EQ(r.MainCount(), 4u) << "Main still holds the stale name";
	EXPECT_TRUE(r.Routes("it.n0"));

	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 60));
	EXPECT_EQ(r.MainCount(), 3u);
	const int postsBefore = r.clientPosts;
	EXPECT_FALSE(r.Answers("it.n0"));
	EXPECT_EQ(r.clientPosts, postsBefore) << "after the resync the name is unknown at Main and never reaches Sim";
	EXPECT_TRUE(r.Answers("it.n1"));
}

// AC 7: one replay message is lost; the staged set never completes, is discarded after 1000 ms and a later digest mismatch resyncs again.
TEST(SimAnnounceResyncIntegration, LostReplayLeavesStagingIncompleteThenTimesOutAndAFreshResyncCompletes)
{
	Rig r;
	SyncedStart(r, 4);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.lost") == 0; };
	r.Add("it.lost");
	// Lose the first replay of the first resync only.
	bool dropOneReplay = true;
	r.drop = [&](const SimRegistration& m)
	{
		if (m.op == SimRegistrationOp::Replay && dropOneReplay) { dropOneReplay = false; return true; }
		return false;
	};
	r.Add("it.after");	// seq gap -> immediate resync request

	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().Staging(); }, 10));
	r.Step(2);
	EXPECT_TRUE(r.Tracker().Staging()) << "a replay is missing, staging cannot complete";
	EXPECT_EQ(r.Tracker().ResyncsCompleted(), 0u);
	EXPECT_EQ(r.MainCount(), 5u) << "live table untouched while staging (4 original + it.after applied)";

	gNowMs += 1100;
	r.main.TickAnnounce(gNowMs);
	EXPECT_FALSE(r.Tracker().Staging());
	EXPECT_EQ(r.Tracker().ResyncTimeouts(), 1u);
	EXPECT_EQ(r.Tracker().ResyncsCompleted(), 0u);

	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 80)) << "the next digest mismatch must start a fresh, complete resync";
	EXPECT_GE(r.Tracker().DigestMismatches(), 1u);
	EXPECT_EQ(r.MainCount(), 6u);
	EXPECT_TRUE(r.Answers("it.lost"));
	EXPECT_TRUE(r.Answers("it.after"));
}

// AC 6: while the replay is in flight Main keeps routing known names, and the live table swaps only when the last replay lands.
TEST(SimAnnounceResyncIntegration, NoRoutingHoleBetweenResyncBeginAndTheLastReplay)
{
	Rig r;
	SyncedStart(r, 20);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.late") == 0; };
	r.Add("it.late");
	r.drop = nullptr;

	// 21 names + begin = 22 messages at 16 per Sim frame: the replay spans two frames. Deliver per message and probe in between.
	int begins = 0, replays = 0;
	bool allRouted = true, tableUnchangedUntilLast = true;
	r.onDelivered = [&](const SimRegistration& m)
	{
		if (m.op == SimRegistrationOp::ResyncBegin) ++begins;
		else if (m.op == SimRegistrationOp::Replay) ++replays;
		else return;
		if (replays < 21)
		{
			allRouted = allRouted && r.Routes("it.n0") && r.Routes("it.n19");
			tableUnchangedUntilLast = tableUnchangedUntilLast && r.MainCount() == 21u;
		}
	};
	r.Add("it.after");	// gap -> resync request
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 20));
	r.onDelivered = nullptr;

	EXPECT_EQ(begins, 1);
	EXPECT_EQ(replays, 22) << "21 hosted before + it.after";
	EXPECT_TRUE(allRouted) << "known names must stay routable while staging";
	EXPECT_TRUE(tableUnchangedUntilLast) << "no partial table is ever visible; the swap is atomic at the end";
	EXPECT_EQ(r.MainCount(), 22u);
	EXPECT_TRUE(r.Answers("it.late"));
}

// AC 4 at integration level: once in sync, a long quiet run exchanges digests but never requests a resync or reports a fault.
TEST(SimAnnounceResyncIntegration, MatchingDigestsStayQuietForManyFrames)
{
	Rig r;
	SyncedStart(r, 10);
	r.Step(400);
	EXPECT_GE(r.digestsDelivered, 10) << "digests must actually flow for this test to mean anything";
	EXPECT_EQ(r.resyncPosts, 0);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), 0u);
	EXPECT_EQ(r.Tracker().DigestMismatches(), 0u);
	EXPECT_EQ(r.Tracker().ResyncsRequested(), 0u);
	EXPECT_TRUE(r.Tracker().InSync());
	EXPECT_EQ(r.sim.AnnounceOverflowCount(), 0u);
}

// After a resync the Sim's seq keeps counting from where it was and Main's tracker continues from the resync's seq: no spurious gap.
TEST(SimAnnounceResyncIntegration, RegisterAfterAResyncContinuesTheSequenceWithNoGap)
{
	Rig r;
	SyncedStart(r, 3);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.lost") == 0; };
	r.Add("it.lost");
	r.drop = nullptr;
	r.Add("it.next");	// gap -> resync
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 10));
	r.Step(2);

	const uint32_t gaps = r.Tracker().AnnounceGaps();
	const uint32_t seq = r.Tracker().LastSeq();
	const uint32_t requested = r.Tracker().ResyncsRequested();
	r.Add("it.fresh");
	r.Step(2);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), gaps);
	EXPECT_EQ(r.Tracker().LastSeq(), seq + 1u);
	EXPECT_EQ(r.Tracker().ResyncsRequested(), requested);
	EXPECT_TRUE(r.Tracker().InSync());
	EXPECT_EQ(r.MainCount(), 6u);
	EXPECT_TRUE(r.Answers("it.fresh"));
}

// AC 13: the six metrics are registered with info and track the tracker / Sim counters.
TEST(SimAnnounceResyncIntegration, MetricsMirrorTheCountersAndCarryInfo)
{
	using Dia::Observation::Metric::MetricRegistry;
	MetricRegistry& reg = MetricRegistry::Instance();
	Rig r;
	r.main.RegisterAnnounceMetrics();
	r.sim.StartForTest();

	static const char* const kNames[] = {
		"dia.debugserver.sim.announce_gaps", "dia.debugserver.sim.digest_mismatches", "dia.debugserver.sim.resyncs_requested",
		"dia.debugserver.sim.resyncs_completed", "dia.debugserver.sim.resync_timeouts", "dia.debugserver.sim.announce_overflow" };
	uint64_t base[6];
	for (int i = 0; i < 6; ++i)
	{
		ASSERT_NE(reg.FindCounter(StringCRC(kNames[i])), nullptr) << kNames[i];
		const Dia::Observation::Metric::MetricInfo* info = reg.GetInfo(StringCRC(kNames[i]));
		ASSERT_NE(info, nullptr) << kNames[i];
		EXPECT_NE(info->label, nullptr);
		EXPECT_NE(info->description, nullptr);
		EXPECT_NE(info->unit, nullptr);
		base[i] = reg.FindCounter(StringCRC(kNames[i]))->Value();
	}

	SyncedStart(r, 3);
	// Mid-burst loss -> gap + resync; tail loss -> digest mismatch + resync.
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.lost") == 0; };
	r.Add("it.lost");
	r.drop = nullptr;
	r.Add("it.after");
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() >= 1; }, 20));
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.tail") == 0; };
	r.Add("it.tail");
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() >= 2; }, 60));
	r.Step(2);

	const uint32_t expected[5] = { r.Tracker().AnnounceGaps(), r.Tracker().DigestMismatches(), r.Tracker().ResyncsRequested(),
		r.Tracker().ResyncsCompleted(), r.Tracker().ResyncTimeouts() };
	EXPECT_GE(expected[0], 1u);
	EXPECT_GE(expected[1], 1u);
	for (int i = 0; i < 5; ++i)
		EXPECT_EQ(reg.FindCounter(StringCRC(kNames[i]))->Value() - base[i], expected[i]) << kNames[i];
	EXPECT_GE(r.sim.AnnounceOverflowCount(), 2u);
	EXPECT_EQ(reg.FindCounter(StringCRC(kNames[5]))->Value() - base[5], r.sim.AnnounceOverflowCount());
}

// AC 12: a second SimQueryHostModule is ignored (Error logged); the first host's table and sequence are untouched.
TEST(SimAnnounceResyncIntegration, SecondSimHostAnnouncementsAreIgnoredAndTheFirstHostIsUnaffected)
{
	Rig r;
	SyncedStart(r, 3);
	const uint32_t seq = r.Tracker().LastSeq();
	const uint32_t gaps = r.Tracker().AnnounceGaps();
	ASSERT_EQ(r.main.IgnoredForeignAnnounceCount(), 0u);

	const unsigned int rogue = StringCRC("SecondSimHost").Value();
	std::unique_ptr<SimReplyEvent> msg(new SimReplyEvent);
	std::memset(msg.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(SimRegistrationOp::Register, "rogue.name", SimKind::Query, 99, *msg));
	r.main.HandleSimReply(*msg, gNowMs, rogue);
	std::memset(msg.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimDigestReply(7, 1, 0x1234u, *msg));
	r.main.HandleSimReply(*msg, gNowMs, rogue);

	EXPECT_EQ(r.main.IgnoredForeignAnnounceCount(), 2u);
	EXPECT_EQ(r.MainCount(), 3u);
	EXPECT_FALSE(r.Routes("rogue.name"));
	EXPECT_EQ(r.Tracker().LastSeq(), seq);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), gaps);
	EXPECT_EQ(r.Tracker().DigestMismatches(), 0u);

	r.Add("it.more");
	r.Step(2);
	EXPECT_EQ(r.MainCount(), 4u);
	EXPECT_TRUE(r.Answers("it.more"));
	EXPECT_EQ(r.Tracker().AnnounceGaps(), gaps);
}

#endif // DIA_DEBUG
