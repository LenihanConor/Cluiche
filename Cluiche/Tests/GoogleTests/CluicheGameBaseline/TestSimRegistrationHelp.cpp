#include <gtest/gtest.h>

#include <Types/SimRegistrationMessage.h>
#include <DiaDebugServer/SimHelpInfo.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Sim-Hosted Command Help, Task 2: help info carried on register / replay messages.

using namespace Cluiche::AppFlow;
using Dia::DebugServer::SimHelpInfo;
using Dia::DebugServer::SimHelpResult;
using Dia::DebugServer::SimHelpStored;
using Dia::DebugServer::SimKind;
using Dia::DebugServer::SimReplyEvent;

namespace
{
    std::unique_ptr<SimReplyEvent> MakeReply()
    {
        std::unique_ptr<SimReplyEvent> r(new SimReplyEvent);
        std::memset(r.get(), 0, sizeof(SimReplyEvent));
        return r;
    }

    SimRegistration ParseJson(const std::string& json)
    {
        auto reply = MakeReply();
        std::strcpy(reply->json, json.c_str());
        return ParseSimRegistrationReply(*reply);
    }

    std::unique_ptr<SimHelpStored> Stored(const SimHelpInfo& info)
    {
        std::unique_ptr<SimHelpStored> s(new SimHelpStored);
        std::memset(s.get(), 0, sizeof(SimHelpStored));
        EXPECT_EQ(Dia::DebugServer::CopyHelp(info, *s), SimHelpResult::Ok);
        return s;
    }

    void ExpectSameHelp(const SimHelpStored& a, const SimHelpStored& b)
    {
        EXPECT_STREQ(a.description, b.description);
        EXPECT_STREQ(a.category, b.category);
        EXPECT_STREQ(a.owner, b.owner);
        EXPECT_EQ(a.noArgs, b.noArgs);
        ASSERT_EQ(a.paramCount, b.paramCount);
        for (uint32_t i = 0; i < a.paramCount; ++i)
        {
            EXPECT_STREQ(a.params[i].name, b.params[i].name) << i;
            EXPECT_STREQ(a.params[i].type, b.params[i].type) << i;
            EXPECT_STREQ(a.params[i].description, b.params[i].description) << i;
            EXPECT_STREQ(a.params[i].unit, b.params[i].unit) << i;
            EXPECT_STREQ(a.params[i].enumValues, b.params[i].enumValues) << i;
            EXPECT_EQ(a.params[i].required, b.params[i].required) << i;
            EXPECT_EQ(a.params[i].hasRange, b.params[i].hasRange) << i;
            if (a.params[i].hasRange)
            {
                EXPECT_FLOAT_EQ(a.params[i].min, b.params[i].min) << i;
                EXPECT_FLOAT_EQ(a.params[i].max, b.params[i].max) << i;
            }
        }
    }

    std::string RegJson(const std::string& infoMember, bool replay = false)
    {
        std::string s = "{\"register\":{\"name\":\"x\",\"kind\":\"query\"" + infoMember + "}";
        s += replay ? ",\"replay\":true}" : ",\"seq\":1}";
        return s;
    }

    std::string Info(const std::string& body) { return ",\"info\":{" + body + "}"; }
    std::string Fill(char c, uint32_t n) { return std::string(n, c); }
    std::string Q(const std::string& s) { return "\"" + s + "\""; }
}

TEST(SimRegistrationHelp, MinimalInfoRoundTripsAndOmitsOptionalFields)
{
    auto stored = Stored(SimHelpInfo{ "does a thing" });
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.echo", SimKind::Query, 7, stored.get(), *reply));
    const std::string text = reply->json;
    EXPECT_NE(text.find("\"info\":{"), std::string::npos) << text;
    EXPECT_EQ(text.find("category"), std::string::npos) << text;
    EXPECT_EQ(text.find("owner"), std::string::npos) << text;
    EXPECT_EQ(text.find("params"), std::string::npos) << text;
    EXPECT_EQ(text.find("noArgs"), std::string::npos) << text;

    SimHelpStored out;
    std::memset(&out, 0, sizeof(out));
    const SimRegistration parsed = ParseSimRegistrationReply(*reply, &out);
    ASSERT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_TRUE(parsed.hasInfo);
    ExpectSameHelp(*stored, out);
}

TEST(SimRegistrationHelp, TypicalInfoRoundTripsEveryParamFieldForRegisterAndReplay)
{
    const Dia::API::ValueParam params[] = {
        { "cell", "int", true, "Cell index", "m", true, -5.5f, 12.25f, nullptr },
        { "mode", "enum", false, "Which mode", nullptr, false, 0.f, 0.f, "weather|ecology" },
        { "flag", "bool", false, nullptr, nullptr, false, 0.f, 0.f, nullptr },
    };
    auto stored = Stored(SimHelpInfo{ "Sets a cell", "cells", "DiaCellLayers", params, 3, false });

    for (int replay = 0; replay < 2; ++replay)
    {
        auto reply = MakeReply();
        if (replay)
            ASSERT_TRUE(BuildSimReplayReply("c.set", SimKind::Command, stored.get(), *reply));
        else
            ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "c.set", SimKind::Command, 3, stored.get(), *reply));
        SimHelpStored out;
        std::memset(&out, 0, sizeof(out));
        const SimRegistration parsed = ParseSimRegistrationReply(*reply, &out);
        EXPECT_EQ(parsed.op, replay ? SimRegistrationOp::Replay : SimRegistrationOp::Register) << reply->json;
        EXPECT_TRUE(parsed.hasInfo);
        ExpectSameHelp(*stored, out);
        EXPECT_TRUE(out.params[0].required);
        EXPECT_TRUE(out.params[0].hasRange);
        EXPECT_STREQ(out.params[1].enumValues, "weather|ecology");
        EXPECT_FALSE(out.params[2].hasRange);
        const std::string text = reply->json;
        EXPECT_NE(text.find("\"min\""), std::string::npos);
        EXPECT_EQ(text.find("\"min\""), text.rfind("\"min\"")) << "min emitted only for the ranged param: " << text;
    }
}

TEST(SimRegistrationHelp, NoArgsFlagRoundTrips)
{
    SimHelpInfo info{ "no args" };
    info.noArgs = true;
    auto stored = Stored(info);
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "n.a", SimKind::Command, 1, stored.get(), *reply));
    SimHelpStored out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
    EXPECT_TRUE(out.noArgs);
}

TEST(SimRegistrationHelp, MaximalPlainInfoRoundTripsUnchanged)
{
    using namespace Dia::DebugServer;
    const std::string desc = Fill('d', kSimHelpMaxDescription), cat = Fill('c', kSimHelpMaxCategory), owner = Fill('o', kSimHelpMaxOwner);
    const std::string pn = Fill('n', kSimHelpMaxParamName), pt = Fill('t', kSimHelpMaxParamType), pd = Fill('p', kSimHelpMaxParamDescription),
                      pu = Fill('u', kSimHelpMaxParamUnit), pe = Fill('e', kSimHelpMaxParamEnum);
    std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
    for (auto& p : params)
        p = { pn.c_str(), pt.c_str(), true, pd.c_str(), pu.c_str(), true, -1.5f, 99.5f, pe.c_str() };
    auto stored = Stored(SimHelpInfo{ desc.c_str(), cat.c_str(), owner.c_str(), params.data(), kSimHelpMaxParams, false });
    ASSERT_EQ(stored->paramCount, kSimHelpMaxParams);

    char longName[kSimRegistrationMaxName];
    std::memset(longName, 'a', sizeof(longName));
    longName[sizeof(longName) - 1] = '\0';

    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, longName, SimKind::Query, 0xFFFFFFFFu, stored.get(), *reply));
    EXPECT_LT(std::strlen(reply->json), static_cast<size_t>(kSimPayloadBytes));
    SimHelpStored out;
    std::memset(&out, 0, sizeof(out));
    ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
    ExpectSameHelp(*stored, out);

    auto replayReply = MakeReply();
    ASSERT_TRUE(BuildSimReplayReply(longName, SimKind::Command, stored.get(), *replayReply));
    std::memset(&out, 0, sizeof(out));
    ASSERT_EQ(ParseSimRegistrationReply(*replayReply, &out).op, SimRegistrationOp::Replay);
    ExpectSameHelp(*stored, out);
}

TEST(SimRegistrationHelp, QuoteHeavyMaximalInfoEitherRoundTripsOrIsRejectedWithoutTouchingOut)
{
    using namespace Dia::DebugServer;
    const std::string desc = Fill('"', kSimHelpMaxDescription), pd = Fill('"', kSimHelpMaxParamDescription), pe = Fill('"', kSimHelpMaxParamEnum);
    std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
    for (auto& p : params)
        p = { "n", "string", false, pd.c_str(), nullptr, false, 0.f, 0.f, pe.c_str() };
    auto stored = Stored(SimHelpInfo{ desc.c_str(), nullptr, nullptr, params.data(), kSimHelpMaxParams, false });
    auto reply = MakeReply();
    if (BuildSimRegistrationReply(SimRegistrationOp::Register, "q.h", SimKind::Query, 1, stored.get(), *reply))
    {
        SimHelpStored out;
        std::memset(&out, 0, sizeof(out));
        ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
        ExpectSameHelp(*stored, out);
    }
    else
    {
        EXPECT_EQ(reply->json[0], '\0');
    }
}

TEST(SimRegistrationHelp, InfoThatCannotFitTheEventIsRejectedNeverTruncated)
{
    using namespace Dia::DebugServer;
    // A control character escapes to 6 bytes ("\u0001"), so a maximal info cannot fit 16 KB.
    const std::string desc = Fill('\x01', kSimHelpMaxDescription), pd = Fill('\x01', kSimHelpMaxParamDescription), pe = Fill('\x01', kSimHelpMaxParamEnum);
    std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
    for (auto& p : params)
        p = { "n", "string", false, pd.c_str(), nullptr, false, 0.f, 0.f, pe.c_str() };
    auto stored = Stored(SimHelpInfo{ desc.c_str(), nullptr, nullptr, params.data(), kSimHelpMaxParams, false });

    auto reply = MakeReply();
    std::memset(reply.get(), 0x5A, sizeof(SimReplyEvent));
    reply->requestId = 1234;
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "big", SimKind::Query, 1, stored.get(), *reply));
    EXPECT_FALSE(BuildSimReplayReply("big", SimKind::Query, stored.get(), *reply));
    EXPECT_EQ(reply->requestId, 1234u);
    EXPECT_EQ(reply->json[0], 0x5A);
    EXPECT_EQ(reply->json[kSimPayloadBytes - 1], 0x5A);
}

TEST(SimRegistrationHelp, BuildRequiresInfoForRegisterAndReplayButNotUnregister)
{
    auto reply = MakeReply();
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "x", SimKind::Query, 1, nullptr, *reply));
    EXPECT_FALSE(BuildSimReplayReply("x", SimKind::Query, nullptr, *reply));
    EXPECT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Unregister, "x", SimKind::Query, 1, nullptr, *reply));
    EXPECT_EQ(std::string(reply->json).find("info"), std::string::npos);
}

TEST(SimRegistrationHelp, BuildRejectsEmptyDescription)
{
    auto stored = Stored(SimHelpInfo{ "ok" });
    stored->description[0] = '\0';
    auto reply = MakeReply();
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "x", SimKind::Query, 1, stored.get(), *reply));
    EXPECT_FALSE(BuildSimReplayReply("x", SimKind::Query, stored.get(), *reply));
}

TEST(SimRegistrationHelp, MissingInfoIsMalformedForRegisterAndReplay)
{
    EXPECT_EQ(ParseJson(RegJson("")).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson("", true)).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(",\"info\":null")).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(",\"info\":[]", true)).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(",\"info\":\"text\"")).op, SimRegistrationOp::Malformed);
    // Sanity: a minimal valid info parses.
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":\"d\""))).op, SimRegistrationOp::Register);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":\"d\""), true)).op, SimRegistrationOp::Replay);
}

TEST(SimRegistrationHelp, BadDescriptionIsMalformed)
{
    const std::string over = Fill('x', Dia::DebugServer::kSimHelpMaxDescription + 1);
    const std::string atMax = Fill('x', Dia::DebugServer::kSimHelpMaxDescription);
    EXPECT_EQ(ParseJson(RegJson(Info(""))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":\"\""))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":5"))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":null"))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":" + Q(over)))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info("\"description\":" + Q(atMax)))).op, SimRegistrationOp::Register);
}

TEST(SimRegistrationHelp, WrongTypedOptionalFieldsAreMalformed)
{
    const std::string d = "\"description\":\"d\"";
    const char* bad[] = {
        ",\"category\":5", ",\"owner\":true", ",\"noArgs\":\"no\"", ",\"noArgs\":1", ",\"params\":{}", ",\"params\":5", ",\"params\":[5]",
        ",\"category\":null",
    };
    for (const char* extra : bad)
        EXPECT_EQ(ParseJson(RegJson(Info(d + extra))).op, SimRegistrationOp::Malformed) << extra;
}

TEST(SimRegistrationHelp, OverLimitOptionalStringsAreMalformed)
{
    using namespace Dia::DebugServer;
    const std::string d = "\"description\":\"d\"";
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"category\":" + Q(Fill('c', kSimHelpMaxCategory + 1))))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"owner\":" + Q(Fill('o', kSimHelpMaxOwner + 1))))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"category\":" + Q(Fill('c', kSimHelpMaxCategory))))).op, SimRegistrationOp::Register);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"owner\":" + Q(Fill('o', kSimHelpMaxOwner))))).op, SimRegistrationOp::Register);
}

TEST(SimRegistrationHelp, ParamValidation)
{
    using namespace Dia::DebugServer;
    const std::string d = "\"description\":\"d\"";
    auto param = [](const std::string& body) { return "{" + body + "}"; };
    auto withParams = [&](const std::string& list) { return ParseJson(RegJson(Info(d + ",\"params\":[" + list + "]"))).op; };
    const std::string ok = "\"name\":\"a\",\"type\":\"int\",\"required\":true";

    EXPECT_EQ(withParams(param(ok)), SimRegistrationOp::Register);
    EXPECT_EQ(withParams(param("\"type\":\"int\",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"\",\"type\":\"int\",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"a\",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"a\",\"type\":\"int\"")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":5,\"type\":\"int\",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"a\",\"type\":7,\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"a\",\"type\":\"int\",\"required\":\"yes\"")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"description\":4")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"unit\":4")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"enum\":4")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"min\":\"a\",\"max\":2")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"min\":1")), SimRegistrationOp::Malformed);   // min without max
    EXPECT_EQ(withParams(param(ok + ",\"min\":1,\"max\":2")), SimRegistrationOp::Register);

    EXPECT_EQ(withParams(param("\"name\":" + Q(Fill('n', kSimHelpMaxParamName + 1)) + ",\"type\":\"int\",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param("\"name\":\"a\",\"type\":" + Q(Fill('t', kSimHelpMaxParamType + 1)) + ",\"required\":true")), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"description\":" + Q(Fill('d', kSimHelpMaxParamDescription + 1)))), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"unit\":" + Q(Fill('u', kSimHelpMaxParamUnit + 1)))), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"enum\":" + Q(Fill('e', kSimHelpMaxParamEnum + 1)))), SimRegistrationOp::Malformed);
    EXPECT_EQ(withParams(param(ok + ",\"enum\":" + Q(Fill('e', kSimHelpMaxParamEnum)))), SimRegistrationOp::Register);
}

TEST(SimRegistrationHelp, SeventeenParamsIsMalformedSixteenIsFine)
{
    const std::string d = "\"description\":\"d\"";
    const std::string p = "{\"name\":\"a\",\"type\":\"int\",\"required\":false}";
    auto list = [&](int n) { std::string s; for (int i = 0; i < n; ++i) { if (i) s += ","; s += p; } return s; };
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"params\":[" + list(16) + "]"))).op, SimRegistrationOp::Register);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"params\":[" + list(17) + "]"))).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"params\":[" + list(17) + "]"), true)).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson(RegJson(Info(d + ",\"params\":[]"))).op, SimRegistrationOp::Register);
}

TEST(SimRegistrationHelp, ParseWithoutOutParamStillValidatesAndReportsHasInfo)
{
    auto stored = Stored(SimHelpInfo{ "hi" });
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "x", SimKind::Query, 1, stored.get(), *reply));
    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_TRUE(parsed.hasInfo);

    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Unregister, "x", SimKind::Query, 2, nullptr, *reply));
    EXPECT_FALSE(ParseSimRegistrationReply(*reply).hasInfo);
}

TEST(SimRegistrationHelp, MalformedInfoLeavesOutParamUntouched)
{
    SimHelpStored out;
    std::memset(&out, 0x77, sizeof(out));
    auto reply = MakeReply();
    std::strcpy(reply->json, RegJson(Info("\"description\":\"\"")).c_str());
    EXPECT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Malformed);
    EXPECT_EQ(static_cast<unsigned char>(out.description[0]), 0x77u);
}

// AC: 24 realistic entries (description ~100 chars, 3 params each) each fit one event.
TEST(SimRegistrationHelp, TwentyFourRealisticEntriesEachFitOneEvent)
{
    const std::string desc = Fill('x', 100);
    const Dia::API::ValueParam params[] = {
        { "cell", "int", true, "The cell index to operate on in the current layer", "m", true, 0.f, 1024.f, nullptr },
        { "mode", "enum", false, "Which mode to use when applying the change", nullptr, false, 0.f, 0.f, "weather|ecology|terrain" },
        { "label", "string", false, "Optional label recorded with the change", nullptr, false, 0.f, 0.f, nullptr },
    };
    auto stored = Stored(SimHelpInfo{ desc.c_str(), "cells", "DiaCellLayers", params, 3, false });
    for (int i = 0; i < 24; ++i)
    {
        const std::string name = "cell.layers.operation." + std::to_string(i);
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, name.c_str(), SimKind::Command, static_cast<uint32_t>(i + 1), stored.get(), *reply)) << i;
        EXPECT_LT(std::strlen(reply->json), static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes / 2));
        SimHelpStored out;
        std::memset(&out, 0, sizeof(out));
        ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
        ExpectSameHelp(*stored, out);
    }
}
