#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/SimQueryHostModule.h>
#include <Types/SimAnnounceTracker.h>
#include <Types/SimRegistrationMessage.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <deque>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// SimQueryHostModule announcement sequencing, periodic digest, resync replay and overflow accounting
// (Sim-Hosted Announcement Resync, Task 4). The reply/request streams are replaced by in-memory fakes.

namespace
{
    using Cluiche::AppFlow::SimAnnounceAction;
    using Cluiche::AppFlow::SimAnnounceTracker;
    using Cluiche::AppFlow::SimQueryHostModule;
    using Cluiche::AppFlow::SimRegistration;
    using Cluiche::AppFlow::SimRegistrationOp;
    using Dia::ApplicationFlow::SendResult;
    using Dia::CellLayers::Testing::LogCapture;
    using Dia::Core::StringCRC;
    using Dia::DebugServer::SimKind;

    Json::Value Ok(const Json::Value&)
    {
        Json::Value v;
        v["ok"] = true;
        return v;
    }

    struct Sent
    {
        SimRegistration parsed;
        std::string     json;
    };

    class TestSim : public SimQueryHostModule
    {
    public:
        TestSim() : SimQueryHostModule(StringCRC("TestSimResync")) {}

        // Runs one SimPU frame and returns the messages sent during it (including register calls made
        // since the previous Frame()).
        std::vector<Sent> Frame()
        {
            Dia::SimTime::SimTimeContext ctx{
                Dia::Core::TimeAbsolute::Zero(),
                Dia::Core::TimeRelative::CreateFromSeconds(0.016f),
                0, 1.0f, false
            };
            DoUpdate(ctx);
            std::vector<Sent> out;
            out.swap(sent);
            return out;
        }

        // Messages sent outside a frame (register / unregister calls).
        std::vector<Sent> TakeSent()
        {
            std::vector<Sent> out;
            out.swap(sent);
            return out;
        }

        void PushResyncRequest()
        {
            Dia::DebugServer::SimRequestEvent e;
            std::memset(&e, 0, sizeof(e));
            e.kind = static_cast<uint32_t>(SimKind::Resync);
            requests.push_back(e);
        }

        bool TryPop(Dia::DebugServer::SimRequestEvent& out) override
        {
            if (requests.empty())
                return false;
            out = requests.front();
            requests.pop_front();
            return true;
        }

        std::deque<Dia::DebugServer::SimRequestEvent> requests;
        std::vector<Sent> sent;
        std::deque<SendResult> dropPlan;   // consumed one per send; empty = delivered
        SendResult defaultResult = SendResult::kDelivered;

    protected:
        SendResult SendReply(const Dia::DebugServer::SimReplyEvent& reply) override
        {
            Sent s;
            s.parsed = Cluiche::AppFlow::ParseSimRegistrationReply(reply);
            s.json   = reply.json;
            SendResult r = defaultResult;
            if (!dropPlan.empty())
            {
                r = dropPlan.front();
                dropPlan.pop_front();
            }
            if (r == SendResult::kDelivered)
                sent.push_back(s);
            return r;
        }
    };

    void Reg(TestSim& m, int count, const char* prefix = "r.n")
    {
        for (int i = 0; i < count; ++i)
        {
            const std::string n = std::string(prefix) + std::to_string(i);
            ASSERT_TRUE(i % 2 == 0 ? m.RegisterQuery(n.c_str(), &Ok) : m.RegisterCommand(n.c_str(), &Ok));
        }
    }

    uint32_t SetHash(const std::vector<std::pair<std::string, SimKind>>& names)
    {
        uint32_t h = 0;
        for (const auto& p : names)
            h += Dia::DebugServer::SimNameSetEntryHash(p.first.c_str(), p.second);
        return h;
    }

    // Burns frames until the module sends a digest; returns frames used (0 on failure).
    int FramesUntilDigest(TestSim& m, Sent* digest, int limit = 100)
    {
        for (int i = 1; i <= limit; ++i)
        {
            for (const Sent& s : m.Frame())
            {
                if (s.parsed.op == SimRegistrationOp::Digest)
                {
                    if (digest) *digest = s;
                    return i;
                }
            }
        }
        return 0;
    }
}

TEST(SimQueryHostResync, SeqStartsAtOneAndIncrementsPerAnnounce)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("s.a", &Ok));
    ASSERT_TRUE(m.RegisterCommand("s.b", &Ok));
    m.UnregisterQuery("s.a");
    const std::vector<Sent> sent = m.TakeSent();
    ASSERT_EQ(3u, sent.size());
    EXPECT_EQ(SimRegistrationOp::Register, sent[0].parsed.op);
    EXPECT_EQ(1u, sent[0].parsed.seq);
    EXPECT_EQ(2u, sent[1].parsed.seq);
    EXPECT_EQ(SimRegistrationOp::Unregister, sent[2].parsed.op);
    EXPECT_EQ(3u, sent[2].parsed.seq);
}

TEST(SimQueryHostResync, DigestArrivesAfterIntervalWithSeqCountAndHash)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("d.a", &Ok));
    ASSERT_TRUE(m.RegisterCommand("d.b", &Ok));
    m.TakeSent();

    Sent digest;
    const int frames = FramesUntilDigest(m, &digest);
    EXPECT_EQ(static_cast<int>(SimQueryHostModule::kDigestIntervalFrames), frames);
    EXPECT_EQ(2u, digest.parsed.seq);
    EXPECT_EQ(2u, digest.parsed.count);
    EXPECT_EQ(SetHash({ { "d.a", SimKind::Query }, { "d.b", SimKind::Command } }), digest.parsed.hash);

    // The counter resets after sending: the next digest is another full interval away.
    EXPECT_EQ(static_cast<int>(SimQueryHostModule::kDigestIntervalFrames), FramesUntilDigest(m, nullptr));
}

TEST(SimQueryHostResync, EmptyModuleDigestIsSeqZeroCountZeroHashZero)
{
    TestSim m;
    Sent digest;
    ASSERT_NE(0, FramesUntilDigest(m, &digest));
    EXPECT_EQ(0u, digest.parsed.seq);
    EXPECT_EQ(0u, digest.parsed.count);
    EXPECT_EQ(0u, digest.parsed.hash);
}

TEST(SimQueryHostResync, DigestIsSkippedOnFrameThatAnnouncedAndSentNextQuietFrame)
{
    TestSim m;
    for (unsigned int i = 1; i < SimQueryHostModule::kDigestIntervalFrames; ++i)
    {
        for (const Sent& s : m.Frame())
            ASSERT_NE(SimRegistrationOp::Digest, s.parsed.op) << "frame " << i;
    }
    ASSERT_TRUE(m.RegisterQuery("q.late", &Ok));   // announces just before the digest frame
    std::vector<Sent> frame = m.Frame();
    ASSERT_EQ(1u, frame.size());
    EXPECT_EQ(SimRegistrationOp::Register, frame[0].parsed.op);

    frame = m.Frame();   // next quiet frame: the overdue digest goes out, covering the new name
    ASSERT_EQ(1u, frame.size());
    ASSERT_EQ(SimRegistrationOp::Digest, frame[0].parsed.op);
    EXPECT_EQ(1u, frame[0].parsed.seq);
    EXPECT_EQ(1u, frame[0].parsed.count);
}

TEST(SimQueryHostResync, DigestHashMatchesMainSideComputationAfterUnregister)
{
    TestSim m;
    Reg(m, 6);
    m.UnregisterQuery("r.n2");
    m.TakeSent();

    std::vector<std::pair<std::string, SimKind>> expected;
    for (int i = 0; i < 6; ++i)
        if (i != 2)
            expected.push_back({ "r.n" + std::to_string(i), i % 2 == 0 ? SimKind::Query : SimKind::Command });

    Sent digest;
    ASSERT_NE(0, FramesUntilDigest(m, &digest));
    EXPECT_EQ(7u, digest.parsed.seq);
    EXPECT_EQ(5u, digest.parsed.count);
    EXPECT_EQ(SetHash(expected), digest.parsed.hash);
}

TEST(SimQueryHostResync, ResyncEmitsBeginThenReplaysInOrderCappedPerFrame)
{
    TestSim m;
    Reg(m, 20);
    m.TakeSent();
    m.Frame();   // flush the "announced" frame

    m.PushResyncRequest();
    std::vector<Sent> f1 = m.Frame();
    const size_t cap = SimQueryHostModule::kReplayPerFrame;
    ASSERT_EQ(cap, f1.size());
    ASSERT_EQ(SimRegistrationOp::ResyncBegin, f1[0].parsed.op);
    EXPECT_EQ(20u, f1[0].parsed.seq);
    EXPECT_EQ(20u, f1[0].parsed.count);
    for (size_t i = 1; i < f1.size(); ++i)
    {
        ASSERT_EQ(SimRegistrationOp::Replay, f1[i].parsed.op) << i;
        EXPECT_STREQ(("r.n" + std::to_string(i - 1)).c_str(), f1[i].parsed.name);
        EXPECT_EQ(0u, f1[i].parsed.seq);
        EXPECT_EQ(std::string::npos, f1[i].json.find("seq"));
    }

    std::vector<Sent> f2 = m.Frame();
    ASSERT_EQ(5u, f2.size());   // 20 - 15 remaining
    for (size_t i = 0; i < f2.size(); ++i)
    {
        ASSERT_EQ(SimRegistrationOp::Replay, f2[i].parsed.op);
        EXPECT_STREQ(("r.n" + std::to_string(15 + i)).c_str(), f2[i].parsed.name);
        EXPECT_EQ(0u, f2[i].parsed.seq);
    }
    EXPECT_TRUE(m.Frame().empty());   // replay finished; nothing more until the digest

    // Replays do not bump the seq counter.
    ASSERT_TRUE(m.RegisterQuery("r.after", &Ok));
    EXPECT_EQ(21u, m.TakeSent().at(0).parsed.seq);
}

TEST(SimQueryHostResync, ReplayKindsMatchRegistrationKinds)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("k.q", &Ok));
    ASSERT_TRUE(m.RegisterCommand("k.c", &Ok));
    m.TakeSent();
    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(3u, f.size());
    EXPECT_EQ(SimKind::Query, f[1].parsed.kind);
    EXPECT_EQ(SimKind::Command, f[2].parsed.kind);
}

TEST(SimQueryHostResync, ResyncWithZeroNamesSendsBeginCountZeroAndNoReplays)
{
    TestSim m;
    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(1u, f.size());
    EXPECT_EQ(SimRegistrationOp::ResyncBegin, f[0].parsed.op);
    EXPECT_EQ(0u, f[0].parsed.seq);
    EXPECT_EQ(0u, f[0].parsed.count);
    EXPECT_TRUE(m.Frame().empty());
}

TEST(SimQueryHostResync, MutationDuringReplayRestartsWithFreshBeginAndNewSeq)
{
    TestSim m;
    Reg(m, 20);
    m.TakeSent();
    m.Frame();

    m.PushResyncRequest();
    const std::vector<Sent> f1 = m.Frame();
    ASSERT_EQ(16u, f1.size());

    ASSERT_TRUE(m.RegisterQuery("r.extra", &Ok));   // seq 21, replay still queued
    std::vector<Sent> f2 = m.Frame();
    ASSERT_EQ(17u, f2.size());   // the register + fresh begin + 15 replays
    ASSERT_EQ(SimRegistrationOp::Register, f2[0].parsed.op);
    ASSERT_EQ(SimRegistrationOp::ResyncBegin, f2[1].parsed.op);
    EXPECT_EQ(21u, f2[1].parsed.seq);
    EXPECT_EQ(21u, f2[1].parsed.count);
    EXPECT_STREQ("r.n0", f2[2].parsed.name);   // restarted from the first entry

    // 21 names, 15 replayed so far: 6 remain.
    const std::vector<Sent> f3 = m.Frame();
    ASSERT_EQ(6u, f3.size());
    EXPECT_STREQ("r.extra", f3.back().parsed.name);
}

TEST(SimQueryHostResync, SecondResyncRequestWhileQueuedRestartsReplay)
{
    TestSim m;
    Reg(m, 20);
    m.TakeSent();
    m.Frame();

    m.PushResyncRequest();
    ASSERT_EQ(16u, m.Frame().size());
    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(16u, f.size());
    EXPECT_EQ(SimRegistrationOp::ResyncBegin, f[0].parsed.op);
    EXPECT_STREQ("r.n0", f[1].parsed.name);
}

TEST(SimQueryHostResync, ResyncRequestDoesNotEmitBogusReply)
{
    TestSim m;
    m.PushResyncRequest();
    for (const Sent& s : m.Frame())
        EXPECT_NE(SimRegistrationOp::Malformed, s.parsed.op);
}

TEST(SimQueryHostResync, OverflowCountsEveryNonDeliveredAnnounceAndWarnsOncePerInterval)
{
    TestSim m;
    m.defaultResult = SendResult::kDroppedOldest;
    LogCapture log("DebugServer");

    ASSERT_TRUE(m.RegisterQuery("o.a", &Ok));
    ASSERT_TRUE(m.RegisterQuery("o.b", &Ok));
    ASSERT_TRUE(m.RegisterQuery("o.c", &Ok));
    EXPECT_EQ(3u, m.AnnounceOverflowCount());
    EXPECT_EQ(1u, log.Warnings("announce overflow"));

    for (int i = 0; i < 5; ++i) m.Frame();
    ASSERT_TRUE(m.RegisterQuery("o.d", &Ok));   // still inside the log interval
    EXPECT_EQ(4u, m.AnnounceOverflowCount());
    EXPECT_EQ(1u, log.Warnings("announce overflow"));

    // The digest at frame 30 overflows too, and the interval has now elapsed.
    for (unsigned int i = 5; i < SimQueryHostModule::kDigestIntervalFrames; ++i) m.Frame();
    EXPECT_EQ(5u, m.AnnounceOverflowCount());
    EXPECT_EQ(2u, log.Warnings("announce overflow"));
}

TEST(SimQueryHostResync, DeliveredMessagesDoNotCountAsOverflow)
{
    TestSim m;
    Reg(m, 3);
    m.PushResyncRequest();
    m.Frame();
    EXPECT_EQ(0u, m.AnnounceOverflowCount());
}

TEST(SimQueryHostResync, ReplayAndBeginOverflowAreCounted)
{
    TestSim m;
    Reg(m, 3);
    m.TakeSent();
    m.Frame();
    m.PushResyncRequest();
    m.defaultResult = SendResult::kDroppedNewest;
    m.Frame();
    EXPECT_EQ(4u, m.AnnounceOverflowCount());   // begin + 3 replays
}

// Light end-to-end loop: module output -> SimAnnounceTracker + a tiny table model; a dropped
// announcement is repaired by digest then resync. Full stream integration is covered separately.
TEST(SimQueryHostResync, DroppedAnnouncementIsRepairedByDigestThenResync)
{
    TestSim m;
    SimAnnounceTracker tracker;
    std::set<std::pair<std::string, SimKind>> table;
    uint64_t nowMs = 0;
    unsigned int resyncRequests = 0;

    auto localHash = [&table]() {
        uint32_t h = 0;
        for (const auto& p : table) h += Dia::DebugServer::SimNameSetEntryHash(p.first.c_str(), p.second);
        return h;
    };

    auto deliver = [&](const std::vector<Sent>& msgs) {
        for (const Sent& s : msgs)
        {
            const SimRegistration& r = s.parsed;
            SimAnnounceAction a = SimAnnounceAction::None;
            switch (r.op)
            {
            case SimRegistrationOp::Register:
            case SimRegistrationOp::Unregister:
                a = tracker.OnRegistration(r, nowMs);
                if (r.op == SimRegistrationOp::Register) table.insert({ r.name, r.kind });
                else
                {
                    table.erase({ r.name, SimKind::Query });
                    table.erase({ r.name, SimKind::Command });
                }
                break;
            case SimRegistrationOp::Digest:
                a = tracker.OnDigest(r, static_cast<uint32_t>(table.size()), localHash(), nowMs);
                break;
            case SimRegistrationOp::ResyncBegin:
                a = tracker.OnResyncBegin(r, nowMs);
                break;
            case SimRegistrationOp::Replay:
                a = tracker.OnReplay(r, nowMs);
                break;
            default:
                FAIL() << "unexpected op";
            }
            if (a == SimAnnounceAction::RequestResync || a == SimAnnounceAction::ApplyAndRequestResync)
            {
                ++resyncRequests;
                m.PushResyncRequest();
            }
            if (a == SimAnnounceAction::InstallSet)
            {
                table.clear();
                for (uint32_t i = 0; i < tracker.StagedCount(); ++i)
                    table.insert({ tracker.StagedName(i), tracker.StagedKind(i) });
            }
        }
    };

    // Three names, the last announcement is lost on the wire (no later seq reveals the gap).
    ASSERT_TRUE(m.RegisterQuery("e.a", &Ok));
    ASSERT_TRUE(m.RegisterCommand("e.b", &Ok));
    deliver(m.TakeSent());
    m.dropPlan.push_back(SendResult::kDroppedOldest);
    ASSERT_TRUE(m.RegisterQuery("e.c", &Ok));       // dropped
    deliver(m.TakeSent());
    ASSERT_EQ(2u, table.size());
    EXPECT_EQ(1u, m.AnnounceOverflowCount());

    // Run frames: digest (seq 3, count 3) mismatches Main's view -> resync request -> replay -> install.
    for (int frame = 0; frame < 80 && table.size() != 3; ++frame)
    {
        nowMs += 100;
        deliver(m.Frame());
    }
    EXPECT_EQ(3u, table.size());
    EXPECT_TRUE(table.count({ "e.c", SimKind::Query }));
    EXPECT_GE(resyncRequests, 1u);
    EXPECT_TRUE(tracker.InSync());
    EXPECT_EQ(3u, tracker.LastSeq());
    EXPECT_EQ(1u, tracker.ResyncsCompleted());

    // Once repaired, the following digests match and no further resync is requested.
    const unsigned int before = resyncRequests;
    for (int frame = 0; frame < 70; ++frame)
    {
        nowMs += 100;
        deliver(m.Frame());
    }
    EXPECT_EQ(before, resyncRequests);
}

#endif // DIA_DEBUG
