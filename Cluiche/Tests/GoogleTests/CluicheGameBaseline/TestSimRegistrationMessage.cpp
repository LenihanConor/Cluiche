#include <gtest/gtest.h>

#include <Types/SimRegistrationMessage.h>

#include <cstring>
#include <memory>

using namespace Cluiche::AppFlow;
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
}

TEST(SimRegistrationMessage, RegisterQueryRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.echo", SimKind::Query, *reply));
    EXPECT_EQ(reply->requestId, kSimRegistrationRequestId);

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "sim.echo");
    EXPECT_EQ(parsed.kind, SimKind::Query);
}

TEST(SimRegistrationMessage, RegisterCommandRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.reset", SimKind::Command, *reply));

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "sim.reset");
    EXPECT_EQ(parsed.kind, SimKind::Command);
}

TEST(SimRegistrationMessage, UnregisterRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Unregister, "sim.echo", SimKind::Query, *reply));

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Unregister);
    EXPECT_STREQ(parsed.name, "sim.echo");
}

TEST(SimRegistrationMessage, OrdinaryReplyIsNotARegistration)
{
    auto reply = MakeReply();
    reply->requestId = 7;
    std::strcpy(reply->json, "{\"register\":{\"name\":\"x\",\"kind\":\"query\"}}");

    EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::NotRegistration);
}

TEST(SimRegistrationMessage, MalformedPayloadsAreRejected)
{
    const char* bad[] = {
        "",
        "not json",
        "[]",
        "{}",
        "{\"register\":{\"name\":\"x\"}}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"banana\"}}",
        "{\"register\":{\"name\":\"\",\"kind\":\"query\"}}",
        "{\"register\":{\"kind\":\"query\"}}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"unregister\":{\"name\":\"x\"}}",
        "{\"unregister\":{}}",
    };
    for (const char* json : bad)
    {
        auto reply = MakeReply();
        std::strcpy(reply->json, json);
        EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::Malformed) << json;
    }
}

TEST(SimRegistrationMessage, UnterminatedPayloadIsMalformed)
{
    auto reply = MakeReply();
    std::memset(reply->json, '{', Dia::DebugServer::kSimPayloadBytes);
    EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::Malformed);
}

TEST(SimRegistrationMessage, BuildRejectsBadNames)
{
    auto reply = MakeReply();
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, nullptr, SimKind::Query, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "", SimKind::Query, *reply));

    char longName[kSimRegistrationMaxName + 8];
    std::memset(longName, 'a', sizeof(longName));
    longName[sizeof(longName) - 1] = '\0';
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, longName, SimKind::Query, *reply));

    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::NotRegistration, "x", SimKind::Query, *reply));
}

TEST(SimRegistrationMessage, NameWithQuoteSurvivesRoundTrip)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "a\"b", SimKind::Query, *reply));
    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "a\"b");
}
