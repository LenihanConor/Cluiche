////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfPauseCommands.cpp
// GoogleTest suite - perf-live-control task 5: perf.pause / perf.resume / perf.resume_all
// (Main-hosted, per-client ownership, released on disconnect) and the DebugServer
// client-disconnect API.  Debug only (_DEBUG); empty in Release.
// No sockets: commands are dispatched through DebugServer::HandleCommand with a send hook.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef _DEBUG

#include <Types/PerfPauseCommands.h>

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaApplicationFlow/PerfPause.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaProtobuf/ProtoStructConverter.h>

#include <cstring>
#include <string>
#include <vector>

namespace Dia { namespace DebugServer {
    struct DebugServerTestAccess
    {
        static void HandleCommand(DebugServer& s, int connId, const dia::debug::DebugMessage& m) { s.HandleCommand(connId, m); }
        static void HandleConnection(DebugServer& s, int connId, bool connected) { s.HandleConnection(connId, connected); }
    };
}}

using namespace Dia::ApplicationFlow;
using Cluiche::AppFlow::PerfPauseCommands;
using Dia::Core::StringCRC;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::DebugServerTestAccess;

namespace
{
    struct Sent { int connId; dia::debug::DebugMessage msg; };
    void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg)
    {
        static_cast<std::vector<Sent>*>(ctx)->push_back(Sent{ connId, msg });
    }

    // Module ids "modA"/"modB" are plain modules; "kernel" resolves to a denylisted type; anything else is unknown.
    bool Resolve(void*, const StringCRC& id, StringCRC& outType)
    {
        if (id == StringCRC("modA") || id == StringCRC("modB")) { outType = StringCRC("PlainModule"); return true; }
        if (id == StringCRC("kernel"))   { outType = StringCRC("KernelModule"); return true; }
        if (id == StringCRC("dbghost"))  { outType = StringCRC("DebugServerHostModule"); return true; }
        if (id == StringCRC("simquery")) { outType = StringCRC("SimQueryHostModule"); return true; }
        return false;
    }

    struct PerfPauseCmdRig : ::testing::Test
    {
        FrameStreamStore<PerfPauseSet> store{ StringCRC("$perfpauseCmdTest") };
        PerfPauseController ctl{ &store };
        DebugServer server;
        std::vector<Sent> sent;
        PerfPauseCommands cmds{ ctl, server };

        void SetUp() override
        {
            ctl.SetModuleResolver(&Resolve, nullptr);
            server.SetSendHook(&CaptureSend, &sent);
            cmds.Register();
        }

        // Dispatch a command as client 'conn'; returns the COMMAND_RESPONSE.
        const dia::debug::CommandResponse& Call(int conn, const char* name, const Json::Value& args = Json::Value(Json::objectValue))
        {
            sent.clear();
            dia::debug::DebugMessage m;
            m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
            auto* c = m.mutable_command_request();
            c->set_command(name);
            Dia::Proto::JsonValueToProtoStruct(args, c->mutable_payload());
            DebugServerTestAccess::HandleCommand(server, conn, m);
            EXPECT_EQ(sent.size(), 1u);
            return sent.back().msg.command_response();
        }

        static Json::Value Args(const char* kind, const char* id)
        {
            Json::Value a(Json::objectValue);
            if (kind) a["kind"] = kind;
            if (id)   a["id"]   = id;
            return a;
        }

        bool ModulePaused(const char* id) const { return ctl.IsPaused(PerfPauseKind::kModule, StringCRC(id)); }
    };
}

TEST_F(PerfPauseCmdRig, PauseAndResumeModule_Ok)                                // AC1
{
    EXPECT_TRUE(Call(3, "perf.pause", Args("module", "modA")).success());
    EXPECT_TRUE(ModulePaused("modA"));
    EXPECT_TRUE(Call(3, "perf.resume", Args("module", "modA")).success());
    EXPECT_FALSE(ModulePaused("modA"));
}

TEST_F(PerfPauseCmdRig, PauseDomain_AcceptsAnyNonEmptyId)
{
    EXPECT_TRUE(Call(1, "perf.pause", Args("domain", "WorldGen")).success());
    EXPECT_TRUE(ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("WorldGen")));
    EXPECT_TRUE(Call(1, "perf.resume", Args("domain", "WorldGen")).success());
    EXPECT_FALSE(ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("WorldGen")));
}

TEST_F(PerfPauseCmdRig, DenylistedModules_AreRefusedWithReason)                 // AC3
{
    for (const char* id : { "kernel", "dbghost", "simquery" })
    {
        const auto& r = Call(1, "perf.pause", Args("module", id));
        EXPECT_FALSE(r.success()) << id;
        EXPECT_FALSE(r.message().empty()) << id;
        EXPECT_FALSE(ModulePaused(id)) << id;
    }
}

TEST_F(PerfPauseCmdRig, UnknownModule_Fails)
{
    const auto& r = Call(1, "perf.pause", Args("module", "nope"));
    EXPECT_FALSE(r.success());
    EXPECT_FALSE(r.message().empty());
}

TEST_F(PerfPauseCmdRig, BadKindOrMissingId_Fails)
{
    EXPECT_FALSE(Call(1, "perf.pause", Args("widget", "modA")).success());
    EXPECT_FALSE(Call(1, "perf.pause", Args(nullptr, "modA")).success());
    EXPECT_FALSE(Call(1, "perf.pause", Args("module", nullptr)).success());
    EXPECT_FALSE(Call(1, "perf.pause", Args("module", "")).success());
    EXPECT_FALSE(Call(1, "perf.resume", Args("module", nullptr)).success());
    EXPECT_FALSE(ModulePaused("modA"));
}

TEST_F(PerfPauseCmdRig, ResumeWhatWasNotPaused_Fails)
{
    EXPECT_FALSE(Call(1, "perf.resume", Args("module", "modA")).success());
}

TEST_F(PerfPauseCmdRig, ResumeAllWithNothingPaused_Ok)
{
    EXPECT_TRUE(Call(1, "perf.resume_all").success());
}

TEST_F(PerfPauseCmdRig, ResumeAll_ReleasesOnlyCallersPauses)
{
    Call(1, "perf.pause", Args("module", "modA"));
    Call(1, "perf.pause", Args("domain", "D1"));
    Call(2, "perf.pause", Args("module", "modB"));
    EXPECT_TRUE(Call(1, "perf.resume_all").success());
    EXPECT_FALSE(ModulePaused("modA"));
    EXPECT_FALSE(ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("D1")));
    EXPECT_TRUE(ModulePaused("modB"));
}

TEST_F(PerfPauseCmdRig, SameIdPausedByTwoClients_StaysPausedUntilBothRelease)
{
    Call(1, "perf.pause", Args("module", "modA"));
    Call(2, "perf.pause", Args("module", "modA"));
    Call(1, "perf.resume", Args("module", "modA"));
    EXPECT_TRUE(ModulePaused("modA"));
    Call(2, "perf.resume", Args("module", "modA"));
    EXPECT_FALSE(ModulePaused("modA"));
}

TEST_F(PerfPauseCmdRig, ResumeBy_NonOwner_DoesNotRelease)
{
    Call(1, "perf.pause", Args("module", "modA"));
    EXPECT_FALSE(Call(2, "perf.resume", Args("module", "modA")).success());
    EXPECT_TRUE(ModulePaused("modA"));
}

TEST_F(PerfPauseCmdRig, Disconnect_ReleasesOnlyThatClientsPauses)               // AC4
{
    EXPECT_TRUE(ctl.Pause(PerfPauseController::kLocalOwner, PerfPauseKind::kModule, StringCRC("modB")).ok);
    Call(1, "perf.pause", Args("module", "modA"));
    Call(1, "perf.pause", Args("domain", "D1"));
    Call(2, "perf.pause", Args("domain", "D2"));

    DebugServerTestAccess::HandleConnection(server, 1, false);

    EXPECT_FALSE(ModulePaused("modA"));
    EXPECT_FALSE(ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("D1")));
    EXPECT_TRUE(ctl.IsPaused(PerfPauseKind::kDomain, StringCRC("D2")));   // other client
    EXPECT_TRUE(ModulePaused("modB"));                                     // local owner
}

TEST_F(PerfPauseCmdRig, ConnectionZero_DoesNotCollideWithLocalOwner)
{
    EXPECT_NE(PerfPauseCommands::OwnerForConn(0), PerfPauseController::kLocalOwner);
    EXPECT_EQ(PerfPauseCommands::OwnerForConn(-1), PerfPauseController::kLocalOwner);
    EXPECT_TRUE(ctl.Pause(PerfPauseController::kLocalOwner, PerfPauseKind::kModule, StringCRC("modB")).ok);
    Call(0, "perf.pause", Args("module", "modA"));
    DebugServerTestAccess::HandleConnection(server, 0, false);
    EXPECT_FALSE(ModulePaused("modA"));
    EXPECT_TRUE(ModulePaused("modB"));
}

TEST_F(PerfPauseCmdRig, Unregister_RemovesCommandsAndObserver)
{
    cmds.Unregister();
    Dia::Core::StringCRC name("perf.pause");
    Json::Value r = Dia::API::ExecuteCommandJson(name, Args("module", "modA"));
    EXPECT_FALSE(r["success"].asBool());   // command not found
    ctl.Pause(2, PerfPauseKind::kModule, StringCRC("modA"));
    DebugServerTestAccess::HandleConnection(server, 1, false);   // observer gone: nothing released
    EXPECT_TRUE(ModulePaused("modA"));
}

TEST_F(PerfPauseCmdRig, EveryCommandHasHelpDescription)
{
    for (const char* n : { "perf.pause", "perf.resume", "perf.resume_all" })
    {
        Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256> all;
        Dia::API::ListJsonCommandInfos(all);
        bool found = false;
        for (unsigned int i = 0; i < all.Size(); ++i)
            if (all[i].name == StringCRC(n))
            {
                found = true;
                ASSERT_NE(all[i].description, nullptr) << n;
                EXPECT_GT(std::strlen(all[i].description), 0u) << n;
            }
        EXPECT_TRUE(found) << n;
    }
}

TEST(DebugServerDisconnectApi, CurrentCommandConnId_IsMinusOneOutsideDispatch)
{
    DebugServer server;
    EXPECT_EQ(server.GetCurrentCommandConnId(), -1);
}

TEST(DebugServerDisconnectApi, ObserverCalledInOrder_AddIsIdempotent_RemoveWorks_CapacityBounded)
{
    struct Ctx { std::vector<int>* log; int tag; };
    static auto cb = +[](void* c, int connId) { auto* x = static_cast<Ctx*>(c); x->log->push_back(x->tag * 100 + connId); };

    DebugServer server;
    std::vector<int> log;
    Ctx a{ &log, 1 }, b{ &log, 2 };
    EXPECT_FALSE(server.AddDisconnectObserver(nullptr, nullptr));
    EXPECT_TRUE(server.AddDisconnectObserver(cb, &a));
    EXPECT_TRUE(server.AddDisconnectObserver(cb, &a));   // duplicate: no second entry
    EXPECT_TRUE(server.AddDisconnectObserver(cb, &b));
    DebugServerTestAccess::HandleConnection(server, 7, false);
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0], 107);
    EXPECT_EQ(log[1], 207);

    server.RemoveDisconnectObserver(cb, &a);
    log.clear();
    DebugServerTestAccess::HandleConnection(server, 8, false);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0], 208);

    // Capacity: b is already in; fill to the limit then one more is refused.
    Ctx extra[DebugServer::kMaxDisconnectObservers];
    unsigned int added = 1;
    for (unsigned int i = 0; i < DebugServer::kMaxDisconnectObservers; ++i)
    {
        extra[i] = Ctx{ &log, 10 + static_cast<int>(i) };
        if (server.AddDisconnectObserver(cb, &extra[i])) ++added;
    }
    EXPECT_EQ(added, DebugServer::kMaxDisconnectObservers);
}

#endif // _DEBUG
