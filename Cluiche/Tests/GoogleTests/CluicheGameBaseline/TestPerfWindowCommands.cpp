////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfWindowCommands.cpp
// GoogleTest suite - Perf Inspector task 2b: perf.window / perf.frame (Main-hosted),
// draining the "$perfcoarse" EventStream into a CoarseChunkAssembler.  All builds.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfWindowCommands.h>

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;
using Cluiche::AppFlow::PerfWindowCommands;
using Dia::ApplicationFlow::Event;
using Dia::ApplicationFlow::EventStreamStore;
using Dia::Core::StringCRC;

namespace
{
    std::vector<uint8_t> DecodeBase64(const std::string& in)
    {
        std::vector<uint8_t> out;
        uint32_t acc = 0;
        int bits = 0;
        for (char c : in)
        {
            int v;
            if (c >= 'A' && c <= 'Z') v = c - 'A';
            else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
            else if (c >= '0' && c <= '9') v = c - '0' + 52;
            else if (c == '+') v = 62;
            else if (c == '/') v = 63;
            else break;
            acc = (acc << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8) { bits -= 8; out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF)); }
        }
        return out;
    }

    std::vector<uint16_t> DecodeU16(const std::string& in)
    {
        std::vector<uint8_t> raw = DecodeBase64(in);
        std::vector<uint16_t> out(raw.size() / 2);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
        return out;
    }

    struct PerfWindowRig : ::testing::Test
    {
        EventStreamStore<CoarseChunk> store{ StringCRC("test_perfwindow"), StringCRC("CoarseChunk"), 16, 1 };
        PerfWindowCommands cmds{ &store, nullptr };
        struct UnitCursor { uint32_t seq = 0; uint64_t nextEntry = 0; };
        std::map<uint32_t, UnitCursor> cursors;

        // Sends one chunk of n entries for the unit; module 0xA = 1.00 ms, unit tick = 2.00 ms.
        void SendChunk(uint32_t unitCrc, float hz, uint32_t n)
        {
            CoarseChunkBuilder builder;
            builder.Begin(unitCrc, hz);
            for (uint32_t i = 0; i < n; ++i)
            {
                builder.RecordModule(0, 0xAu, 1.0f);
                builder.RecordUnitTick(2.0f);
                builder.CommitEntry(0.0);
            }
            CoarseChunk chunk = builder.Take(1000.0);
            UnitCursor& cur = cursors[unitCrc];
            chunk.seq = cur.seq++;
            chunk.firstEntry = cur.nextEntry;
            cur.nextEntry += n;
            Event<CoarseChunk> ev;
            ev.payload = chunk;
            store.Send(ev);
        }

        static Json::Value Args(float seconds) { Json::Value a(Json::objectValue); a["seconds"] = seconds; return a; }
        static Json::Value FrameArgs(const char* unit, uint64_t frame)
        {
            Json::Value a(Json::objectValue);
            if (unit) a["unit"] = unit;
            a["frame"] = static_cast<Json::UInt64>(frame);
            return a;
        }
    };
}

TEST_F(PerfWindowRig, DrainAppliesPendingChunks)
{
    SendChunk(0x11u, 30.0f, 10);
    SendChunk(0x11u, 30.0f, 10);
    SendChunk(0x22u, 60.0f, 5);
    EXPECT_EQ(0u, cmds.GetAssembler().GetUnitCount());
    cmds.Drain();
    EXPECT_EQ(2u, cmds.GetAssembler().GetUnitCount());
    ASSERT_NE(nullptr, cmds.GetAssembler().GetRing(0x11u));
    EXPECT_EQ(20u, cmds.GetAssembler().GetRing(0x11u)->GetEntryCount());
    EXPECT_EQ(5u, cmds.GetAssembler().GetRing(0x22u)->GetEntryCount());
    cmds.Drain();   // nothing pending: no change
    EXPECT_EQ(20u, cmds.GetAssembler().GetRing(0x11u)->GetEntryCount());
}

TEST_F(PerfWindowRig, DrainWithNullStoreIsHarmless)
{
    PerfWindowCommands none(nullptr, nullptr);
    none.Drain();
    EXPECT_EQ(0u, none.GetAssembler().GetUnitCount());
}

TEST_F(PerfWindowRig, WindowWithNoUnitsIsValidEmptyReply)
{
    const Json::Value r = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_FALSE(r.isMember("error"));
    ASSERT_TRUE(r["units"].isArray());
    EXPECT_EQ(0u, r["units"].size());
}

TEST_F(PerfWindowRig, WindowReturnsDecodableArrays)
{
    SendChunk(0x11u, 30.0f, 20);
    cmds.Drain();
    const Json::Value r = cmds.HandleWindow(Args(60.0f));
    ASSERT_EQ(1u, r["units"].size());
    const Json::Value& u = r["units"][0];
    EXPECT_EQ(0x11u, u["unitCrc"].asUInt());
    EXPECT_EQ(20u, u["count"].asUInt());
    EXPECT_EQ("0x00000011", u["unit"].asString());   // no app: hex fallback
    const std::vector<uint16_t> unitMs = DecodeU16(u["unitMs"].asString());
    ASSERT_EQ(20u, unitMs.size());
    EXPECT_EQ(200u, unitMs[0]);
    const std::vector<uint16_t> moduleMs = DecodeU16(u["moduleMs"].asString());
    EXPECT_EQ(20u * u["modules"].size(), moduleMs.size());
    EXPECT_EQ(100u, moduleMs[0]);
}

TEST_F(PerfWindowRig, WindowSecondsDefaultsTo60AndClamps)
{
    SendChunk(0x11u, 10.0f, 64);
    SendChunk(0x11u, 10.0f, 64);
    cmds.Drain();
    // 128 entries at 10 Hz: default (60 s) returns all
    EXPECT_EQ(128u, cmds.HandleWindow(Json::Value(Json::objectValue))["units"][0]["count"].asUInt());
    // 2 s at 10 Hz = 20 entries
    const Json::Value two = cmds.HandleWindow(Args(2.0f));
    EXPECT_EQ(20u, two["units"][0]["count"].asUInt());
    EXPECT_EQ(108u, two["units"][0]["firstEntry"].asUInt());
    // below 1 clamps to 1 s = 10 entries; above 60 clamps to 60
    EXPECT_EQ(10u, cmds.HandleWindow(Args(0.0f))["units"][0]["count"].asUInt());
    EXPECT_EQ(128u, cmds.HandleWindow(Args(1000.0f))["units"][0]["count"].asUInt());
    // wrong type falls back to the default
    Json::Value bad(Json::objectValue);
    bad["seconds"] = "soon";
    EXPECT_EQ(128u, cmds.HandleWindow(bad)["units"][0]["count"].asUInt());
}

TEST_F(PerfWindowRig, FrameKnownUnitReportsModuleAndUnitMs)
{
    SendChunk(0x11u, 30.0f, 10);
    cmds.Drain();
    const Json::Value r = cmds.HandleFrame(FrameArgs("0x00000011", 4));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_EQ(4u, r["frame"].asUInt64());
    EXPECT_NEAR(2.0f, r["unitMs"].asFloat(), 0.001f);
    ASSERT_EQ(1u, r["modules"].size());
    EXPECT_EQ(0xAu, r["modules"][0]["crc"].asUInt());
    EXPECT_NEAR(1.0f, r["modules"][0]["ms"].asFloat(), 0.001f);
    EXPECT_FALSE(r["sections"]["available"].asBool());
    EXPECT_FALSE(r["sections"]["reason"].asString().empty());
}

TEST_F(PerfWindowRig, FrameUnitMatchesByNumericCrcToo)
{
    SendChunk(0x11u, 30.0f, 3);
    cmds.Drain();
    EXPECT_FALSE(cmds.HandleFrame(FrameArgs("17", 0)).isMember("error"));
}

TEST_F(PerfWindowRig, FrameErrors)
{
    SendChunk(0x11u, 30.0f, 10);
    cmds.Drain();
    EXPECT_TRUE(cmds.HandleFrame(FrameArgs("Nope", 0)).isMember("error"));
    EXPECT_TRUE(cmds.HandleFrame(FrameArgs("0x00000011", 10)).isMember("error"));   // not yet written
    EXPECT_TRUE(cmds.HandleFrame(FrameArgs(nullptr, 0)).isMember("error"));
    Json::Value noFrame(Json::objectValue);
    noFrame["unit"] = "0x00000011";
    EXPECT_TRUE(cmds.HandleFrame(noFrame).isMember("error"));
}

TEST_F(PerfWindowRig, RegisterUnregisterIdempotentAndDispatches)
{
    cmds.Register();
    cmds.Register();
    SendChunk(0x11u, 30.0f, 5);
    cmds.Drain();
    Json::Value resp = Dia::API::ExecuteCommandJson(StringCRC("perf.window"), Json::Value(Json::objectValue));
    EXPECT_TRUE(resp["success"].asBool());
    EXPECT_EQ(1u, resp["data"]["units"].size());
    resp = Dia::API::ExecuteCommandJson(StringCRC("perf.frame"), FrameArgs("0x00000011", 0));
    EXPECT_TRUE(resp["success"].asBool());
    cmds.Unregister();
    cmds.Unregister();
    resp = Dia::API::ExecuteCommandJson(StringCRC("perf.window"), Json::Value(Json::objectValue));
    EXPECT_FALSE(resp["success"].asBool());
}

TEST_F(PerfWindowRig, CommandsHaveDescriptionAndParams)
{
    cmds.Register();
    Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256> all;
    Dia::API::ListJsonCommandInfos(all);
    unsigned found = 0;
    for (unsigned int i = 0; i < all.Size(); ++i)
    {
        const bool isWindow = all[i].name == StringCRC("perf.window");
        const bool isFrame  = all[i].name == StringCRC("perf.frame");
        if (!isWindow && !isFrame) continue;
        ++found;
        ASSERT_NE(all[i].description, nullptr);
        EXPECT_GT(std::strlen(all[i].description), 0u);
        ASSERT_NE(all[i].params, nullptr);
        EXPECT_EQ(all[i].paramCount, isWindow ? 1u : 2u);
        for (unsigned p = 0; p < all[i].paramCount; ++p)
        {
            ASSERT_NE(all[i].params[p].description, nullptr);
            EXPECT_GT(std::strlen(all[i].params[p].description), 0u);
        }
    }
    EXPECT_EQ(2u, found);
    cmds.Unregister();
}
