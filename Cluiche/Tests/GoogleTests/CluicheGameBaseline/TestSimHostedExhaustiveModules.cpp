#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/DebugServerHostModule.h>
#include <Modules/SimQueryHostModule.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <Types/SimRegistrationMessage.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

// Sim-hosted queries: module-level degenerate-input tests (no Application / streams connected).
// Stream-backed behaviour (batch limits, registration over the wire) is covered by the
// sim_hosted E2E scenario.

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
    using Cluiche::AppFlow::SimQueryHostModule;
    using Dia::CellLayers::Testing::LogCapture;
    using Dia::Core::StringCRC;

    Json::Value Ok(const Json::Value&)
    {
        Json::Value v;
        v["ok"] = true;
        return v;
    }

    class TestableHostModule : public DebugServerHostModule
    {
    public:
        TestableHostModule() : DebugServerHostModule(StringCRC("TestHostModule")) {}
        using DebugServerHostModule::HandleSimReply;
        using DebugServerHostModule::TickAnnounce;
        using DebugServerHostModule::GetAnnounceTracker;

        // Captures posted requests instead of using the (unconnected) stream.
        bool PostRequest(const Dia::DebugServer::SimRequestEvent& r) override
        {
            posted.push_back(std::make_tuple(r.requestId, r.nameCrc, r.kind));
            return accept;
        }
        bool BasePostRequest(const Dia::DebugServer::SimRequestEvent& r) { return DebugServerHostModule::PostRequest(r); }

        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> posted;
        bool accept = true;
    };

    using Dia::DebugServer::SimKind;

    // Replies are ~16 KB: keep them off the stack.
    struct ReplyBox
    {
        std::unique_ptr<Dia::DebugServer::SimReplyEvent> r{ new Dia::DebugServer::SimReplyEvent() };
        Dia::DebugServer::SimReplyEvent& operator*() { return *r; }
    };

    std::unique_ptr<SimQueryHostModule> MakeSimModule()
    {
        return std::unique_ptr<SimQueryHostModule>(new SimQueryHostModule(StringCRC("TestSimQueryHost")));
    }
}

TEST(SimHostedModules, PostRequestOnUnconnectedStreamReturnsFalse)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    Dia::DebugServer::SimRequestEvent* req = new Dia::DebugServer::SimRequestEvent();
    std::unique_ptr<Dia::DebugServer::SimRequestEvent> owner(req);
    req->requestId = 1;
    EXPECT_FALSE(m->BasePostRequest(*req));
}

TEST(SimHostedModules, RegisterRejectsNullEmptyAndTooLongNames)
{
    auto m = MakeSimModule();
    EXPECT_FALSE(m->RegisterQuery(nullptr, Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_FALSE(m->RegisterQuery("", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_FALSE(m->RegisterQuery(std::string(96, 'a').c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_FALSE(m->RegisterCommand(std::string(200, 'a').c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_TRUE(m->RegisterQuery(std::string(95, 'a').c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
}

TEST(SimHostedModules, RegisterNullHandlerAndDuplicateReturnFalse)
{
    auto m = MakeSimModule();
    EXPECT_FALSE(m->RegisterQuery("m.null", Dia::DebugServer::SimHelpInfo{"test help"}, nullptr));
    EXPECT_TRUE(m->RegisterQuery("m.dup", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_FALSE(m->RegisterQuery("m.dup", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_FALSE(m->RegisterCommand("m.dup", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_TRUE(m->GetHost().Has(StringCRC("m.dup")));
    EXPECT_FALSE(m->GetHost().Has(StringCRC("m.null")));
}

TEST(SimHostedModules, UnregisterUnknownAndNullAreHarmlessAndNameCanBeReRegistered)
{
    auto m = MakeSimModule();
    m->UnregisterQuery(nullptr);
    m->UnregisterQuery("never");
    ASSERT_TRUE(m->RegisterQuery("m.q", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    m->UnregisterQuery("m.q");
    EXPECT_FALSE(m->GetHost().Has(StringCRC("m.q")));
    m->UnregisterQuery("m.q");
    EXPECT_TRUE(m->RegisterQuery("m.q", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
}

TEST(SimHostedModules, AnnounceTableHoldsExactly32Names)
{
    auto m = MakeSimModule();
    for (int i = 0; i < 32; ++i)
    {
        const std::string n = "m.cap" + std::to_string(i);
        ASSERT_TRUE(m->RegisterQuery(n.c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, &Ok)) << n;
    }
    LogCapture log("DebugServer");
    EXPECT_FALSE(m->RegisterQuery("m.cap.extra", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_EQ(1u, log.Errors("cannot register"));
    m->UnregisterQuery("m.cap0");
    EXPECT_TRUE(m->RegisterQuery("m.cap.extra", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
}

TEST(SimHostedModules, RegisterWithUnconnectedReplyStreamStillSucceedsAndWarns)
{
    auto m = MakeSimModule();
    LogCapture log("DebugServer");
    EXPECT_TRUE(m->RegisterQuery("m.unconnected", Dia::DebugServer::SimHelpInfo{"test help"}, &Ok));
    EXPECT_EQ(1u, log.Warnings("rejected by the stream"));
    EXPECT_TRUE(m->GetHost().Has(StringCRC("m.unconnected")));
}

TEST(SimHostedModules, SeqGapPostsResyncRequestAndStillAppliesAnnouncement)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    ReplyBox box;
    LogCapture log("DebugServer");
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(
        Cluiche::AppFlow::SimRegistrationOp::Register, "gap.a", SimKind::Query, 1, TestHelp(), *box));
    m->HandleSimReply(*box, 1000);
    EXPECT_TRUE(m->posted.empty());

    ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(
        Cluiche::AppFlow::SimRegistrationOp::Register, "gap.b", SimKind::Query, 5, TestHelp(), *box));   // 2..4 lost
    m->HandleSimReply(*box, 1001);
    ASSERT_EQ(1u, m->posted.size());
    EXPECT_EQ(0u, std::get<0>(m->posted[0]));
    EXPECT_EQ(0u, std::get<1>(m->posted[0]));
    EXPECT_EQ(static_cast<uint32_t>(SimKind::Resync), std::get<2>(m->posted[0]));
    EXPECT_EQ(1u, log.Warnings("announcement gap"));

    uint32_t count = 0, hash = 0;
    m->GetServer()->GetSimHostedDigest(count, hash);
    EXPECT_EQ(2u, count);   // gap message was still applied
}

TEST(SimHostedModules, FailedResyncPostRetriesOnNextTrigger)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    m->accept = false;
    ReplyBox box;
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimDigestReply(3, 2, 0x1234u, *box));
    m->HandleSimReply(*box, 1000);
    ASSERT_EQ(1u, m->posted.size());
    m->accept = true;
    m->HandleSimReply(*box, 1001);   // inside the rate-limit window, but the failed post cleared it
    EXPECT_EQ(2u, m->posted.size());
}

TEST(SimHostedModules, MatchingDigestPostsNothingMismatchPostsResync)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    ReplyBox box;
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(
        Cluiche::AppFlow::SimRegistrationOp::Register, "dig.a", SimKind::Query, 1, TestHelp(), *box));
    m->HandleSimReply(*box, 1000);
    uint32_t count = 0, hash = 0;
    m->GetServer()->GetSimHostedDigest(count, hash);

    ASSERT_TRUE(Cluiche::AppFlow::BuildSimDigestReply(1, count, hash, *box));
    m->HandleSimReply(*box, 1100);
    EXPECT_TRUE(m->posted.empty());

    ASSERT_TRUE(Cluiche::AppFlow::BuildSimDigestReply(1, count, hash ^ 1u, *box));
    LogCapture log("DebugServer");
    m->HandleSimReply(*box, 1200);
    EXPECT_EQ(1u, m->posted.size());
    EXPECT_EQ(1u, log.Warnings("digest mismatch"));
}

TEST(SimHostedModules, ResyncReplayCompletionSwapsTableAndLogsInfo)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    ASSERT_TRUE(m->GetServer()->RegisterSimHosted(StringCRC("stale.a"), SimKind::Query));

    ReplyBox box;
    LogCapture log("DebugServer");
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimResyncBeginReply(9, 2, *box));
    m->HandleSimReply(*box, 2000);
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimReplayReply("fresh.a", SimKind::Query, TestHelp(), *box));
    m->HandleSimReply(*box, 2001);
    // not complete yet: stale table untouched
    uint32_t count = 0, hash = 0;
    m->GetServer()->GetSimHostedDigest(count, hash);
    EXPECT_EQ(1u, count);
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimReplayReply("fresh.b", SimKind::Command, TestHelp(), *box));
    m->HandleSimReply(*box, 2002);

    m->GetServer()->GetSimHostedDigest(count, hash);
    EXPECT_EQ(2u, count);
    const uint32_t expected = Cluiche::AppFlow::SimNameSetEntryHash("fresh.a", SimKind::Query)
                            + Cluiche::AppFlow::SimNameSetEntryHash("fresh.b", SimKind::Command);
    EXPECT_EQ(expected, hash);
    EXPECT_TRUE(m->GetAnnounceTracker().InSync());
    EXPECT_EQ(9u, m->GetAnnounceTracker().LastSeq());
    EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "resync complete, 2 names"));
}

TEST(SimHostedModules, StalledResyncStagingIsDiscardedByTickAndLiveTableKept)
{
    std::unique_ptr<TestableHostModule> m(new TestableHostModule());
    ASSERT_TRUE(m->GetServer()->RegisterSimHosted(StringCRC("live.a"), SimKind::Query));
    ReplyBox box;
    ASSERT_TRUE(Cluiche::AppFlow::BuildSimResyncBeginReply(4, 3, *box));
    m->HandleSimReply(*box, 3000);
    LogCapture log("DebugServer");
    m->TickAnnounce(3000 + Cluiche::AppFlow::kSimResyncTimeoutMs + 1);
    EXPECT_FALSE(m->GetAnnounceTracker().Staging());
    EXPECT_EQ(1u, log.Warnings("timed out"));
    uint32_t count = 0, hash = 0;
    m->GetServer()->GetSimHostedDigest(count, hash);
    EXPECT_EQ(1u, count);
}

#endif // DIA_DEBUG
