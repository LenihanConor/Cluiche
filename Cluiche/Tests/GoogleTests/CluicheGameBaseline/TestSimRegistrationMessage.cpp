#include <gtest/gtest.h>

#include <Types/SimRegistrationMessage.h>

#include <cstring>
#include <memory>
#include <string>

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

    SimRegistration ParseJson(const char* json)
    {
        auto reply = MakeReply();
        std::strcpy(reply->json, json);
        return ParseSimRegistrationReply(*reply);
    }
}

TEST(SimRegistrationMessage, RegisterQueryRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.echo", SimKind::Query, 7, *reply));
    EXPECT_EQ(reply->requestId, kSimRegistrationRequestId);

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "sim.echo");
    EXPECT_EQ(parsed.kind, SimKind::Query);
    EXPECT_EQ(parsed.seq, 7u);
}

TEST(SimRegistrationMessage, RegisterCommandRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.reset", SimKind::Command, 1, *reply));

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "sim.reset");
    EXPECT_EQ(parsed.kind, SimKind::Command);
    EXPECT_EQ(parsed.seq, 1u);
}

TEST(SimRegistrationMessage, UnregisterRoundTrips)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Unregister, "sim.echo", SimKind::Query, 8, *reply));

    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Unregister);
    EXPECT_STREQ(parsed.name, "sim.echo");
    EXPECT_EQ(parsed.seq, 8u);
}

TEST(SimRegistrationMessage, SeqSurvivesRoundTripIncludingWrapValues)
{
    const uint32_t seqs[] = { 0u, 1u, 7u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (uint32_t seq : seqs)
    {
        for (SimRegistrationOp op : { SimRegistrationOp::Register, SimRegistrationOp::Unregister })
        {
            auto reply = MakeReply();
            ASSERT_TRUE(BuildSimRegistrationReply(op, "sim.echo", SimKind::Query, seq, *reply)) << seq;
            const SimRegistration parsed = ParseSimRegistrationReply(*reply);
            EXPECT_EQ(parsed.op, op) << seq;
            EXPECT_EQ(parsed.seq, seq) << reply->json;
        }
    }
}

TEST(SimRegistrationMessage, WireFormatMatchesSpec)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "sim.echo", SimKind::Query, 7, *reply));
    const SimRegistration parsed = ParseJson(reply->json);
    EXPECT_NE(std::string(reply->json).find("\"seq\":7"), std::string::npos) << reply->json;
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
}

TEST(SimRegistrationMessage, RegisterWithoutSeqIsMalformed)
{
    EXPECT_EQ(ParseJson("{\"register\":{\"name\":\"x\",\"kind\":\"query\"}}").op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson("{\"unregister\":{\"name\":\"x\"}}").op, SimRegistrationOp::Malformed);
    EXPECT_EQ(ParseJson("{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"replay\":false}").op, SimRegistrationOp::Malformed);
}

TEST(SimRegistrationMessage, BadSeqIsMalformed)
{
    const char* bad[] = {
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":-1}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":4294967296}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":\"7\"}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":1.5}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":null}",
        "{\"unregister\":{\"name\":\"x\"},\"seq\":-3}",
        "{\"unregister\":{\"name\":\"x\"},\"seq\":true}",
    };
    for (const char* json : bad)
        EXPECT_EQ(ParseJson(json).op, SimRegistrationOp::Malformed) << json;
}

TEST(SimRegistrationMessage, ReplayWithoutSeqParsesAsReplay)
{
    const SimRegistration q = ParseJson("{\"register\":{\"name\":\"sim.echo\",\"kind\":\"query\"},\"replay\":true}");
    EXPECT_EQ(q.op, SimRegistrationOp::Replay);
    EXPECT_STREQ(q.name, "sim.echo");
    EXPECT_EQ(q.kind, SimKind::Query);
    EXPECT_EQ(q.seq, 0u);

    const SimRegistration c = ParseJson("{\"register\":{\"name\":\"sim.reset\",\"kind\":\"command\"},\"replay\":true}");
    EXPECT_EQ(c.op, SimRegistrationOp::Replay);
    EXPECT_EQ(c.kind, SimKind::Command);
}

TEST(SimRegistrationMessage, ReplayRoundTrips)
{
    for (SimKind kind : { SimKind::Query, SimKind::Command })
    {
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimReplayReply("sim.echo", kind, *reply));
        EXPECT_EQ(reply->requestId, kSimRegistrationRequestId);
        const SimRegistration parsed = ParseSimRegistrationReply(*reply);
        EXPECT_EQ(parsed.op, SimRegistrationOp::Replay);
        EXPECT_STREQ(parsed.name, "sim.echo");
        EXPECT_EQ(parsed.kind, kind);
        EXPECT_EQ(parsed.seq, 0u);
    }
}

TEST(SimRegistrationMessage, ReplayBuildRejectsBadNames)
{
    auto reply = MakeReply();
    EXPECT_FALSE(BuildSimReplayReply(nullptr, SimKind::Query, *reply));
    EXPECT_FALSE(BuildSimReplayReply("", SimKind::Query, *reply));
    char longName[kSimRegistrationMaxName + 8];
    std::memset(longName, 'a', sizeof(longName));
    longName[sizeof(longName) - 1] = '\0';
    EXPECT_FALSE(BuildSimReplayReply(longName, SimKind::Query, *reply));
}

TEST(SimRegistrationMessage, MalformedReplayVariants)
{
    const char* bad[] = {
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"replay\":\"yes\"}",
        "{\"register\":{\"name\":\"x\"},\"replay\":true}",
        "{\"register\":{\"kind\":\"query\"},\"replay\":true}",
        "{\"unregister\":{\"name\":\"x\"},\"replay\":true}",
        "{\"replay\":true}",
    };
    for (const char* json : bad)
        EXPECT_EQ(ParseJson(json).op, SimRegistrationOp::Malformed) << json;
}

TEST(SimRegistrationMessage, DigestRoundTrips)
{
    struct Case { uint32_t seq, count, hash; };
    const Case cases[] = {
        { 8, 12, 0x1a2b3c4du },
        { 0, 0, 0u },
        { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu },
        { 1, 1, 0x80000000u },
        { 0x80000000u, 5, 0x0000000Fu },
    };
    for (const Case& c : cases)
    {
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimDigestReply(c.seq, c.count, c.hash, *reply));
        EXPECT_EQ(reply->requestId, kSimRegistrationRequestId);
        const SimRegistration parsed = ParseSimRegistrationReply(*reply);
        EXPECT_EQ(parsed.op, SimRegistrationOp::Digest) << reply->json;
        EXPECT_EQ(parsed.seq, c.seq);
        EXPECT_EQ(parsed.count, c.count);
        EXPECT_EQ(parsed.hash, c.hash) << reply->json;
    }
}

TEST(SimRegistrationMessage, DigestHashIsHexString)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimDigestReply(8, 12, 0x1a2b3c4du, *reply));
    EXPECT_NE(std::string(reply->json).find("\"hash\":\"0x1a2b3c4d\""), std::string::npos) << reply->json;
    ASSERT_TRUE(BuildSimDigestReply(8, 12, 0x2u, *reply));
    EXPECT_NE(std::string(reply->json).find("\"hash\":\"0x00000002\""), std::string::npos) << reply->json;
}

TEST(SimRegistrationMessage, MalformedDigestVariants)
{
    const char* bad[] = {
        "{\"digest\":{}}",
        "{\"digest\":5}",
        "{\"digest\":{\"count\":12,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12}}",
        "{\"digest\":{\"seq\":-1,\"count\":12,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":-12,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":4294967296,\"count\":12,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":4294967296,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":\"8\",\"count\":12,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":1.5,\"hash\":\"0x1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":439041101}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"1a2b3c4d\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0xZZ\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d5\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d \"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x-1a2b3c\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"\"}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d\"},\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":1}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d\"},\"resync_begin\":{\"seq\":8,\"count\":12}}",
        "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d\"},\"unregister\":{\"name\":\"x\"},\"seq\":1}",
    };
    for (const char* json : bad)
        EXPECT_EQ(ParseJson(json).op, SimRegistrationOp::Malformed) << json;
}

TEST(SimRegistrationMessage, ResyncBeginRoundTrips)
{
    const uint32_t vals[] = { 0u, 1u, 12u, 0x80000000u, 0xFFFFFFFFu };
    for (uint32_t v : vals)
    {
        auto reply = MakeReply();
        ASSERT_TRUE(BuildSimResyncBeginReply(v, 3, *reply));
        EXPECT_EQ(reply->requestId, kSimRegistrationRequestId);
        const SimRegistration parsed = ParseSimRegistrationReply(*reply);
        EXPECT_EQ(parsed.op, SimRegistrationOp::ResyncBegin) << reply->json;
        EXPECT_EQ(parsed.seq, v);
        EXPECT_EQ(parsed.count, 3u);

        ASSERT_TRUE(BuildSimResyncBeginReply(8, v, *reply));
        const SimRegistration parsed2 = ParseSimRegistrationReply(*reply);
        EXPECT_EQ(parsed2.op, SimRegistrationOp::ResyncBegin);
        EXPECT_EQ(parsed2.count, v);
    }
}

TEST(SimRegistrationMessage, MalformedResyncBeginVariants)
{
    const char* bad[] = {
        "{\"resync_begin\":{}}",
        "{\"resync_begin\":[]}",
        "{\"resync_begin\":{\"seq\":8}}",
        "{\"resync_begin\":{\"count\":12}}",
        "{\"resync_begin\":{\"seq\":-8,\"count\":12}}",
        "{\"resync_begin\":{\"seq\":8,\"count\":-12}}",
        "{\"resync_begin\":{\"seq\":8,\"count\":4294967296}}",
        "{\"resync_begin\":{\"seq\":\"8\",\"count\":12}}",
        "{\"resync_begin\":{\"seq\":8,\"count\":12.5}}",
        "{\"resync_begin\":{\"seq\":8,\"count\":12},\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"replay\":true}",
    };
    for (const char* json : bad)
        EXPECT_EQ(ParseJson(json).op, SimRegistrationOp::Malformed) << json;
}

TEST(SimRegistrationMessage, OrdinaryReplyIsNotARegistration)
{
    auto reply = MakeReply();
    reply->requestId = 7;
    std::strcpy(reply->json, "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"seq\":1}");
    EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::NotRegistration);

    std::strcpy(reply->json, "{\"digest\":{\"seq\":8,\"count\":12,\"hash\":\"0x1a2b3c4d\"}}");
    EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::NotRegistration);
}

TEST(SimRegistrationMessage, MalformedPayloadsAreRejected)
{
    const char* bad[] = {
        "",
        "not json",
        "[]",
        "{}",
        "{\"register\":{\"name\":\"x\"},\"seq\":1}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"banana\"},\"seq\":1}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"resync\"},\"seq\":1}",
        "{\"register\":{\"name\":\"\",\"kind\":\"query\"},\"seq\":1}",
        "{\"register\":{\"kind\":\"query\"},\"seq\":1}",
        "{\"register\":{\"name\":\"x\",\"kind\":\"query\"},\"unregister\":{\"name\":\"x\"},\"seq\":1}",
        "{\"unregister\":{},\"seq\":1}",
    };
    for (const char* json : bad)
        EXPECT_EQ(ParseJson(json).op, SimRegistrationOp::Malformed) << json;
}

TEST(SimRegistrationMessage, UnterminatedPayloadIsMalformed)
{
    auto reply = MakeReply();
    std::memset(reply->json, '{', Dia::DebugServer::kSimPayloadBytes);
    EXPECT_EQ(ParseSimRegistrationReply(*reply).op, SimRegistrationOp::Malformed);
}

TEST(SimRegistrationMessage, BuildRejectsBadNamesAndOps)
{
    auto reply = MakeReply();
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, nullptr, SimKind::Query, 1, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, "", SimKind::Query, 1, *reply));

    char longName[kSimRegistrationMaxName + 8];
    std::memset(longName, 'a', sizeof(longName));
    longName[sizeof(longName) - 1] = '\0';
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Register, longName, SimKind::Query, 1, *reply));

    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::NotRegistration, "x", SimKind::Query, 1, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Malformed, "x", SimKind::Query, 1, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Digest, "x", SimKind::Query, 1, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::ResyncBegin, "x", SimKind::Query, 1, *reply));
    EXPECT_FALSE(BuildSimRegistrationReply(SimRegistrationOp::Replay, "x", SimKind::Query, 1, *reply));
}

TEST(SimRegistrationMessage, NameWithQuoteSurvivesRoundTrip)
{
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimRegistrationReply(SimRegistrationOp::Register, "a\"b", SimKind::Query, 2, *reply));
    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.op, SimRegistrationOp::Register);
    EXPECT_STREQ(parsed.name, "a\"b");
}

TEST(SimNameSetHash, EntryHashIsDeterministicAndKindSensitive)
{
    EXPECT_EQ(SimNameSetEntryHash("sim.echo", SimKind::Query), SimNameSetEntryHash("sim.echo", SimKind::Query));
    EXPECT_NE(SimNameSetEntryHash("sim.echo", SimKind::Query), SimNameSetEntryHash("sim.echo", SimKind::Command));
    EXPECT_NE(SimNameSetEntryHash("sim.echo", SimKind::Query), SimNameSetEntryHash("sim.echo2", SimKind::Query));
}

TEST(SimNameSetHash, SetHashIsOrderIndependent)
{
    const char* names[] = { "a.one", "b.two", "c.three", "d.four" };
    const SimKind kinds[] = { SimKind::Query, SimKind::Command, SimKind::Query, SimKind::Command };

    uint32_t forward = 0;
    for (int i = 0; i < 4; ++i)
        forward += SimNameSetEntryHash(names[i], kinds[i]);
    uint32_t reverse = 0;
    for (int i = 3; i >= 0; --i)
        reverse += SimNameSetEntryHash(names[i], kinds[i]);
    uint32_t shuffled = 0;
    const int order[] = { 2, 0, 3, 1 };
    for (int i : order)
        shuffled += SimNameSetEntryHash(names[i], kinds[i]);

    EXPECT_EQ(forward, reverse);
    EXPECT_EQ(forward, shuffled);
}

TEST(SimNameSetHash, DifferentSetsDiffer)
{
    const uint32_t a = SimNameSetEntryHash("a", SimKind::Query) + SimNameSetEntryHash("b", SimKind::Query);
    const uint32_t b = SimNameSetEntryHash("a", SimKind::Query) + SimNameSetEntryHash("c", SimKind::Query);
    const uint32_t c = SimNameSetEntryHash("a", SimKind::Query);
    const uint32_t d = SimNameSetEntryHash("a", SimKind::Query) + SimNameSetEntryHash("b", SimKind::Command);
    EXPECT_NE(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(SimNameSetHash, WrappingSumIsLossless)
{
    // Sum of many entries wraps in uint32; round-trips through a digest unchanged.
    uint32_t sum = 0;
    for (int i = 0; i < 64; ++i)
        sum += SimNameSetEntryHash(("sim.name" + std::to_string(i)).c_str(), (i & 1) ? SimKind::Command : SimKind::Query);
    auto reply = MakeReply();
    ASSERT_TRUE(BuildSimDigestReply(0xFFFFFFFFu, 64, sum, *reply));
    const SimRegistration parsed = ParseSimRegistrationReply(*reply);
    EXPECT_EQ(parsed.hash, sum);
    EXPECT_EQ(parsed.seq, 0xFFFFFFFFu);
}
