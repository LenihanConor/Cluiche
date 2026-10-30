#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/DebugServerHostModule.h>
#include <Modules/SimQueryHostModule.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>
#include <string>

// Sim-hosted queries: module-level degenerate-input tests (no Application / streams connected).
// Stream-backed behaviour (batch limits, registration over the wire) is covered by the
// sim_hosted E2E scenario.

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
        using DebugServerHostModule::PostRequest;
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
    EXPECT_FALSE(m->PostRequest(*req));
}

TEST(SimHostedModules, RegisterRejectsNullEmptyAndTooLongNames)
{
    auto m = MakeSimModule();
    EXPECT_FALSE(m->RegisterQuery(nullptr, &Ok));
    EXPECT_FALSE(m->RegisterQuery("", &Ok));
    EXPECT_FALSE(m->RegisterQuery(std::string(96, 'a').c_str(), &Ok));
    EXPECT_FALSE(m->RegisterCommand(std::string(200, 'a').c_str(), &Ok));
    EXPECT_TRUE(m->RegisterQuery(std::string(95, 'a').c_str(), &Ok));
}

TEST(SimHostedModules, RegisterNullHandlerAndDuplicateReturnFalse)
{
    auto m = MakeSimModule();
    EXPECT_FALSE(m->RegisterQuery("m.null", nullptr));
    EXPECT_TRUE(m->RegisterQuery("m.dup", &Ok));
    EXPECT_FALSE(m->RegisterQuery("m.dup", &Ok));
    EXPECT_FALSE(m->RegisterCommand("m.dup", &Ok));
    EXPECT_TRUE(m->GetHost().Has(StringCRC("m.dup")));
    EXPECT_FALSE(m->GetHost().Has(StringCRC("m.null")));
}

TEST(SimHostedModules, UnregisterUnknownAndNullAreHarmlessAndNameCanBeReRegistered)
{
    auto m = MakeSimModule();
    m->UnregisterQuery(nullptr);
    m->UnregisterQuery("never");
    ASSERT_TRUE(m->RegisterQuery("m.q", &Ok));
    m->UnregisterQuery("m.q");
    EXPECT_FALSE(m->GetHost().Has(StringCRC("m.q")));
    m->UnregisterQuery("m.q");
    EXPECT_TRUE(m->RegisterQuery("m.q", &Ok));
}

TEST(SimHostedModules, AnnounceTableHoldsExactly32Names)
{
    auto m = MakeSimModule();
    for (int i = 0; i < 32; ++i)
    {
        const std::string n = "m.cap" + std::to_string(i);
        ASSERT_TRUE(m->RegisterQuery(n.c_str(), &Ok)) << n;
    }
    LogCapture log("DebugServer");
    EXPECT_FALSE(m->RegisterQuery("m.cap.extra", &Ok));
    EXPECT_EQ(1u, log.Errors("cannot register"));
    m->UnregisterQuery("m.cap0");
    EXPECT_TRUE(m->RegisterQuery("m.cap.extra", &Ok));
}

TEST(SimHostedModules, RegisterWithUnconnectedReplyStreamStillSucceedsAndWarns)
{
    auto m = MakeSimModule();
    LogCapture log("DebugServer");
    EXPECT_TRUE(m->RegisterQuery("m.unconnected", &Ok));
    EXPECT_EQ(1u, log.Warnings("rejected by the stream"));
    EXPECT_TRUE(m->GetHost().Has(StringCRC("m.unconnected")));
}

#endif // DIA_DEBUG
