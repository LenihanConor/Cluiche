////////////////////////////////////////////////////////////////////////////////
// Filename: TestDiaVisualDebuggerConsole.cpp
// Description: Tests for DiaVisualDebuggerConsole and DiaImGuiManager.
//              Tests cover C++ logic only -- no ImGui render output verification.
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-console.md
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaImGui/DiaImGuiManager.h>
#include <DiaImGui/IImGuiBackend.h>
#include <DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/LogEntry.h>
#include <DiaCore/CRC/StringCRC.h>

// =====================================================================
// Test helpers
// =====================================================================

namespace
{
    // Stub backend for DiaImGuiManager tests
    class StubImGuiBackend : public Dia::ImGui::IImGuiBackend
    {
    public:
        void Init()             override { ++initCount; }
        void Shutdown()         override { ++shutdownCount; }
        void NewFrame(float dt) override { ++newFrameCount; lastDt = dt; }
        void Render()           override { ++renderCount; }

        int initCount     = 0;
        int shutdownCount = 0;
        int newFrameCount = 0;
        int renderCount   = 0;
        float lastDt      = 0.0f;
    };

    // Minimal IVisualDebugger for layer manager tests
    class StubVisualDebugger : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit StubVisualDebugger(const char* name)
            : mName(name)
        {}

        Dia::Core::StringCRC GetLayerName() const override { return mName; }
        void Draw(Dia::Graphics::FrameData& /*frameData*/) override {}

    private:
        Dia::Core::StringCRC mName;
    };
}

// =====================================================================
// Suite: DiaImGuiManager
// =====================================================================

TEST(DiaImGuiManager, SetBackend_GetBackend_RoundTrip)
{
    Dia::ImGui::DiaImGuiManager manager;
    StubImGuiBackend backend;

    EXPECT_EQ(manager.GetBackend(), nullptr);
    manager.SetBackend(&backend);
    EXPECT_EQ(manager.GetBackend(), &backend);
}

TEST(DiaImGuiManager, NewFrame_NullBackend_NoAssert)
{
    Dia::ImGui::DiaImGuiManager manager;
    // Should no-op gracefully without crashing
    manager.NewFrame(1.0f / 60.0f);
    manager.Render();
    manager.Init();
    manager.Shutdown();
}

TEST(DiaImGuiManager, ForwardsToBackend)
{
    Dia::ImGui::DiaImGuiManager manager;
    StubImGuiBackend backend;
    manager.SetBackend(&backend);

    manager.Init();
    EXPECT_EQ(backend.initCount, 1);

    manager.NewFrame(0.016f);
    EXPECT_EQ(backend.newFrameCount, 1);
    EXPECT_FLOAT_EQ(backend.lastDt, 0.016f);

    manager.Render();
    EXPECT_EQ(backend.renderCount, 1);

    manager.Shutdown();
    EXPECT_EQ(backend.shutdownCount, 1);
}

// =====================================================================
// Suite: Visibility
// =====================================================================

TEST(VisualDebuggerConsole_Visibility, DefaultHidden)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    EXPECT_FALSE(console.IsVisible());
}

TEST(VisualDebuggerConsole_Visibility, Toggle_ShowsConsole)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    console.Toggle();
    EXPECT_TRUE(console.IsVisible());
}

TEST(VisualDebuggerConsole_Visibility, Toggle_HidesConsole)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    console.Toggle();  // show
    console.Toggle();  // hide
    EXPECT_FALSE(console.IsVisible());
}

// =====================================================================
// Suite: LogTail
// =====================================================================

TEST(VisualDebuggerConsole_LogTail, Attach_LogMessage_AppearsInTail)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();

    // Register a thread buffer for this thread (required by DiaLogger)
    logger.RegisterThreadBuffer();

    console.Attach(logger);

    // Log a warning (console sink threshold is kWarning)
    Dia::Logger::LogEntry entry;
    entry.level = Dia::Logger::LogLevel::kWarning;
    entry.channel = Dia::Core::StringCRC("test");
    strncpy_s(entry.message, sizeof(entry.message), "Test warning message", sizeof(entry.message));
    logger.DispatchImmediate(entry);

    EXPECT_EQ(console.GetLogCount(), 1);
    EXPECT_STREQ(console.GetLogLine(0), "Test warning message");

    console.Detach();
    logger.UnregisterThreadBuffer();
}

TEST(VisualDebuggerConsole_LogTail, LogTail_CapacityLimit_OldestDropped)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
    logger.RegisterThreadBuffer();

    console.Attach(logger);

    const int totalMessages = Dia::Debug::DiaVisualDebuggerConsole::kLogTailCapacity + 5;
    for (int i = 0; i < totalMessages; ++i)
    {
        Dia::Logger::LogEntry entry;
        entry.level = Dia::Logger::LogLevel::kError;
        entry.channel = Dia::Core::StringCRC("test");
        snprintf(entry.message, sizeof(entry.message), "Message %d", i);
        logger.DispatchImmediate(entry);
    }

    EXPECT_EQ(console.GetLogCount(), Dia::Debug::DiaVisualDebuggerConsole::kLogTailCapacity);

    // The oldest messages should have been dropped. The most recent message should be present.
    char expected[128];
    snprintf(expected, sizeof(expected), "Message %d", totalMessages - 1);
    EXPECT_STREQ(console.GetLogLine(console.GetLogCount() - 1), expected);

    console.Detach();
    logger.UnregisterThreadBuffer();
}

TEST(VisualDebuggerConsole_LogTail, Detach_StopsReceivingLogs)
{
    Dia::Debug::DiaVisualDebuggerConsole console;
    Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
    logger.RegisterThreadBuffer();

    console.Attach(logger);

    // Log one message
    Dia::Logger::LogEntry entry;
    entry.level = Dia::Logger::LogLevel::kError;
    entry.channel = Dia::Core::StringCRC("test");
    strncpy_s(entry.message, sizeof(entry.message), "Before detach", sizeof(entry.message));
    logger.DispatchImmediate(entry);

    int countBefore = console.GetLogCount();
    EXPECT_EQ(countBefore, 1);

    console.Detach();

    // Log another message after detach
    Dia::Logger::LogEntry entry2;
    entry2.level = Dia::Logger::LogLevel::kError;
    entry2.channel = Dia::Core::StringCRC("test");
    strncpy_s(entry2.message, sizeof(entry2.message), "After detach", sizeof(entry2.message) - 1);
    logger.DispatchImmediate(entry2);

    // Count should not have changed
    EXPECT_EQ(console.GetLogCount(), countBefore);

    logger.UnregisterThreadBuffer();
}

// =====================================================================
// Suite: DebugLayerManager accessors
// =====================================================================

TEST(DebugLayerManagerAccessors, GetLayerCount_AfterRegister_IsOne)
{
    Dia::Debug::DebugLayerManager manager;
    StubVisualDebugger debugger("test.layer");
    manager.Register(&debugger, 0);

    EXPECT_EQ(manager.GetLayerCount(), 1);
}

TEST(DebugLayerManagerAccessors, GetLayerName_ReturnsRegisteredName)
{
    Dia::Debug::DebugLayerManager manager;
    StubVisualDebugger debugger("physics.shapes");
    manager.Register(&debugger, 0);

    Dia::Core::StringCRC name = manager.GetLayerName(0);
    EXPECT_EQ(name, Dia::Core::StringCRC("physics.shapes"));
}

TEST(DebugLayerManagerAccessors, GetLayerName_OutOfRange_ReturnsZero)
{
    Dia::Debug::DebugLayerManager manager;
    Dia::Core::StringCRC name = manager.GetLayerName(99);
    EXPECT_EQ(name, Dia::Core::StringCRC::kZero);
}

#endif // DIA_DEBUG
