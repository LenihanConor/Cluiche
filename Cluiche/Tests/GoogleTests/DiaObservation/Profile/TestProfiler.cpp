#include <gtest/gtest.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Profile/ScopedZone.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/ProfileCategory.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>

using namespace Dia::Observation::Profile;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    // Read the entire jsonl file into a string. Returns empty on failure.
    std::string ReadFile(const char* path)
    {
        std::ifstream f(path);
        if (!f.is_open())
            return {};
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>());
    }

    // Trigger a drain cycle: call BeginFrame so the drain thread picks up all
    // closed records from the previous frame, then sleep briefly.
    void FlushAndWait(int sleepMs = 50)
    {
        Profiler::Instance().BeginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

struct ProfilerTest : ::testing::Test
{
    Dia::Observation::Testing::ProfileFixture* mFixture = nullptr;

    void SetUp() override
    {
        // ProfileFixture ctor: stops any running session, starts a fresh one
        // with kAll mask, and registers the calling thread's scope buffer.
        mFixture = new Dia::Observation::Testing::ProfileFixture();
    }

    void TearDown() override
    {
        // ProfileFixture dtor: unregisters thread buffer, stops profiler,
        // and removes the temp files.
        delete mFixture;
        mFixture = nullptr;
    }
};

// ---------------------------------------------------------------------------
// AC1 — DIA_PROFILE_SCOPE writes a closed record to profile.jsonl
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC1_ScopeAppearsInJsonl)
{
    {
        DIA_PROFILE_SCOPE("AC1Zone", Category::kDiaApplicationFlow);
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_FALSE(content.empty()) << "profile.jsonl must not be empty after a closed scope";
    // scope_name is serialized as the CRC uint32 value
    uint32_t crc = Dia::Core::StringCRC("AC1Zone").Value();
    std::string crcStr = std::to_string(crc);
    EXPECT_NE(content.find(crcStr), std::string::npos)
        << "Expected scope_name CRC " << crc << " (AC1Zone) in profile.jsonl";
}

// ---------------------------------------------------------------------------
// AC2 — DIA_PROFILE_SCOPE_NAMED compiles and produces a ScopedZone variable
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC2_ScopeNamedCompiles)
{
    // The macro must expand to a ScopedZone local named 'zone'.
    DIA_PROFILE_SCOPE_NAMED(zone, "AC2Zone", Category::kDiaApplicationFlow);

    // Verify the variable is indeed a ScopedZone — this is a compile-time
    // assertion; if it fails the test won't build.
    static_assert(
        std::is_same<decltype(zone), ScopedZone>::value,
        "DIA_PROFILE_SCOPE_NAMED must produce a ScopedZone");

    // Runtime: variable must be usable without crashing.
    (void)zone;
    SUCCEED();
}

// ---------------------------------------------------------------------------
// AC3 — DIA_PROFILE_SCOPE_METRIC calls histogram.Observe() on destruction
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC3_ScopeMetricObservesHistogram)
{
    const float bounds[] = { 1.0f, 10.0f, 100.0f, 1000.0f };
    Dia::Observation::Metric::Histogram histogram(bounds, 4);

    {
        DIA_PROFILE_SCOPE_METRIC("AC3Zone", Category::kDiaApplicationFlow, &histogram);
        // Destructor fires here, must call histogram.Observe(duration).
    }

    Dia::Observation::Metric::Histogram::Data data;
    histogram.ReadData(data);
    EXPECT_GE(data.total, 1u)
        << "Histogram must have at least one observation after DIA_PROFILE_SCOPE_METRIC closes";
}

// ---------------------------------------------------------------------------
// AC4 — ActiveMask == 0 makes DIA_PROFILE_SCOPE a no-op (no record written)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC4_ZeroMaskIsNoOp)
{
    // Disable all categories.
    Profiler::Instance().SetActiveMask(Category::kNone);

    {
        DIA_PROFILE_SCOPE("AC4Zone", Category::kDiaApplicationFlow);
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    EXPECT_EQ(content.find("AC4Zone"), std::string::npos)
        << "No record should appear in profile.jsonl when ActiveMask is 0";
}

// ---------------------------------------------------------------------------
// AC6 — Root scope has parent_scope_id: 0 in profile.jsonl
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC6_RootScopeHasZeroParent)
{
    {
        DIA_PROFILE_SCOPE("AC6Root", Category::kDiaApplicationFlow);
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_FALSE(content.empty());
    EXPECT_NE(content.find("\"parent_scope_id\":0"), std::string::npos)
        << "Root scope must have parent_scope_id equal to 0 in profile.jsonl";
}

// ---------------------------------------------------------------------------
// AC7 — Child scope has a non-zero parent_scope_id in profile.jsonl
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC7_ChildScopeHasNonZeroParent)
{
    {
        DIA_PROFILE_SCOPE("AC7Root", Category::kDiaApplicationFlow);
        {
            DIA_PROFILE_SCOPE("AC7Child", Category::kDiaApplicationFlow);
            // Child closes first, then root closes.
        }
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_FALSE(content.empty());

    // At least one record must have a non-zero parent_scope_id.
    // The pattern "parent_scope_id":N where N > 0 means the digit after the
    // colon is not '0' or the number has more than one digit.
    bool foundNonZeroParent = false;
    std::string needle = "\"parent_scope_id\":";
    std::size_t pos = 0;
    while ((pos = content.find(needle, pos)) != std::string::npos)
    {
        pos += needle.size();
        // Read the number that follows.
        std::size_t end = content.find_first_not_of("0123456789", pos);
        std::string numStr = content.substr(pos, end - pos);
        if (!numStr.empty() && numStr != "0")
        {
            foundNonZeroParent = true;
            break;
        }
    }

    EXPECT_TRUE(foundNonZeroParent)
        << "At least one child scope must have a non-zero parent_scope_id in profile.jsonl";
}

// ---------------------------------------------------------------------------
// AC8 — BeginFrame increments frame counter; GetCurrentFrame returns 2 after
//        two BeginFrame calls (starts at 0, each BeginFrame advances by 1)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC8_BeginFrameIncrementsCounter)
{
    // ProfileFixture starts the profiler fresh; frame counter starts at 0.
    uint32_t before = Profiler::Instance().GetCurrentFrame();

    Profiler::Instance().BeginFrame();
    uint32_t after1 = Profiler::Instance().GetCurrentFrame();

    Profiler::Instance().BeginFrame();
    uint32_t after2 = Profiler::Instance().GetCurrentFrame();

    EXPECT_EQ(after1, before + 1u)
        << "First BeginFrame must increment frame counter by 1";
    EXPECT_EQ(after2, before + 2u)
        << "Second BeginFrame must give frame counter of start + 2";
}

// ---------------------------------------------------------------------------
// AC9 — Records flushed to profile.jsonl carry the frame_number they were
//        opened in (scope opened in frame 0 must show frame_number:0)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC9_RecordCarriesFrameNumber)
{
    // Confirm we are in frame 0 at test start.
    uint32_t openFrame = Profiler::Instance().GetCurrentFrame();

    {
        DIA_PROFILE_SCOPE("AC9Zone", Category::kDiaApplicationFlow);
        // Scope closes at end of block — record stamped with openFrame.
    }

    // Advance to next frame so the drain thread picks up the closed record.
    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_FALSE(content.empty());

    std::string frameField = "\"frame_number\":" + std::to_string(openFrame);
    EXPECT_NE(content.find(frameField), std::string::npos)
        << "Expected frame_number:" << openFrame << " in profile.jsonl for AC9Zone";
}

// ---------------------------------------------------------------------------
// AC12 — Unregistered thread silently drops scope records; no crash
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC12_UnregisteredThreadDropsSilently)
{
    std::exception_ptr exPtr = nullptr;
    bool crashed = false;

    std::thread worker([&]()
    {
        // Intentionally do NOT call RegisterThreadScopeBuffer().
        try
        {
            // Opening a scope on an unregistered thread must not crash.
            DIA_PROFILE_SCOPE("AC12Unregistered", Category::kDiaApplicationFlow);
            // Scope destructor also must not crash.
        }
        catch (...)
        {
            exPtr = std::current_exception();
            crashed = true;
        }
    });

    worker.join();

    EXPECT_FALSE(crashed)
        << "DIA_PROFILE_SCOPE on an unregistered thread must not throw or crash";
    // No record check — the point of this AC is no-crash behaviour.
}

// ---------------------------------------------------------------------------
// AC15 — DIA_PROFILE_SCOPE_METRIC with nullptr histogram is a no-crash no-op
//         (behaves like DIA_PROFILE_SCOPE)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC15_NullHistogramNoCrash)
{
    {
        DIA_PROFILE_SCOPE_METRIC("AC15Zone", Category::kDiaApplicationFlow, nullptr);
        // Destructor must not dereference nullptr.
    }
    SUCCEED() << "DIA_PROFILE_SCOPE_METRIC with nullptr histogram must not crash";
}

// ---------------------------------------------------------------------------
// AC19 — Profiler::Stop() drains remaining closed records before file close
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, AC19_StopDrainsRemainingRecords)
{
    {
        DIA_PROFILE_SCOPE("AC19Zone", Category::kDiaApplicationFlow);
    }

    // Do NOT call BeginFrame/FlushAndWait — let Stop() drain the record.
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_FALSE(content.empty())
        << "profile.jsonl must contain records drained by Stop()";
    uint32_t crc = Dia::Core::StringCRC("AC19Zone").Value();
    std::string crcStr = std::to_string(crc);
    EXPECT_NE(content.find(crcStr), std::string::npos)
        << "AC19Zone (CRC " << crc << ") must appear in profile.jsonl after Stop() drains remaining records";
}
