#include <gtest/gtest.h>

#include <Types/SimRegistrationMessage.h>
#include <DiaDebugServer/SimHelpInfo.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <utility>
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

// ---- Exhaustive pass (Task 7) ----

namespace
{
    // Builds a Register message for 'stored', parses it back, and expects the identical help (and again as a replay).
    void ExpectRoundTrip(const SimHelpStored& stored, const char* label)
    {
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "esc.n", SimKind::Command, 3, &stored, *reply)) << label;
        SimHelpStored out;
        std::memset(&out, 0, sizeof(out));
        ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register) << label << ": " << reply->json;
        ExpectSameHelp(stored, out);

        auto replay = MakeReply();
        ASSERT_TRUE(BuildSimReplayReply("esc.n", SimKind::Query, &stored, *replay)) << label;
        std::memset(&out, 0, sizeof(out));
        ASSERT_EQ(ParseSimRegistrationReply(*replay, &out).op, SimRegistrationOp::Replay) << label;
        ExpectSameHelp(stored, out);
    }
}

TEST(SimRegistrationHelp, TrickyTextRoundTripsThroughJsonEscapingInEveryStringField)
{
    std::string allControl;
    for (char c = 1; c < 0x20; ++c) allControl += c;
    const std::vector<std::pair<const char*, std::string>> cases = {
        { "quotes",      "say \"hi\" and 'bye'" },
        { "backslashes", "C:\\dir\\file \\n \\\\ \\\"" },
        { "literal-escapes", "\\u0041 \\u00e9 \\/ \\b" },
        { "newlines",    "line1\nline2\r\nline3\ttabbed" },
        { "all-control", allControl },
        { "del",         "a\x7f" "b" },
        { "unicode",     "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" },
        { "emoji",       "ok \xF0\x9F\x98\x80 \xF0\x9F\x9A\x80 done" },
        { "html",        "<script>alert('x')</script> & </tag>" },
        { "json-lookalike", "{\"register\":{\"name\":\"evil\"},\"seq\":1}" },
        { "pipes",       "a|b|c|\"d\"|\\e" },
        { "invalid-utf8", "bad \xFF\xFE bytes" },
    };
    for (const auto& c : cases)
    {
        const std::string& t = c.second;
        ASSERT_LE(t.size(), 64u) << c.first;   // every field stays inside its byte limit
        const std::string unit = t.substr(0, Dia::DebugServer::kSimHelpMaxParamUnit);
        const Dia::API::ValueParam p = { t.c_str(), unit.c_str(), true, t.c_str(), unit.c_str(), true, -1.0f, 1.0f, t.c_str() };
        auto stored = Stored(SimHelpInfo{ t.c_str(), t.c_str(), t.c_str(), &p, 1, false });
        ExpectRoundTrip(*stored, c.first);
    }
}

TEST(SimRegistrationHelp, MaximalLengthEscapeHeavyFieldsRoundTripWhenTheyFit)
{
    using namespace Dia::DebugServer;
    // Backslash and quote double in size when escaped; the message still fits for a single param.
    const std::string desc = Fill('\\', kSimHelpMaxDescription), cat = Fill('"', kSimHelpMaxCategory), owner = Fill('\\', kSimHelpMaxOwner);
    const std::string pn = Fill('"', kSimHelpMaxParamName), pd = Fill('\\', kSimHelpMaxParamDescription), pe = Fill('"', kSimHelpMaxParamEnum);
    const Dia::API::ValueParam p = { pn.c_str(), "string", false, pd.c_str(), nullptr, false, 0.f, 0.f, pe.c_str() };
    auto stored = Stored(SimHelpInfo{ desc.c_str(), cat.c_str(), owner.c_str(), &p, 1, true });
    ExpectRoundTrip(*stored, "escape-heavy");
}

TEST(SimRegistrationHelp, NullAndEmptyOptionalFieldsEncodeToTheSameMessage)
{
    const Dia::API::ValueParam pNull = { "p", "int", false, nullptr };
    const Dia::API::ValueParam pEmpty = { "p", "int", false, "", "", false, 0.f, 0.f, "" };
    auto a = Stored(SimHelpInfo{ "d", nullptr, nullptr, &pNull, 1, false });
    auto b = Stored(SimHelpInfo{ "d", "", "", &pEmpty, 1, false });
    auto ra = MakeReply(), rb = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "n", SimKind::Query, 1, a.get(), *ra));
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "n", SimKind::Query, 1, b.get(), *rb));
    EXPECT_STREQ(ra->json, rb->json);
    EXPECT_EQ(std::string(ra->json).find("category"), std::string::npos);
}

TEST(SimRegistrationHelp, EmptyStringOptionalFieldsInAMessageParseAsEmpty)
{
    SimHelpStored out;
    std::memset(&out, 0x55, sizeof(out));
    auto reply = MakeReply();
    std::strcpy(reply->json, RegJson(Info("\"description\":\"d\",\"category\":\"\",\"owner\":\"\",\"params\":[{\"name\":\"a\",\"type\":\"\",\"required\":false,\"description\":\"\",\"unit\":\"\",\"enum\":\"\"}]")).c_str());
    ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
    EXPECT_STREQ(out.category, "");
    EXPECT_STREQ(out.owner, "");
    EXPECT_STREQ(out.params[0].type, "");
    EXPECT_STREQ(out.params[0].description, "");
    EXPECT_STREQ(out.params[0].enumValues, "");
}

TEST(SimRegistrationHelp, DuplicateParamNamesRoundTripInOrder)
{
    const Dia::API::ValueParam params[2] = { { "same", "int", true, "first" }, { "same", "float", false, "second" } };
    auto stored = Stored(SimHelpInfo{ "d", nullptr, nullptr, params, 2, false });
    ExpectRoundTrip(*stored, "dup-params");
}

TEST(SimRegistrationHelp, BuildRejectsNonFiniteAndInvertedRangesWithoutTouchingOut)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float ranges[][2] = { { nan, 1.f }, { 0.f, nan }, { 0.f, inf }, { -inf, 0.f }, { 5.f, 1.f } };
    for (const auto& r : ranges)
    {
        // CopyHelp rejects these, so craft the stored value by hand to prove the encoder is a second line of defence.
        auto stored = Stored(SimHelpInfo{ "d" });
        stored->paramCount = 1;
        std::strcpy(stored->params[0].name, "p");
        std::strcpy(stored->params[0].type, "float");
        stored->params[0].hasRange = true;
        stored->params[0].min = r[0];
        stored->params[0].max = r[1];
        auto reply = MakeReply();
        std::memset(reply.get(), 0x5A, sizeof(SimReplyEvent));
        EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "n", SimKind::Query, 1, stored.get(), *reply)) << r[0] << " " << r[1];
        EXPECT_FALSE(BuildSimReplayReply("n", SimKind::Query, stored.get(), *reply));
        EXPECT_EQ(reply->json[0], 0x5A);
    }
}

TEST(SimRegistrationHelp, ParseRejectsInvertedOrNonFiniteRanges)
{
    const std::string d = "\"description\":\"d\"";
    const std::string p = "{\"name\":\"a\",\"type\":\"float\",\"required\":false,";
    auto parse = [&](const std::string& range) { return ParseJson(RegJson(Info(d + ",\"params\":[" + p + range + "}]"))).op; };
    EXPECT_EQ(parse("\"min\":5,\"max\":1"), SimRegistrationOp::Malformed);
    EXPECT_EQ(parse("\"min\":1,\"max\":1"), SimRegistrationOp::Register);
    EXPECT_EQ(parse("\"min\":-1,\"max\":1"), SimRegistrationOp::Register);
    EXPECT_EQ(parse("\"min\":0,\"max\":1e300"), SimRegistrationOp::Malformed);      // finite double, overflows float
    EXPECT_EQ(parse("\"min\":-1e300,\"max\":0"), SimRegistrationOp::Malformed);
    EXPECT_EQ(parse("\"min\":0,\"max\":1e999"), SimRegistrationOp::Malformed);      // parses to infinity
    EXPECT_EQ(parse("\"min\":0,\"max\":NaN"), SimRegistrationOp::Malformed);
    EXPECT_EQ(parse("\"min\":0,\"max\":Infinity"), SimRegistrationOp::Malformed);
    EXPECT_EQ(parse("\"min\":null,\"max\":1"), SimRegistrationOp::Malformed);
}

// The 16 KB event boundary: find the first size that does not fit; one control char less must build, this must not.
namespace
{
    std::unique_ptr<SimHelpStored> BoundaryInfo(uint32_t extraControlChars, std::string& storage)
    {
        // 16 params whose description holds 'extraControlChars' control characters (6 bytes each once escaped).
        storage.assign(extraControlChars, '\x01');
        std::vector<Dia::API::ValueParam> params(Dia::DebugServer::kSimHelpMaxParams);
        for (auto& p : params)
            p = { "n", "string", false, storage.c_str(), nullptr, false, 0.f, 0.f, nullptr };
        const std::string desc(Dia::DebugServer::kSimHelpMaxDescription, '\x01');
        return Stored(SimHelpInfo{ desc.c_str(), nullptr, nullptr, params.data(), Dia::DebugServer::kSimHelpMaxParams, false });
    }
}

TEST(SimRegistrationHelp, SizeRightAtTheEventBoundaryBuildsAndOneControlCharMoreIsRejected)
{
    std::string storage;
    uint32_t firstFail = 0;
    for (uint32_t n = 0; n <= Dia::DebugServer::kSimHelpMaxParamDescription; ++n)
    {
        auto reply = MakeReply();
        auto info = BoundaryInfo(n, storage);
        if (!BuildSimRegistrationReply(SimRegistrationOp::Register, "edge", SimKind::Query, 1, info.get(), *reply)) { firstFail = n; break; }
    }
    ASSERT_GT(firstFail, 0u);

    {
        auto info = BoundaryInfo(firstFail - 1, storage);
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "edge", SimKind::Query, 1, info.get(), *reply));
        EXPECT_LT(std::strlen(reply->json), static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
        EXPECT_GT(std::strlen(reply->json), static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes) - 16 * 6);   // really at the edge
        SimHelpStored out;
        std::memset(&out, 0, sizeof(out));
        ASSERT_EQ(ParseSimRegistrationReply(*reply, &out).op, SimRegistrationOp::Register);
        ExpectSameHelp(*info, out);
    }
    {
        auto info = BoundaryInfo(firstFail, storage);
        auto reply = MakeReply();
        EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "edge", SimKind::Query, 1, info.get(), *reply));
        EXPECT_FALSE(BuildSimReplayReply("edge", SimKind::Query, info.get(), *reply));
        EXPECT_EQ(reply->json[0], '\0');
    }
}

// Fuzz: randomly corrupted register / replay messages never crash and never throw; they are Malformed or valid. A valid
// result has a fully bounded, NUL-terminated info; a Malformed one leaves the caller's buffer untouched.
TEST(SimRegistrationHelp, FuzzedCorruptedInfoNeverCrashesOrThrows)
{
    using namespace Dia::DebugServer;
    const Dia::API::ValueParam params[] = {
        { "cell", "int", true, "The cell index", "m", true, 0.f, 1024.f, nullptr },
        { "mode", "enum", false, "Which mode \"quoted\"", nullptr, false, 0.f, 0.f, "weather|ecology" },
    };
    auto stored = Stored(SimHelpInfo{ "does a thing", "cells", "DiaCellLayers", params, 2, true });
    auto base = MakeReply(), baseReplay = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "fuzz.n", SimKind::Command, 9, stored.get(), *base));
    ASSERT_TRUE(BuildSimReplayReply("fuzz.n", SimKind::Query, stored.get(), *baseReplay));
    const std::string seeds[] = { base->json, baseReplay->json };
    const char junk[] = { '{', '}', '[', ']', '"', '\\', ':', ',', '0', '-', 'e', '.', 'n', 't', 'f', '\x01', '\xFF', '/' };

    std::mt19937 rng(0xC0FFEEu);
    unsigned valid = 0, malformed = 0;
    for (unsigned iter = 0; iter < 30000; ++iter)
    {
        std::string m = seeds[iter % 2];
        const unsigned edits = 1 + rng() % 4;
        for (unsigned e = 0; e < edits && !m.empty(); ++e)
        {
            const size_t pos = rng() % m.size();
            switch (rng() % 5)
            {
            case 0: m[pos] = junk[rng() % sizeof(junk)]; break;                       // overwrite
            case 1: m.erase(pos, 1 + rng() % 8); break;                               // delete a run
            case 2: m.insert(pos, 1, junk[rng() % sizeof(junk)]); break;              // insert
            case 3: m.resize(pos); break;                                             // truncate
            default: m[pos] = static_cast<char>(rng() & 0xFF); break;                 // random byte
            }
        }
        if (m.size() >= kSimPayloadBytes) m.resize(kSimPayloadBytes - 1);

        auto reply = MakeReply();
        std::memcpy(reply->json, m.data(), m.size());
        SimHelpStored out;
        std::memset(&out, 0x77, sizeof(out));
        SimRegistration r;
        ASSERT_NO_THROW(r = ParseSimRegistrationReply(*reply, &out)) << m;

        if (r.op == SimRegistrationOp::Register || r.op == SimRegistrationOp::Replay)
        {
            ++valid;
            ASSERT_TRUE(r.hasInfo) << m;
            ASSERT_LE(std::strlen(r.name), static_cast<size_t>(kSimRegistrationMaxName - 1));
            ASSERT_NE(out.description[0], '\0') << m;
            ASSERT_LE(std::strlen(out.description), static_cast<size_t>(kSimHelpMaxDescription));
            ASSERT_LE(std::strlen(out.category), static_cast<size_t>(kSimHelpMaxCategory));
            ASSERT_LE(std::strlen(out.owner), static_cast<size_t>(kSimHelpMaxOwner));
            ASSERT_LE(out.paramCount, kSimHelpMaxParams);
            for (uint32_t i = 0; i < out.paramCount; ++i)
            {
                ASSERT_NE(out.params[i].name[0], '\0') << m;
                ASSERT_LE(std::strlen(out.params[i].name), static_cast<size_t>(kSimHelpMaxParamName));
                ASSERT_LE(std::strlen(out.params[i].type), static_cast<size_t>(kSimHelpMaxParamType));
                ASSERT_LE(std::strlen(out.params[i].enumValues), static_cast<size_t>(kSimHelpMaxParamEnum));
                if (out.params[i].hasRange)
                {
                    ASSERT_TRUE(std::isfinite(out.params[i].min) && std::isfinite(out.params[i].max)) << m;
                    ASSERT_LE(out.params[i].min, out.params[i].max) << m;
                }
            }
        }
        else if (r.op == SimRegistrationOp::Malformed)
        {
            ++malformed;
            ASSERT_EQ(static_cast<unsigned char>(out.description[0]), 0x77u) << m;
        }
    }
    EXPECT_GT(valid, 100u);       // the corpus really exercises both outcomes
    EXPECT_GT(malformed, 100u);
}

TEST(SimRegistrationHelp, DeeplyNestedInfoJunkIsMalformedNotAnException)
{
    std::string deep = "{\"register\":{\"name\":\"x\",\"kind\":\"query\",\"info\":";
    deep += std::string(5000, '[');
    EXPECT_EQ(ParseJson(deep).op, SimRegistrationOp::Malformed);
    std::string objs = "{\"register\":{\"name\":\"x\",\"kind\":\"query\",\"info\":";
    for (int i = 0; i < 3000; ++i) objs += "{\"a\":";
    EXPECT_EQ(ParseJson(objs).op, SimRegistrationOp::Malformed);
}

// Byte-exact cap: a message of exactly kSimPayloadBytes - 1 characters (plus the NUL) builds, one byte more is rejected.
TEST(SimRegistrationHelp, ByteExactEventCapBoundaryIsHonoured)
{
    using namespace Dia::DebugServer;
    auto build = [](uint32_t controlChars, uint32_t descChars, std::unique_ptr<SimReplyEvent>& reply)
    {
        const std::string ctl(controlChars, '\x01'), desc(descChars, 'd');
        std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
        for (auto& p : params)
            p = { "n", "string", false, ctl.c_str(), nullptr, false, 0.f, 0.f, nullptr };
        auto stored = Stored(SimHelpInfo{ desc.c_str(), nullptr, nullptr, params.data(), kSimHelpMaxParams, false });
        reply = MakeReply();
        return BuildSimRegistrationReply(SimRegistrationOp::Register, "edge", SimKind::Query, 1, stored.get(), *reply);
    };
    std::unique_ptr<SimReplyEvent> reply;
    uint32_t n = 0;
    while (n < kSimHelpMaxParamDescription && build(n + 1, 1, reply)) ++n;
    ASSERT_GT(n, 0u);
    ASSERT_LT(n, kSimHelpMaxParamDescription);
    uint32_t k = 1;
    while (k < kSimHelpMaxDescription && build(n, k + 1, reply)) ++k;
    ASSERT_LT(k, kSimHelpMaxDescription);

    ASSERT_TRUE(build(n, k, reply));
    EXPECT_EQ(std::strlen(reply->json), static_cast<size_t>(kSimPayloadBytes - 1));   // exactly at the cap
    EXPECT_FALSE(build(n, k + 1, reply));                                              // one byte over
    EXPECT_EQ(reply->json[0], '\0');
}
