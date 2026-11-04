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
    const Dia::DebugServer::SimHelpStored* TestHelp()
    {
        static Dia::DebugServer::SimHelpStored sHelp = []
        {
            Dia::DebugServer::SimHelpStored h;
            std::memset(&h, 0, sizeof(h));
            Dia::DebugServer::CopyHelp(Dia::DebugServer::SimHelpInfo{ "test help" }, h);
            return h;
        }();
        return &sHelp;
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
		bool failResyncPost = false;   // Main's PostRequest for a Resync request fails (full stream)
		int resyncPostAttempts = 0;
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
			ASSERT_TRUE(command ? sim.RegisterCommand(name, Dia::DebugServer::SimHelpInfo{"test help"}, &Ok) : sim.RegisterQuery(name, Dia::DebugServer::SimHelpInfo{"test help"}, &Ok)) << name;
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
		{
			++rig.resyncPostAttempts;
			if (rig.failResyncPost)
				return false;
			++rig.resyncPosts;
		}
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
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(SimRegistrationOp::Register, "rogue.name", SimKind::Query, 99, TestHelp(), *msg));
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

// ---- Task 7 edge tests ----

namespace
{
	void Inject(Rig& r, const SimReplyEvent& m, unsigned int sender = 0)
	{
		r.main.HandleSimReply(m, gNowMs, sender != 0 ? sender : r.sim.GetInstanceId().Value());
	}

	bool DropTail(const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.tail") == 0; }
}

// Edge: a full 64-name set (the per-module cap) replays across four Updates and installs.
TEST(SimAnnounceResyncIntegration, ReplayOfSixtyFourNamesCompletes)
{
	Rig r;
	SyncedStart(r, 63);
	r.drop = &DropTail;
	r.Add("it.tail");
	r.drop = nullptr;
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 60));
	EXPECT_EQ(r.MainCount(), 64u);
	for (int i = 0; i < 63; ++i) EXPECT_TRUE(r.Answers(("it.n" + std::to_string(i)).c_str())) << i;
	EXPECT_TRUE(r.Answers("it.tail"));
}

// Edge: Sim holds 0 names while Main holds stale ones; resync_begin count 0 installs an empty table.
TEST(SimAnnounceResyncIntegration, ResyncWhileSimHoldsZeroNamesEmptiesMain)
{
	Rig r;
	SyncedStart(r, 2);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Unregister; };
	r.sim.UnregisterQuery("it.n0");
	r.sim.UnregisterQuery("it.n1");
	r.drop = nullptr;
	ASSERT_EQ(r.MainCount(), 2u);
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 60));
	EXPECT_EQ(r.MainCount(), 0u);
	EXPECT_TRUE(r.Tracker().InSync());
	EXPECT_FALSE(r.Routes("it.n0"));
}

// Edge: unregistering a name the Sim never had announces nothing and consumes no seq.
TEST(SimAnnounceResyncIntegration, UnregisterOfUnknownNameOnSimIsSilent)
{
	Rig r;
	SyncedStart(r, 2);
	const uint32_t seq = r.Tracker().LastSeq();
	r.sim.UnregisterQuery("never.registered");
	r.Step(3);
	EXPECT_EQ(r.Tracker().LastSeq(), seq);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), 0u);
	EXPECT_EQ(r.MainCount(), 2u);
	r.Add("it.next");
	r.Step(2);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), 0u) << "the next register is still consecutive";
	EXPECT_EQ(r.MainCount(), 3u);
}

// Edge: a (garbage) digest arriving while a replay is being staged does not disturb the staging.
TEST(SimAnnounceResyncIntegration, DigestArrivingMidReplayDoesNotDisturbStaging)
{
	Rig r;
	SyncedStart(r, 20);
	r.drop = &DropTail;
	r.Add("it.tail");
	r.drop = nullptr;
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().Staging(); }, 60)) << "21 names need two Updates, so staging is visible between frames";
	std::unique_ptr<SimReplyEvent> d(new SimReplyEvent);
	std::memset(d.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimDigestReply(1, 99, 0xdeadbeefu, *d));
	Inject(r, *d);
	EXPECT_TRUE(r.Tracker().Staging());
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 20));
	EXPECT_EQ(r.MainCount(), 21u);
	EXPECT_TRUE(r.Tracker().InSync());
	EXPECT_TRUE(r.Answers("it.tail"));
}

// Edge: a duplicate resync_begin restarts staging (half-filled set abandoned); the next digest mismatch requests a fresh resync whose begin restarts staging again and completes.
TEST(SimAnnounceResyncIntegration, DuplicateResyncBeginRecoversViaTheNextDigestDrivenResync)
{
	Rig r;
	SyncedStart(r, 20);
	r.drop = &DropTail;
	r.Add("it.tail");
	r.drop = nullptr;
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().Staging(); }, 60));
	std::unique_ptr<SimReplyEvent> b(new SimReplyEvent);
	std::memset(b.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimResyncBeginReply(r.Tracker().LastSeq(), 21, *b));
	Inject(r, *b);
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 150));
	EXPECT_EQ(r.MainCount(), 21u);
	EXPECT_TRUE(r.Answers("it.tail"));
}

// Edge: Main's PostRequest fails permanently. No crash, no spin: retries ride the digest cadence (about 1 per 30 frames).
TEST(SimAnnounceResyncIntegration, PermanentlyFailingResyncPostRetriesAtDigestCadenceAndRecoversWhenItWorks)
{
	Rig r;
	SyncedStart(r, 3);
	r.failResyncPost = true;
	r.drop = &DropTail;
	r.Add("it.tail");
	r.drop = nullptr;
	r.Step(200);
	EXPECT_EQ(r.resyncPosts, 0);
	EXPECT_GE(r.resyncPostAttempts, 2) << "it keeps retrying";
	EXPECT_LE(r.resyncPostAttempts, 12) << "but not every frame";
	EXPECT_EQ(r.MainCount(), 3u);
	EXPECT_EQ(r.Tracker().ResyncsCompleted(), 0u);

	r.failResyncPost = false;
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 60));
	EXPECT_EQ(r.MainCount(), 4u);
}

// Edge: two gaps in the same frame post one request.
TEST(SimAnnounceResyncIntegration, TwoGapsInOneFramePostOneRequest)
{
	Rig r;
	SyncedStart(r, 2);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strncmp(m.name, "it.lost", 7) == 0; };
	r.Add("it.lost1");
	r.Add("it.lost2");
	r.drop = nullptr;
	r.Add("it.a");
	r.Add("it.b");
	r.Add("it.c");
	r.Step(1);
	EXPECT_EQ(r.resyncPosts, 1);
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 20));
	EXPECT_EQ(r.resyncPosts, 1);
	EXPECT_EQ(r.MainCount(), 7u);
}

// AC 1 end to end: a register without seq (not replay) reaches Main as Malformed and changes nothing.
TEST(SimAnnounceResyncIntegration, RegisterWithoutSeqIsIgnoredByMain)
{
	Rig r;
	SyncedStart(r, 2);
	const uint32_t seq = r.Tracker().LastSeq();
	std::unique_ptr<SimReplyEvent> m(new SimReplyEvent);
	std::memset(m.get(), 0, sizeof(SimReplyEvent));
	m->requestId = 0;
	std::strcpy(m->json, "{\"register\":{\"name\":\"noseq.name\",\"kind\":\"query\"}}");
	Inject(r, *m);
	EXPECT_EQ(r.MainCount(), 2u);
	EXPECT_FALSE(r.Routes("noseq.name"));
	EXPECT_EQ(r.Tracker().LastSeq(), seq);
	EXPECT_EQ(r.Tracker().AnnounceGaps(), 0u);
	EXPECT_TRUE(r.Tracker().InSync());
}

// Guard path: foreign resync_begin(0) / replay cannot open staging or wipe the table.
TEST(SimAnnounceResyncIntegration, ForeignResyncBeginAndReplayAreIgnored)
{
	Rig r;
	SyncedStart(r, 3);
	const unsigned int rogue = StringCRC("RogueHost").Value();
	std::unique_ptr<SimReplyEvent> m(new SimReplyEvent);
	std::memset(m.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimResyncBeginReply(5, 0, *m));
	Inject(r, *m, rogue);
	std::memset(m.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimResyncBeginReply(5, 1, *m));
	Inject(r, *m, rogue);
	std::memset(m.get(), 0, sizeof(SimReplyEvent));
	ASSERT_TRUE(Cluiche::AppFlow::BuildSimReplayReply("rogue.replay", SimKind::Query, TestHelp(), *m));
	Inject(r, *m, rogue);
	EXPECT_EQ(r.main.IgnoredForeignAnnounceCount(), 3u);
	EXPECT_FALSE(r.Tracker().Staging());
	EXPECT_EQ(r.Tracker().ResyncsCompleted(), 0u);
	EXPECT_EQ(r.MainCount(), 3u);
	EXPECT_FALSE(r.Routes("rogue.replay"));
}

// AC 14: protocol control words are not client-visible commands; a client asking for them gets "unknown" and nothing reaches Sim.
TEST(SimAnnounceResyncIntegration, ControlWordsAreNotClientCommands)
{
	Rig r;
	SyncedStart(r, 2);
	const int posts = r.clientPosts;
	const size_t sentBefore = r.sent.size();
	for (const char* w : { "resync", "digest", "resync_begin", "replay", "sim.resync" })
		r.Send(950, w);
	EXPECT_EQ(r.clientPosts, posts);
	EXPECT_EQ(r.resyncPosts, 0);
	ASSERT_EQ(r.sent.size(), sentBefore + 5);
	for (size_t i = sentBefore; i < r.sent.size(); ++i)
		EXPECT_FALSE(r.sent[i].msg.command_response().success());
}


// ---- Sim-Hosted Command Help, Task 3: the Main-side catalogue through the real host modules ----

namespace
{
	void AddWithHelp(Rig& r, const char* name, const char* description, bool command = false)
	{
		Dia::DebugServer::SimHelpInfo info{ description };
		info.category = "it.cat";
		ASSERT_TRUE(command ? r.sim.RegisterCommand(name, info, &Ok) : r.sim.RegisterQuery(name, info, &Ok)) << name;
	}

	std::string HelpOf(Rig& r, const char* name)
	{
		const Dia::DebugServer::SimHelpStored* h = r.main.GetServer()->FindSimHostedHelp(StringCRC(name));
		return h != nullptr ? std::string(h->description) : std::string("<none>");
	}
}

// AC 4 end to end: a register announced over the stream lands in the catalogue with its info.
TEST(SimAnnounceResyncIntegration, RegisterWithInfoIsStoredInTheMainCatalogue)
{
	Rig r;
	AddWithHelp(r, "it.one", "first help");
	AddWithHelp(r, "it.two", "second help", true);
	r.Step(3);
	ASSERT_EQ(r.MainCount(), 2u);
	EXPECT_EQ(HelpOf(r, "it.one"), "first help");
	EXPECT_EQ(HelpOf(r, "it.two"), "second help");
	EXPECT_STREQ(r.main.GetServer()->FindSimHostedHelp(StringCRC("it.one"))->category, "it.cat");
	Dia::DebugServer::SimHostedEntryView view;
	ASSERT_TRUE(r.main.GetServer()->GetSimHostedEntry(1, view));
	EXPECT_EQ(view.kind, SimKind::Command);
}

// AC 4 + resync: a lost register is repaired by the resync and its help arrives intact (replay carries info).
TEST(SimAnnounceResyncIntegration, LostRegisterIsRepairedByResyncWithItsHelpIntact)
{
	Rig r;
	SyncedStart(r, 2);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "it.lost") == 0; };
	AddWithHelp(r, "it.lost", "help of the lost one");
	r.drop = nullptr;
	AddWithHelp(r, "it.after", "help after the gap");
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 10));

	EXPECT_EQ(r.MainCount(), 4u);
	EXPECT_EQ(HelpOf(r, "it.lost"), "help of the lost one");
	EXPECT_EQ(HelpOf(r, "it.after"), "help after the gap");
	EXPECT_EQ(HelpOf(r, "it.n0"), "test help");
}

// AC 12 end to end: unregister on the Sim side (stage teardown) removes the help on Main.
TEST(SimAnnounceResyncIntegration, UnregisterOnSimRemovesTheCatalogueEntryOnMain)
{
	Rig r;
	AddWithHelp(r, "it.gone", "going away");
	AddWithHelp(r, "it.stay", "staying");
	r.Step(3);
	ASSERT_EQ(HelpOf(r, "it.gone"), "going away");
	r.sim.UnregisterQuery("it.gone");
	r.Step(3);
	EXPECT_EQ(HelpOf(r, "it.gone"), "<none>");
	EXPECT_EQ(HelpOf(r, "it.stay"), "staying");
	EXPECT_EQ(r.MainCount(), 1u);
}

// AC 12 through the resync path: a lost unregister leaves a stale entry; the resync removes the entry and its help.
TEST(SimAnnounceResyncIntegration, LostUnregisterStaleHelpIsRemovedByResync)
{
	Rig r;
	AddWithHelp(r, "it.stale", "stale help");
	AddWithHelp(r, "it.live", "live help");
	r.Step(3);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Unregister; };
	r.sim.UnregisterQuery("it.stale");
	r.drop = nullptr;
	r.Step(2);
	ASSERT_EQ(HelpOf(r, "it.stale"), "stale help") << "Main still holds the stale entry";
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 60));
	EXPECT_EQ(HelpOf(r, "it.stale"), "<none>");
	EXPECT_EQ(HelpOf(r, "it.live"), "live help");
}

// A re-registration (stage restart without a clean unregister seen by Main) replaces the help.
TEST(SimAnnounceResyncIntegration, ReRegisteredNameReplacesItsHelpOnMain)
{
	Rig r;
	AddWithHelp(r, "it.again", "version one");
	r.Step(3);
	ASSERT_EQ(HelpOf(r, "it.again"), "version one");
	r.sim.UnregisterQuery("it.again");
	AddWithHelp(r, "it.again", "version two");
	r.Step(3);
	EXPECT_EQ(HelpOf(r, "it.again"), "version two");
	EXPECT_EQ(r.MainCount(), 1u);
}


// ---- Exhaustive pass (Task 7) ----

// Help is not part of the set hash, but a help change still has to reach Main: the register carries a seq, so a lost
// unregister/register pair opens a gap and the digest-driven resync replays the NEW help.
TEST(SimAnnounceResyncIntegration, HelpChangedWhileTheAnnouncementsWereLostIsRepairedByResync)
{
	Rig r;
	SyncedStart(r, 2);
	ASSERT_EQ(HelpOf(r, "it.n0"), "test help");
	r.drop = [](const SimRegistration& m) { return std::strcmp(m.name, "it.n0") == 0 && (m.op == SimRegistrationOp::Unregister || m.op == SimRegistrationOp::Register); };
	r.sim.UnregisterQuery("it.n0");
	AddWithHelp(r, "it.n0", "help after the change");   // same name, same kind: the set hash is unchanged
	r.drop = nullptr;
	EXPECT_EQ(HelpOf(r, "it.n0"), "test help") << "Main has not heard of the change yet";
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 80));
	EXPECT_EQ(HelpOf(r, "it.n0"), "help after the change");
	EXPECT_EQ(r.MainCount(), 2u);
}

namespace
{
	void AddRich(Rig& r, const char* name, bool command, const std::vector<Dia::API::ValueParam>& params, const std::string& desc)
	{
		Dia::DebugServer::SimHelpInfo info{ desc.c_str(), "it.cat", "it.owner", params.data(), static_cast<unsigned int>(params.size()), false };
		ASSERT_TRUE(command ? r.sim.RegisterCommand(name, info, &Ok) : r.sim.RegisterQuery(name, info, &Ok)) << name;
	}
}

// Cap check through the whole pipeline: 64 names x 16 params each reach Main, a lost register is repaired by the replay of
// all 64 with their params intact, and a 65th name is refused on the Sim side before Main hears of it.
TEST(SimAnnounceResyncIntegration, SixtyFourNamesWithSixteenParamsEachSurviveRegisterAndResync)
{
	Rig r;
	const std::string pdesc(150, 'p');
	std::vector<Dia::API::ValueParam> params(16);
	std::vector<std::string> pnames;
	for (int i = 0; i < 16; ++i) pnames.push_back("param" + std::to_string(i));
	for (int i = 0; i < 16; ++i)
		params[i] = { pnames[i].c_str(), "enum", i % 2 == 0, pdesc.c_str(), "u", true, -1.0f, static_cast<float>(i), "alpha|beta|gamma" };

	const std::string desc(200, 'd');
	for (int i = 0; i < 63; ++i)
		AddRich(r, ("big.n" + std::to_string(i)).c_str(), i % 2 == 1, params, desc);
	r.Step(3);
	ASSERT_EQ(r.MainCount(), 63u);

	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "big.n63") == 0; };
	AddRich(r, "big.n63", false, params, desc);
	r.drop = nullptr;
	EXPECT_FALSE(r.sim.RegisterQuery("big.n64", Dia::DebugServer::SimHelpInfo{ "one too many" }, &Ok));
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 200));

	ASSERT_EQ(r.MainCount(), 64u);
	for (int i = 0; i < 64; ++i)
	{
		const std::string n = "big.n" + std::to_string(i);
		const Dia::DebugServer::SimHelpStored* h = r.main.GetServer()->FindSimHostedHelp(StringCRC(n.c_str()));
		ASSERT_NE(h, nullptr) << n;
		EXPECT_EQ(h->paramCount, 16u) << n;
		EXPECT_STREQ(h->params[15].name, "param15") << n;
		EXPECT_STREQ(h->params[15].enumValues, "alpha|beta|gamma") << n;
		EXPECT_FLOAT_EQ(h->params[15].max, 15.0f) << n;
		EXPECT_EQ(std::strlen(h->description), 200u) << n;
	}
	EXPECT_EQ(HelpOf(r, "big.n64"), "<none>");
}

// A name registered with no stage-side help change but a resync mid-flight must not lose or mix the help of other names.
TEST(SimAnnounceResyncIntegration, ResyncReplayKeepsEachNamesOwnHelpWhenHelpsDiffer)
{
	Rig r;
	for (int i = 0; i < 6; ++i)
		AddWithHelp(r, ("mixh.n" + std::to_string(i)).c_str(), ("help number " + std::to_string(i)).c_str(), i % 2 == 1);
	r.Step(3);
	r.drop = [](const SimRegistration& m) { return m.op == SimRegistrationOp::Register && std::strcmp(m.name, "mixh.late") == 0; };
	AddWithHelp(r, "mixh.late", "the late one");
	r.drop = nullptr;
	AddWithHelp(r, "mixh.after", "after the gap");
	ASSERT_TRUE(r.StepUntil([&] { return r.Tracker().ResyncsCompleted() > 0; }, 80));
	for (int i = 0; i < 6; ++i)
		EXPECT_EQ(HelpOf(r, ("mixh.n" + std::to_string(i)).c_str()), "help number " + std::to_string(i));
	EXPECT_EQ(HelpOf(r, "mixh.late"), "the late one");
	EXPECT_EQ(HelpOf(r, "mixh.after"), "after the gap");
}

#endif // DIA_DEBUG
