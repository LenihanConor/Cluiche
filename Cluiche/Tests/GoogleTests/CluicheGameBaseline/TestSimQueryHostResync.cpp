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
        std::shared_ptr<Dia::DebugServer::SimHelpStored> info;   // decoded help, when the message carried it
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
            std::shared_ptr<Dia::DebugServer::SimHelpStored> info(new Dia::DebugServer::SimHelpStored);
            std::memset(info.get(), 0, sizeof(Dia::DebugServer::SimHelpStored));
            s.parsed = Cluiche::AppFlow::ParseSimRegistrationReply(reply, info.get());
            if (s.parsed.hasInfo)
                s.info = info;
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
            ASSERT_TRUE(i % 2 == 0 ? m.RegisterQuery(n.c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok) : m.RegisterCommand(n.c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
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
    ASSERT_TRUE(m.RegisterQuery("s.a", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterCommand("s.b", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
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
    ASSERT_TRUE(m.RegisterQuery("d.a", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterCommand("d.b", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
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
    ASSERT_TRUE(m.RegisterQuery("q.late", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));   // announces just before the digest frame
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
    ASSERT_TRUE(m.RegisterQuery("r.after", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_EQ(21u, m.TakeSent().at(0).parsed.seq);
}

TEST(SimQueryHostResync, ReplayKindsMatchRegistrationKinds)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("k.q", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterCommand("k.c", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
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

    ASSERT_TRUE(m.RegisterQuery("r.extra", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));   // seq 21, replay still queued
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

    ASSERT_TRUE(m.RegisterQuery("o.a", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterQuery("o.b", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterQuery("o.c", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_EQ(3u, m.AnnounceOverflowCount());
    EXPECT_EQ(1u, log.Warnings("announce overflow"));

    for (int i = 0; i < 5; ++i) m.Frame();
    ASSERT_TRUE(m.RegisterQuery("o.d", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));   // still inside the log interval
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
    ASSERT_TRUE(m.RegisterQuery("e.a", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    ASSERT_TRUE(m.RegisterCommand("e.b", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    deliver(m.TakeSent());
    m.dropPlan.push_back(SendResult::kDroppedOldest);
    ASSERT_TRUE(m.RegisterQuery("e.c", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));       // dropped
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

// ---- Sim-Hosted Command Help, Task 2: help info on register / replay ----

namespace
{
    const Dia::API::ValueParam kHelpParams[] = {
        { "cell", "int", true, "Cell index", "m", true, -5.5f, 12.25f, nullptr },
        { "mode", "enum", false, "Which mode", nullptr, false, 0.f, 0.f, "weather|ecology" },
    };

    Dia::DebugServer::SimHelpInfo RichHelp()
    {
        return Dia::DebugServer::SimHelpInfo{ "Rich help text", "cells", "DiaCellLayers", kHelpParams, 2, false };
    }

    void ExpectSameInfo(const Dia::DebugServer::SimHelpStored& a, const Dia::DebugServer::SimHelpStored& b)
    {
        EXPECT_STREQ(a.description, b.description);
        EXPECT_STREQ(a.category, b.category);
        EXPECT_STREQ(a.owner, b.owner);
        EXPECT_EQ(a.noArgs, b.noArgs);
        ASSERT_EQ(a.paramCount, b.paramCount);
        for (uint32_t i = 0; i < a.paramCount; ++i)
        {
            EXPECT_STREQ(a.params[i].name, b.params[i].name);
            EXPECT_STREQ(a.params[i].type, b.params[i].type);
            EXPECT_STREQ(a.params[i].description, b.params[i].description);
            EXPECT_STREQ(a.params[i].unit, b.params[i].unit);
            EXPECT_STREQ(a.params[i].enumValues, b.params[i].enumValues);
            EXPECT_EQ(a.params[i].required, b.params[i].required);
            EXPECT_EQ(a.params[i].hasRange, b.params[i].hasRange);
            if (a.params[i].hasRange)
            {
                EXPECT_FLOAT_EQ(a.params[i].min, b.params[i].min);
                EXPECT_FLOAT_EQ(a.params[i].max, b.params[i].max);
            }
        }
    }
}

TEST(SimQueryHostHelp, RegisterMessageCarriesTheHostStoredInfo)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterCommand("h.cmd", RichHelp(), &Ok));
    const std::vector<Sent> sent = m.TakeSent();
    ASSERT_EQ(1u, sent.size());
    ASSERT_EQ(SimRegistrationOp::Register, sent[0].parsed.op);
    ASSERT_TRUE(sent[0].parsed.hasInfo);
    ASSERT_TRUE(sent[0].info != nullptr);
    const Dia::DebugServer::SimHelpStored* stored = m.GetHost().GetHelp(StringCRC("h.cmd"));
    ASSERT_TRUE(stored != nullptr);
    ExpectSameInfo(*stored, *sent[0].info);
    EXPECT_STREQ("Rich help text", sent[0].info->description);
}

TEST(SimQueryHostHelp, ReplayCarriesEachNamesInfoAndCountEqualsReplaysSent)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("h.a", Dia::DebugServer::SimHelpInfo{ "help for a" }, &Ok));
    ASSERT_TRUE(m.RegisterCommand("h.b", RichHelp(), &Ok));
    ASSERT_TRUE(m.RegisterQuery("h.c", Dia::DebugServer::SimHelpInfo{ "help for c", "cat" }, &Ok));
    m.TakeSent();
    m.Frame();

    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(4u, f.size());
    ASSERT_EQ(SimRegistrationOp::ResyncBegin, f[0].parsed.op);
    EXPECT_EQ(3u, f[0].parsed.count);
    const char* names[] = { "h.a", "h.b", "h.c" };
    for (size_t i = 1; i < f.size(); ++i)
    {
        ASSERT_EQ(SimRegistrationOp::Replay, f[i].parsed.op);
        EXPECT_STREQ(names[i - 1], f[i].parsed.name);
        ASSERT_TRUE(f[i].info != nullptr) << i;
        const Dia::DebugServer::SimHelpStored* stored = m.GetHost().GetHelp(StringCRC(names[i - 1]));
        ASSERT_TRUE(stored != nullptr);
        ExpectSameInfo(*stored, *f[i].info);
    }
    EXPECT_STREQ("help for a", f[1].info->description);
    EXPECT_STREQ("help for c", f[3].info->description);
}

TEST(SimQueryHostHelp, RegisterWhoseInfoCannotFitIsRejectedAndRolledBack)
{
    TestSim m;
    LogCapture log("DebugServer");
    // Control characters escape to six bytes each, so a maximal info does not fit the 16 KB event.
    const std::string desc(Dia::DebugServer::kSimHelpMaxDescription, '\x01');
    const std::string pd(Dia::DebugServer::kSimHelpMaxParamDescription, '\x01');
    const std::string pe(Dia::DebugServer::kSimHelpMaxParamEnum, '\x01');
    std::vector<Dia::API::ValueParam> params(Dia::DebugServer::kSimHelpMaxParams);
    for (auto& p : params)
        p = { "n", "string", false, pd.c_str(), nullptr, false, 0.f, 0.f, pe.c_str() };
    const Dia::DebugServer::SimHelpInfo big{ desc.c_str(), nullptr, nullptr, params.data(), Dia::DebugServer::kSimHelpMaxParams, false };

    EXPECT_FALSE(m.RegisterQuery("too.big", big, &Ok));
    EXPECT_FALSE(m.RegisterCommand("too.big.cmd", big, &Ok));
    EXPECT_GE(log.Errors("too.big"), 1u);
    EXPECT_TRUE(m.TakeSent().empty());
    EXPECT_FALSE(m.GetHost().Has(StringCRC("too.big")));
    EXPECT_FALSE(m.GetHost().Has(StringCRC("too.big.cmd")));
    EXPECT_TRUE(m.GetHost().GetHelp(StringCRC("too.big")) == nullptr);

    // Rolled back fully: the same name registers again, and no sequence number was consumed.
    ASSERT_TRUE(m.RegisterQuery("too.big", Dia::DebugServer::SimHelpInfo{ "small now" }, &Ok));
    const std::vector<Sent> sent = m.TakeSent();
    ASSERT_EQ(1u, sent.size());
    EXPECT_EQ(1u, sent[0].parsed.seq);

    // The announced set is only the one good name.
    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(2u, f.size());
    EXPECT_EQ(1u, f[0].parsed.count);
}

TEST(SimQueryHostHelp, ReplaySkipsNamesWithoutHelpAndCountMatchesReplaysSent)
{
    TestSim m;
    ASSERT_TRUE(m.RegisterQuery("m.a", Dia::DebugServer::SimHelpInfo{ "a" }, &Ok));
    ASSERT_TRUE(m.RegisterQuery("m.b", Dia::DebugServer::SimHelpInfo{ "b" }, &Ok));
    ASSERT_TRUE(m.RegisterQuery("m.c", Dia::DebugServer::SimHelpInfo{ "c" }, &Ok));
    m.TakeSent();
    m.Frame();
    LogCapture log("DebugServer");

    // Should never happen in production: the host loses the entry behind the module's back.
    m.GetHost().UnregisterQuery(StringCRC("m.b"));

    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_GE(f.size(), 1u);
    ASSERT_EQ(SimRegistrationOp::ResyncBegin, f[0].parsed.op);
    size_t replays = 0;
    for (size_t i = 1; i < f.size(); ++i)
    {
        ASSERT_EQ(SimRegistrationOp::Replay, f[i].parsed.op);
        EXPECT_STRNE("m.b", f[i].parsed.name);
        ++replays;
    }
    EXPECT_EQ(2u, replays);
    EXPECT_EQ(static_cast<uint32_t>(replays), f[0].parsed.count);
    EXPECT_GE(log.Errors("m.b"), 1u);
}

TEST(SimQueryHostHelp, ReplayAfterDroppedRegisterCarriesIdenticalInfo)
{
    TestSim m;
    m.dropPlan.push_back(SendResult::kDroppedOldest);   // the register is lost on the wire
    ASSERT_TRUE(m.RegisterCommand("lost.cmd", RichHelp(), &Ok));
    EXPECT_TRUE(m.TakeSent().empty());

    m.PushResyncRequest();
    const std::vector<Sent> f = m.Frame();
    ASSERT_EQ(2u, f.size());
    ASSERT_EQ(SimRegistrationOp::Replay, f[1].parsed.op);
    ASSERT_TRUE(f[1].info != nullptr);
    ExpectSameInfo(*m.GetHost().GetHelp(StringCRC("lost.cmd")), *f[1].info);
}

// Regression: the host table once used DynamicArrayC, whose RemoveAt destroys the last slot; the next Add then assigned over
// a destroyed shared_ptr and freed help that was still referenced. Churn must keep every stored help valid.
TEST(SimQueryHostHelp, UnregisterRegisterChurnKeepsEveryStoredHelpValid)
{
    TestSim m;
    for (int round = 0; round < 4; ++round)
    {
        for (int i = 0; i < 10; ++i)
        {
            const std::string n = "churn." + std::to_string(i);
            if (round > 0)
                m.UnregisterQuery(n.c_str());
            ASSERT_TRUE(m.RegisterQuery(n.c_str(), Dia::DebugServer::SimHelpInfo{ "churn help" }, &Ok));
        }
    }
    for (int i = 0; i < 10; ++i)
    {
        const Dia::DebugServer::SimHelpStored* help = m.GetHost().GetHelp(StringCRC(("churn." + std::to_string(i)).c_str()));
        ASSERT_TRUE(help != nullptr) << i;
        EXPECT_STREQ("churn help", help->description) << i;
        EXPECT_EQ(0u, help->paramCount) << i;
    }
}

TEST(SimQueryHostHelp, TwentyFourRealisticNamesAllAnnounceAndReplayWithInfo)
{
    TestSim m;
    const Dia::API::ValueParam params[] = {
        { "cell", "int", true, "The cell index to operate on in the current layer", "m", true, 0.f, 1024.f, nullptr },
        { "mode", "enum", false, "Which mode to use when applying the change", nullptr, false, 0.f, 0.f, "weather|ecology|terrain" },
        { "label", "string", false, "Optional label recorded with the change", nullptr, false, 0.f, 0.f, nullptr },
    };
    const std::string desc(100, 'x');
    for (int i = 0; i < 24; ++i)
    {
        const std::string n = "cell.layers.operation." + std::to_string(i);
        ASSERT_TRUE(m.RegisterCommand(n.c_str(), Dia::DebugServer::SimHelpInfo{ desc.c_str(), "cells", "DiaCellLayers", params, 3, false }, &Ok)) << i;
    }
    const std::vector<Sent> reg = m.TakeSent();
    ASSERT_EQ(24u, reg.size());
    for (const Sent& s : reg)
        EXPECT_TRUE(s.info != nullptr);

    m.Frame();
    m.PushResyncRequest();
    std::vector<Sent> all = m.Frame();
    const std::vector<Sent> more = m.Frame();
    all.insert(all.end(), more.begin(), more.end());
    ASSERT_EQ(25u, all.size());
    EXPECT_EQ(24u, all[0].parsed.count);
    for (size_t i = 1; i < all.size(); ++i)
    {
        ASSERT_EQ(SimRegistrationOp::Replay, all[i].parsed.op);
        ASSERT_TRUE(all[i].info != nullptr);
        EXPECT_EQ(3u, all[i].info->paramCount);
    }
}

#endif // DIA_DEBUG
