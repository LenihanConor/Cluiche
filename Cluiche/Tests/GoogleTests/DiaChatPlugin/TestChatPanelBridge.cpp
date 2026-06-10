// TestChatPanelBridge.cpp - Unit tests for ChatPanelBridge and ChatPanelBridgeHealth
//
// Covers: event queue (push/drain), all 8 event types, metrics counters,
// and ChatPanelBridgeHealth state transitions.
// Python is not involved — all tests run against the C++ layer only.

#include <gtest/gtest.h>

#include "Plugins/DiaChatPlugin/ChatPanelBridge.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <thread>

using namespace CluicheEditor;
using namespace Dia::Observation::Metric;
using namespace Dia::Observation::Testing;

using Dia::Observation::Health::HealthRegistry;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Health::Health;
using Dia::Observation::Health::HealthReporterBase;
using CluicheEditor::ChatPanelBridgeHealth;

// ==============================================================================
// Helpers
// ==============================================================================

static bool RegistryContainsReporter(const Dia::Core::StringCRC& name)
{
    HealthRegistry::ReporterSnapshot snapshots[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snapshots, 32, count);
    for (unsigned int i = 0; i < count; ++i)
        if (snapshots[i].name.Value() == name.Value()) return true;
    return false;
}

static Health SnapshotReporterHealth(const Dia::Core::StringCRC& name)
{
    HealthRegistry::ReporterSnapshot snapshots[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snapshots, 32, count);
    for (unsigned int i = 0; i < count; ++i)
        if (snapshots[i].name.Value() == name.Value()) return snapshots[i].health;
    Health h;
    h.status   = HealthStatus::kOK;
    h.errors   = 0;
    h.warnings = 0;
    return h;
}

// ==============================================================================
// Fixture
// ==============================================================================

struct ChatPanelBridgeTest : ::testing::Test
{
    MetricFixture metricFixture;   // resets MetricRegistry before/after each test
    HealthFixture healthFixture;   // resets HealthRegistry before/after each test

    // null UISystem is safe — NotifyUIDataChanged skips the JS call and logs a warning.
    Dia::Editor::WebUIBridge bridge { nullptr };
};

// ==============================================================================
// Construction / Destruction
// ==============================================================================

TEST_F(ChatPanelBridgeTest, Construct_RegistersThreeCounters)
{
    ChatPanelBridge b(&bridge);
    auto& reg = MetricRegistry::Instance();
    EXPECT_NE(reg.FindCounter(Dia::Core::StringCRC("dia.chat.messages_sent")),   nullptr);
    EXPECT_NE(reg.FindCounter(Dia::Core::StringCRC("dia.chat.tool_calls")),       nullptr);
    EXPECT_NE(reg.FindCounter(Dia::Core::StringCRC("dia.chat.tokens_streamed")),  nullptr);
}

TEST_F(ChatPanelBridgeTest, Construct_RegistersHealthReporter)
{
    ChatPanelBridge b(&bridge);
    EXPECT_TRUE(RegistryContainsReporter(Dia::Core::StringCRC("DiaChatPlugin")));
}

TEST_F(ChatPanelBridgeTest, Destruct_DoesNotCrash)
{
    // Verifies that destruction correctly unregisters from HealthRegistry without crashing.
    // HealthRegistry retains a retired snapshot entry — the reporter pointer is safe to
    // discard because Unregister() copies the final health value before releasing it.
    ChatPanelBridge* b = new ChatPanelBridge(&bridge);
    EXPECT_TRUE(RegistryContainsReporter(Dia::Core::StringCRC("DiaChatPlugin")));
    delete b;  // must not crash
}

// ==============================================================================
// OnTokenChunk — metrics
// ==============================================================================

TEST_F(ChatPanelBridgeTest, OnTokenChunk_DoneTrue_IncrementsMessagesSent)
{
    ChatPanelBridge b(&bridge);
    auto* ctr = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.messages_sent"));
    ASSERT_NE(ctr, nullptr);

    b.OnTokenChunk("hello", false);
    EXPECT_EQ(ctr->Value(), 0u);   // done=false — not counted yet

    b.OnTokenChunk("", true);
    EXPECT_EQ(ctr->Value(), 1u);
}

TEST_F(ChatPanelBridgeTest, OnTokenChunk_NonEmptyText_IncrementsTokensStreamed)
{
    ChatPanelBridge b(&bridge);
    auto* ctr = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.tokens_streamed"));
    ASSERT_NE(ctr, nullptr);

    b.OnTokenChunk("token", false);
    EXPECT_EQ(ctr->Value(), 1u);

    b.OnTokenChunk("", false);    // empty — not counted
    EXPECT_EQ(ctr->Value(), 1u);

    b.OnTokenChunk("more", true);
    EXPECT_EQ(ctr->Value(), 2u);
}

TEST_F(ChatPanelBridgeTest, OnToolStart_IncrementsToolCalls)
{
    ChatPanelBridge b(&bridge);
    auto* ctr = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.tool_calls"));
    ASSERT_NE(ctr, nullptr);

    b.OnToolStart("id1", "scene.list_entities", Json::Value());
    b.OnToolStart("id2", "history.undo", Json::Value());
    EXPECT_EQ(ctr->Value(), 2u);
}

// ==============================================================================
// Event queue — DoUpdate drains events
// ==============================================================================

TEST_F(ChatPanelBridgeTest, DoUpdate_EmptyQueue_NoOp)
{
    ChatPanelBridge b(&bridge);
    b.DoUpdate(0.016f);   // must not crash
}

TEST_F(ChatPanelBridgeTest, DoUpdate_TokenEvent_DrainedOnMainThread)
{
    ChatPanelBridge b(&bridge);
    std::thread t([&b]() { b.OnTokenChunk("word", false); });
    t.join();
    b.DoUpdate(0.016f);   // must not crash
}

TEST_F(ChatPanelBridgeTest, DoUpdate_AllEightEventTypes_DrainedWithoutCrash)
{
    ChatPanelBridge b(&bridge);

    b.OnTokenChunk("hi", false);
    b.OnTokenChunk("", true);
    b.OnToolStart("c1", "foo", Json::Value());
    b.OnToolResult("c1", Json::Value(), 10);
    b.OnToolError("c1", "oops");
    b.OnConfirmRequired("c2", "bar", Json::Value(), "Do this?");
    b.OnChatError("{\"message\":\"fail\"}");
    b.OnBackendStatus("ollama", "llama3.2", true);

    b.DoUpdate(0.016f);
    b.DoUpdate(0.016f);   // second pass — queue now empty, also safe
}

TEST_F(ChatPanelBridgeTest, DoUpdate_QueueClearedAfterDrain)
{
    ChatPanelBridge b(&bridge);
    b.OnTokenChunk("a", false);
    b.DoUpdate(0.016f);

    // Confirm counter was not double-incremented by a second update.
    auto* ctr = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.tokens_streamed"));
    ASSERT_NE(ctr, nullptr);
    b.DoUpdate(0.016f);
    EXPECT_EQ(ctr->Value(), 1u);
}

// ==============================================================================
// Thread-safety — concurrent push + drain
// ==============================================================================

TEST_F(ChatPanelBridgeTest, ConcurrentPushAndDrain_NoRace)
{
    ChatPanelBridge b(&bridge);

    // Spawn a background thread pushing 50 token chunks while the main thread drains.
    std::thread pusher([&b]()
    {
        for (int i = 0; i < 50; ++i)
            b.OnTokenChunk("x", i == 49);
    });

    for (int i = 0; i < 10; ++i)
        b.DoUpdate(0.016f);

    pusher.join();
    b.DoUpdate(0.016f);   // final drain

    auto* sent = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.messages_sent"));
    auto* tok  = MetricRegistry::Instance().FindCounter(Dia::Core::StringCRC("dia.chat.tokens_streamed"));
    ASSERT_NE(sent, nullptr);
    ASSERT_NE(tok,  nullptr);
    EXPECT_EQ(sent->Value(), 1u);   // done=true fired once
    EXPECT_GE(tok->Value(),  1u);   // at least some non-empty chunks counted
}

// ==============================================================================
// ChatPanelBridgeHealth — direct tests
// ==============================================================================

TEST_F(ChatPanelBridgeTest, BridgeHealth_DefaultIsOK)
{
    ChatPanelBridgeHealth h;
    EXPECT_EQ(h.Report().status, HealthStatus::kOK);
}

TEST_F(ChatPanelBridgeTest, BridgeHealth_BackendUnavailable_IsFailing)
{
    ChatPanelBridgeHealth h;
    h.OnBackendAvailability(false);
    Health report = h.Report();
    EXPECT_EQ(report.status, HealthStatus::kFailing);
    EXPECT_EQ(report.reason.Value(), Dia::Core::StringCRC("backend_unavailable").Value());
}

TEST_F(ChatPanelBridgeTest, BridgeHealth_BackendAvailable_IsOK)
{
    ChatPanelBridgeHealth h;
    h.OnBackendAvailability(false);
    h.OnBackendAvailability(true);
    EXPECT_EQ(h.Report().status, HealthStatus::kOK);
}

TEST_F(ChatPanelBridgeTest, BridgeHealth_ReporterName)
{
    ChatPanelBridgeHealth h;
    EXPECT_EQ(h.GetReporterName().Value(), Dia::Core::StringCRC("DiaChatPlugin").Value());
}

// ==============================================================================
// ChatPanelBridgeHealth — via OnBackendStatus on the bridge
// ==============================================================================

TEST_F(ChatPanelBridgeTest, BackendStatus_Unavailable_PropagatesFailingHealth)
{
    ChatPanelBridge b(&bridge);
    b.OnBackendStatus("", "", false);
    b.DoUpdate(0.0f);

    Health h = SnapshotReporterHealth(Dia::Core::StringCRC("DiaChatPlugin"));
    int actualStatus   = static_cast<int>(h.status);
    int expectedFailing = static_cast<int>(HealthStatus::kFailing);
    EXPECT_EQ(actualStatus, expectedFailing);
    EXPECT_EQ(h.reason.Value(), Dia::Core::StringCRC("backend_unavailable").Value());
}

TEST_F(ChatPanelBridgeTest, BackendStatus_Available_PropagatesOKHealth)
{
    ChatPanelBridge b(&bridge);
    b.OnBackendStatus("", "", false);
    b.DoUpdate(0.0f);
    b.OnBackendStatus("ollama", "llama3.2", true);
    b.DoUpdate(0.0f);

    Health h = SnapshotReporterHealth(Dia::Core::StringCRC("DiaChatPlugin"));
    int actualStatus  = static_cast<int>(h.status);
    int expectedOK    = static_cast<int>(HealthStatus::kOK);
    EXPECT_EQ(actualStatus, expectedOK);
}
