#include <gtest/gtest.h>

#include <Types/SimAnnounceTracker.h>

#include <cstdio>
#include <cstring>

using namespace Cluiche::AppFlow;
using Dia::DebugServer::SimKind;

namespace
{
    SimRegistration Reg(uint32_t seq, const char* name = "sim.a", SimRegistrationOp op = SimRegistrationOp::Register)
    {
        SimRegistration m;
        m.op = op;
        std::strcpy(m.name, name);
        m.kind = SimKind::Query;
        m.seq = seq;
        return m;
    }
    SimRegistration Replay(const char* name, SimKind kind = SimKind::Query)
    {
        SimRegistration m;
        m.op = SimRegistrationOp::Replay;
        std::strcpy(m.name, name);
        m.kind = kind;
        return m;
    }
    SimRegistration Begin(uint32_t seq, uint32_t count)
    {
        SimRegistration m;
        m.op = SimRegistrationOp::ResyncBegin;
        m.seq = seq;
        m.count = count;
        return m;
    }
    SimRegistration Digest(uint32_t seq, uint32_t count, uint32_t hash)
    {
        SimRegistration m;
        m.op = SimRegistrationOp::Digest;
        m.seq = seq;
        m.count = count;
        m.hash = hash;
        return m;
    }
    void Seed(SimAnnounceTracker& t, uint32_t seq) { t.SeedLastSeq(seq); }
}

TEST(SimAnnounceTracker, InitialState)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.LastSeq(), 0u);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.AnnounceGaps(), 0u);
    EXPECT_EQ(t.DigestMismatches(), 0u);
    EXPECT_EQ(t.ResyncsRequested(), 0u);
    EXPECT_EQ(t.ResyncsCompleted(), 0u);
    EXPECT_EQ(t.ResyncTimeouts(), 0u);
}

TEST(SimAnnounceTracker, ConsecutiveSeqApplies)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnRegistration(Reg(1), 0), SimAnnounceAction::Apply);
    EXPECT_EQ(t.OnRegistration(Reg(2, "sim.a", SimRegistrationOp::Unregister), 0), SimAnnounceAction::Apply);
    EXPECT_EQ(t.LastSeq(), 2u);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.AnnounceGaps(), 0u);
}

TEST(SimAnnounceTracker, GapAppliesAndRequestsOnce)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    EXPECT_EQ(t.OnRegistration(Reg(5), 100), SimAnnounceAction::ApplyAndRequestResync);
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.LastSeq(), 5u);
    EXPECT_EQ(t.AnnounceGaps(), 1u);
    EXPECT_EQ(t.ResyncsRequested(), 1u);
}

TEST(SimAnnounceTracker, RequestRateLimitedTo500ms)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    EXPECT_EQ(t.OnRegistration(Reg(5), 100), SimAnnounceAction::ApplyAndRequestResync);
    // Another message inside the window: applied, no request.
    EXPECT_EQ(t.OnRegistration(Reg(6), 300), SimAnnounceAction::Apply);
    EXPECT_EQ(t.OnRegistration(Reg(9), 599), SimAnnounceAction::Apply);   // gap but rate limited
    EXPECT_EQ(t.ResyncsRequested(), 1u);
    EXPECT_EQ(t.AnnounceGaps(), 2u);
    // 500 ms after the first request: requests again.
    EXPECT_EQ(t.OnRegistration(Reg(12), 600), SimAnnounceAction::ApplyAndRequestResync);
    EXPECT_EQ(t.ResyncsRequested(), 2u);
}

TEST(SimAnnounceTracker, OutOfSyncConsecutiveMessageRequestsAfterWindow)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(3), 0);   // gap from 0 -> requests
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.OnRegistration(Reg(4), 499), SimAnnounceAction::Apply);
    EXPECT_EQ(t.OnRegistration(Reg(5), 500), SimAnnounceAction::ApplyAndRequestResync);
}

TEST(SimAnnounceTracker, DuplicateSeqIsGap)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    t.OnRegistration(Reg(2), 0);
    EXPECT_EQ(t.OnRegistration(Reg(2), 10), SimAnnounceAction::ApplyAndRequestResync);
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.AnnounceGaps(), 1u);
}

TEST(SimAnnounceTracker, ReorderedSeqIsGap)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    EXPECT_EQ(t.OnRegistration(Reg(3), 0), SimAnnounceAction::ApplyAndRequestResync);
    EXPECT_EQ(t.OnRegistration(Reg(2), 10), SimAnnounceAction::Apply);   // rate limited, still a gap
    EXPECT_EQ(t.AnnounceGaps(), 2u);
    EXPECT_EQ(t.LastSeq(), 2u);
}

TEST(SimAnnounceTracker, WrongOpsIgnored)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnRegistration(Digest(0, 0, 0), 0), SimAnnounceAction::None);
    EXPECT_EQ(t.OnRegistration(Replay("x"), 0), SimAnnounceAction::None);
    EXPECT_EQ(t.LastSeq(), 0u);
}

TEST(SimAnnounceTracker, DigestSeqBehindRequestsResync)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    // Tail loss: sim is at seq 3, Main only saw 1.
    EXPECT_EQ(t.OnDigest(Digest(3, 2, 0x10), 2, 0x10, 1000), SimAnnounceAction::RequestResync);
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.DigestMismatches(), 1u);
    EXPECT_EQ(t.ResyncsRequested(), 1u);
}

TEST(SimAnnounceTracker, DigestCountOrHashMismatchRequests)
{
    SimAnnounceTracker a;
    a.OnRegistration(Reg(1), 0);
    EXPECT_EQ(a.OnDigest(Digest(1, 3, 0x10), 2, 0x10, 0), SimAnnounceAction::RequestResync);
    SimAnnounceTracker b;
    b.OnRegistration(Reg(1), 0);
    EXPECT_EQ(b.OnDigest(Digest(1, 2, 0x11), 2, 0x10, 0), SimAnnounceAction::RequestResync);
}

TEST(SimAnnounceTracker, DigestMatchDoesNothing)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    EXPECT_EQ(t.OnDigest(Digest(1, 2, 0x10), 2, 0x10, 5000), SimAnnounceAction::None);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.DigestMismatches(), 0u);
    EXPECT_EQ(t.ResyncsRequested(), 0u);
}

TEST(SimAnnounceTracker, DigestMismatchRateLimited)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnDigest(Digest(1, 1, 1), 0, 0, 0), SimAnnounceAction::RequestResync);
    EXPECT_EQ(t.OnDigest(Digest(1, 1, 1), 0, 0, 499), SimAnnounceAction::None);
    EXPECT_EQ(t.DigestMismatches(), 2u);
    EXPECT_EQ(t.OnDigest(Digest(1, 1, 1), 0, 0, 500), SimAnnounceAction::RequestResync);
    EXPECT_EQ(t.ResyncsRequested(), 2u);
}

TEST(SimAnnounceTracker, NotifyRequestFailedAllowsImmediateRetry)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnDigest(Digest(1, 1, 1), 0, 0, 0), SimAnnounceAction::RequestResync);
    t.NotifyRequestFailed();
    EXPECT_EQ(t.ResyncsRequested(), 0u);
    EXPECT_EQ(t.OnDigest(Digest(1, 1, 1), 0, 0, 10), SimAnnounceAction::RequestResync);
    EXPECT_EQ(t.ResyncsRequested(), 1u);
}

TEST(SimAnnounceTracker, ResyncInstallsOnLastReplay)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    t.OnRegistration(Reg(9), 0);   // gap
    EXPECT_FALSE(t.InSync());

    EXPECT_EQ(t.OnResyncBegin(Begin(20, 3), 10), SimAnnounceAction::None);
    EXPECT_TRUE(t.Staging());
    EXPECT_EQ(t.OnReplay(Replay("sim.a"), 11), SimAnnounceAction::None);
    EXPECT_EQ(t.OnReplay(Replay("sim.b", SimKind::Command), 12), SimAnnounceAction::None);
    // Nothing signalled a table change before the last replay.
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.LastSeq(), 9u);
    EXPECT_EQ(t.ResyncsCompleted(), 0u);

    EXPECT_EQ(t.OnReplay(Replay("sim.c"), 13), SimAnnounceAction::InstallSet);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.LastSeq(), 20u);
    EXPECT_EQ(t.ResyncsCompleted(), 1u);
    ASSERT_EQ(t.StagedCount(), 3u);
    EXPECT_STREQ(t.StagedName(0), "sim.a");
    EXPECT_STREQ(t.StagedName(1), "sim.b");
    EXPECT_EQ(t.StagedKind(1), SimKind::Command);
    EXPECT_STREQ(t.StagedName(2), "sim.c");

    // Next announcement is consecutive: no gap.
    EXPECT_EQ(t.OnRegistration(Reg(21), 20), SimAnnounceAction::Apply);
    EXPECT_EQ(t.AnnounceGaps(), 1u);   // only the original gap
}

TEST(SimAnnounceTracker, RequestWindowResetAfterResync)
{
    SimAnnounceTracker t;
    t.OnDigest(Digest(1, 1, 1), 0, 0, 0);
    t.OnResyncBegin(Begin(1, 0), 10);
    EXPECT_TRUE(t.InSync());
    // A fresh problem right after the resync requests immediately.
    EXPECT_EQ(t.OnDigest(Digest(5, 1, 1), 0, 0, 20), SimAnnounceAction::RequestResync);
}

TEST(SimAnnounceTracker, EmptyResyncInstallsImmediately)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(4), 0);
    EXPECT_EQ(t.OnResyncBegin(Begin(7, 0), 5), SimAnnounceAction::InstallSet);
    EXPECT_EQ(t.StagedCount(), 0u);
    EXPECT_EQ(t.LastSeq(), 7u);
    EXPECT_TRUE(t.InSync());
    EXPECT_FALSE(t.Staging());
    EXPECT_EQ(t.ResyncsCompleted(), 1u);
}

TEST(SimAnnounceTracker, ReplayWithoutStagingIgnored)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnReplay(Replay("sim.a"), 0), SimAnnounceAction::None);
    EXPECT_EQ(t.StagedCount(), 0u);
}

TEST(SimAnnounceTracker, ExtraReplaysAfterCompletionIgnored)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(2, 1), 0);
    EXPECT_EQ(t.OnReplay(Replay("sim.a"), 1), SimAnnounceAction::InstallSet);
    EXPECT_EQ(t.OnReplay(Replay("sim.b"), 2), SimAnnounceAction::None);
    EXPECT_EQ(t.ResyncsCompleted(), 1u);
}

TEST(SimAnnounceTracker, DuplicateReplayNamesCountOnce)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(2, 2), 0);
    EXPECT_EQ(t.OnReplay(Replay("sim.a"), 1), SimAnnounceAction::None);
    EXPECT_EQ(t.OnReplay(Replay("sim.a", SimKind::Query), 2), SimAnnounceAction::None);   // dup: not counted
    EXPECT_EQ(t.StagedCount(), 1u);
    EXPECT_EQ(t.OnReplay(Replay("sim.b"), 3), SimAnnounceAction::InstallSet);
    EXPECT_EQ(t.StagedCount(), 2u);
}

TEST(SimAnnounceTracker, SameNameDifferentKindAreDistinct)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(2, 2), 0);
    t.OnReplay(Replay("sim.a", SimKind::Query), 1);
    EXPECT_EQ(t.OnReplay(Replay("sim.a", SimKind::Command), 2), SimAnnounceAction::InstallSet);
}

TEST(SimAnnounceTracker, SecondBeginRestartsStaging)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(2, 2), 0);
    t.OnReplay(Replay("old"), 1);
    EXPECT_EQ(t.OnResyncBegin(Begin(5, 1), 100), SimAnnounceAction::None);
    EXPECT_EQ(t.StagedCount(), 0u);
    EXPECT_EQ(t.OnReplay(Replay("new"), 101), SimAnnounceAction::InstallSet);
    EXPECT_EQ(t.LastSeq(), 5u);
    EXPECT_STREQ(t.StagedName(0), "new");
    // Timeout window measured from the restart.
    SimAnnounceTracker u;
    u.OnResyncBegin(Begin(2, 2), 0);
    u.OnResyncBegin(Begin(2, 2), 900);
    EXPECT_EQ(u.Tick(1500), SimAnnounceAction::None);
    EXPECT_EQ(u.Tick(1900), SimAnnounceAction::DiscardStaging);
}

TEST(SimAnnounceTracker, OversizedBeginRejected)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnResyncBegin(Begin(5, kSimAnnounceMaxStaged + 1), 0), SimAnnounceAction::None);
    EXPECT_FALSE(t.Staging());
    EXPECT_EQ(t.ResyncsCompleted(), 0u);
    EXPECT_EQ(t.ResyncTimeouts(), 0u);
    EXPECT_EQ(t.LastSeq(), 0u);
    // Max itself is accepted.
    EXPECT_EQ(t.OnResyncBegin(Begin(5, kSimAnnounceMaxStaged), 0), SimAnnounceAction::None);
    EXPECT_TRUE(t.Staging());
}

TEST(SimAnnounceTracker, OversizedBeginDoesNotDisturbActiveStaging)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(5, 2), 0);
    t.OnReplay(Replay("a"), 1);
    EXPECT_EQ(t.OnResyncBegin(Begin(6, kSimAnnounceMaxStaged + 1), 2), SimAnnounceAction::None);
    EXPECT_EQ(t.StagedCount(), 1u);
}

TEST(SimAnnounceTracker, WrongOpsIgnoredByBeginAndReplay)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.OnResyncBegin(Reg(1), 0), SimAnnounceAction::None);
    EXPECT_FALSE(t.Staging());
    t.OnResyncBegin(Begin(2, 1), 0);
    EXPECT_EQ(t.OnReplay(Reg(1), 1), SimAnnounceAction::None);
    EXPECT_EQ(t.StagedCount(), 0u);
}

TEST(SimAnnounceTracker, LostReplayTimesOutAndRecovers)
{
    SimAnnounceTracker t;
    t.OnRegistration(Reg(1), 0);
    t.OnDigest(Digest(4, 3, 7), 1, 1, 100);   // request #1
    t.OnResyncBegin(Begin(4, 3), 110);
    t.OnReplay(Replay("a"), 111);
    t.OnReplay(Replay("b"), 112);             // third replay lost

    EXPECT_EQ(t.Tick(1000), SimAnnounceAction::None);   // 890 ms after start
    EXPECT_EQ(t.Tick(1110), SimAnnounceAction::DiscardStaging);
    EXPECT_EQ(t.ResyncTimeouts(), 1u);
    EXPECT_FALSE(t.Staging());
    EXPECT_EQ(t.StagedCount(), 0u);
    EXPECT_EQ(t.LastSeq(), 1u);               // live state untouched
    EXPECT_FALSE(t.InSync());
    EXPECT_EQ(t.ResyncsCompleted(), 0u);
    EXPECT_EQ(t.Tick(1200), SimAnnounceAction::None);   // only once

    // Next digest mismatch requests again, and a fresh resync completes.
    EXPECT_EQ(t.OnDigest(Digest(4, 3, 7), 1, 1, 1300), SimAnnounceAction::RequestResync);
    EXPECT_EQ(t.OnResyncBegin(Begin(4, 3), 1310), SimAnnounceAction::None);
    t.OnReplay(Replay("a"), 1311);
    t.OnReplay(Replay("b"), 1312);
    EXPECT_EQ(t.OnReplay(Replay("c"), 1313), SimAnnounceAction::InstallSet);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.LastSeq(), 4u);
}

TEST(SimAnnounceTracker, TickWithoutStagingIsNone)
{
    SimAnnounceTracker t;
    EXPECT_EQ(t.Tick(99999), SimAnnounceAction::None);
}

TEST(SimAnnounceTracker, LateReplayAfterTimeoutDiscards)
{
    SimAnnounceTracker t;
    t.OnResyncBegin(Begin(4, 1), 0);
    EXPECT_EQ(t.OnReplay(Replay("a"), 1000), SimAnnounceAction::DiscardStaging);
    EXPECT_EQ(t.ResyncTimeouts(), 1u);
    EXPECT_EQ(t.ResyncsCompleted(), 0u);
}

TEST(SimAnnounceTracker, SeqWrapIsConsecutive)
{
    SimAnnounceTracker t;
    Seed(t, 0xFFFFFFFEu);
    EXPECT_EQ(t.OnRegistration(Reg(0xFFFFFFFFu), 0), SimAnnounceAction::Apply);
    EXPECT_EQ(t.OnRegistration(Reg(0u), 0), SimAnnounceAction::Apply);
    EXPECT_EQ(t.OnRegistration(Reg(1u), 0), SimAnnounceAction::Apply);
    EXPECT_TRUE(t.InSync());
    EXPECT_EQ(t.AnnounceGaps(), 0u);
    EXPECT_EQ(t.LastSeq(), 1u);
}

TEST(SimAnnounceTracker, GapAcrossWrapDetected)
{
    SimAnnounceTracker t;
    Seed(t, 0xFFFFFFFEu);
    EXPECT_EQ(t.OnRegistration(Reg(1u), 0), SimAnnounceAction::ApplyAndRequestResync);
    EXPECT_EQ(t.AnnounceGaps(), 1u);
    EXPECT_FALSE(t.InSync());
}

TEST(SimAnnounceTracker, DigestAcrossWrapMatches)
{
    SimAnnounceTracker t;
    Seed(t, 0xFFFFFFFFu);
    t.OnRegistration(Reg(0u), 0);
    EXPECT_EQ(t.OnDigest(Digest(0u, 1, 9), 1, 9, 0), SimAnnounceAction::None);
}
