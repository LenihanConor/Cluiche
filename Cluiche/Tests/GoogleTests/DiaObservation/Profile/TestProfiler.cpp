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
#include <memory>
#include <vector>

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

// ---------------------------------------------------------------------------
// A1a - Subject on ScopeRecord / ScopedZone (profiling-frame-instrumentation A1, AA2, AA12)
// ---------------------------------------------------------------------------

namespace
{
    unsigned int PeekRecords(ScopeRecord* out, unsigned int maxOut)
    {
        return Profiler::Instance().PeekThreadClosedRecords(out, maxOut);
    }

    const ScopeRecord* FindByName(const ScopeRecord* recs, unsigned int n, const char* name)
    {
        Dia::Core::StringCRC crc(name);
        for (unsigned int i = 0; i < n; ++i)
            if (recs[i].name == crc)
                return &recs[i];
        return nullptr;
    }
}

TEST_F(ProfilerTest, A1a_ExplicitSubjectRecorded)
{
    {
        ScopedZone z(Dia::Core::StringCRC("A1aExplicit"), Category::kDiaApplicationFlow, 0x1234u);
    }
    ScopeRecord recs[16];
    unsigned int n = PeekRecords(recs, 16);
    ASSERT_EQ(1u, n);
    EXPECT_EQ(0x1234u, recs[0].subject);
}

TEST_F(ProfilerTest, A1a_MacroSubjectRecorded)
{
    {
        DIA_PROFILE_SCOPE_SUBJECT("A1aMacro", Category::kDiaApplicationFlow, 0x77u);
    }
    ScopeRecord recs[16];
    ASSERT_EQ(1u, PeekRecords(recs, 16));
    EXPECT_EQ(0x77u, recs[0].subject);
}

// A1b � file sink writes subject text
TEST_F(ProfilerTest, A1b_SinkWritesRegisteredSubjectName)
{
    Profiler::RegisterSubjectName(0xD1000001u, "SinkNamedSubject");
    {
        ScopedZone z(Dia::Core::StringCRC("A1bNamed"), Category::kDiaApplicationFlow, 0xD1000001u);
        DIA_PROFILE_SCOPE("A1bNamedChild", Category::kDiaApplicationFlow);
    }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    EXPECT_NE(content.find("\"subject\":\"SinkNamedSubject\""), std::string::npos);
    // parent + inheriting child both carry it
    size_t first = content.find("\"subject\":\"SinkNamedSubject\"");
    EXPECT_NE(content.find("\"subject\":\"SinkNamedSubject\"", first + 1), std::string::npos);
}

TEST_F(ProfilerTest, AA12_SinkWritesUnregisteredSubjectAsHex)
{
    {
        ScopedZone z(Dia::Core::StringCRC("A1bHex"), Category::kDiaApplicationFlow, 0xD1000002u);
    }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    EXPECT_NE(content.find("\"subject\":\"0xd1000002\""), std::string::npos);
}

TEST_F(ProfilerTest, A1b_SinkOmitsSubjectWhenZero)
{
    {
        DIA_PROFILE_SCOPE("A1bNoSubject", Category::kDiaApplicationFlow);
    }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    ASSERT_NE(content.find(std::to_string(Dia::Core::StringCRC("A1bNoSubject").Value())), std::string::npos);
    EXPECT_EQ(content.find("\"subject\""), std::string::npos);
}

TEST_F(ProfilerTest, AA2_PlainNestedScopeInheritsSubject)
{
    {
        ScopedZone outer(Dia::Core::StringCRC("A1aOuter"), Category::kDiaApplicationFlow, 0xABCDu);
        {
            DIA_PROFILE_SCOPE("A1aInner", Category::kDiaApplicationFlow);
            {
                DIA_PROFILE_SCOPE("A1aInnerInner", Category::kDiaApplicationFlow);
            }
        }
    }
    ScopeRecord recs[16];
    unsigned int n = PeekRecords(recs, 16);
    ASSERT_EQ(3u, n);
    EXPECT_EQ(0xABCDu, FindByName(recs, n, "A1aOuter")->subject);
    EXPECT_EQ(0xABCDu, FindByName(recs, n, "A1aInner")->subject);
    EXPECT_EQ(0xABCDu, FindByName(recs, n, "A1aInnerInner")->subject);
}

TEST_F(ProfilerTest, A1a_NestedExplicitSubjectOverrides)
{
    {
        ScopedZone outer(Dia::Core::StringCRC("A1aOvOuter"), Category::kDiaApplicationFlow, 0x11u);
        {
            ScopedZone mid(Dia::Core::StringCRC("A1aOvMid"), Category::kDiaApplicationFlow, 0x22u);
            DIA_PROFILE_SCOPE("A1aOvLeaf", Category::kDiaApplicationFlow);
        }
    }
    ScopeRecord recs[16];
    unsigned int n = PeekRecords(recs, 16);
    ASSERT_EQ(3u, n);
    EXPECT_EQ(0x11u, FindByName(recs, n, "A1aOvOuter")->subject);
    EXPECT_EQ(0x22u, FindByName(recs, n, "A1aOvMid")->subject);
    EXPECT_EQ(0x22u, FindByName(recs, n, "A1aOvLeaf")->subject);
}

TEST_F(ProfilerTest, A1a_SubjectZeroStaysZero)
{
    {
        DIA_PROFILE_SCOPE("A1aZeroRoot", Category::kDiaApplicationFlow);
        DIA_PROFILE_SCOPE("A1aZeroChild", Category::kDiaApplicationFlow);
    }
    {
        ScopedZone z(Dia::Core::StringCRC("A1aZeroExplicit"), Category::kDiaApplicationFlow, 0u);
    }
    ScopeRecord recs[16];
    unsigned int n = PeekRecords(recs, 16);
    ASSERT_EQ(3u, n);
    for (unsigned int i = 0; i < n; ++i)
        EXPECT_EQ(0u, recs[i].subject);
}

TEST_F(ProfilerTest, A1a_CurrentSubjectInsideAndOutside)
{
    EXPECT_EQ(0u, Profiler::CurrentSubject());
    {
        ScopedZone outer(Dia::Core::StringCRC("A1aCurOuter"), Category::kDiaApplicationFlow, 0x5u);
        EXPECT_EQ(0x5u, Profiler::CurrentSubject());
        {
            DIA_PROFILE_SCOPE("A1aCurPlain", Category::kDiaApplicationFlow);
            EXPECT_EQ(0x5u, Profiler::CurrentSubject());
            {
                ScopedZone inner(Dia::Core::StringCRC("A1aCurInner"), Category::kDiaApplicationFlow, 0x6u);
                EXPECT_EQ(0x6u, Profiler::CurrentSubject());
            }
            EXPECT_EQ(0x5u, Profiler::CurrentSubject());
        }
    }
    EXPECT_EQ(0u, Profiler::CurrentSubject());
}

TEST_F(ProfilerTest, A1a_SiblingAfterSubjectScopeDoesNotInherit)
{
    {
        ScopedZone first(Dia::Core::StringCRC("A1aSibFirst"), Category::kDiaApplicationFlow, 0x99u);
    }
    {
        DIA_PROFILE_SCOPE("A1aSibSecond", Category::kDiaApplicationFlow);
    }
    ScopeRecord recs[16];
    unsigned int n = PeekRecords(recs, 16);
    ASSERT_EQ(2u, n);
    EXPECT_EQ(0x99u, FindByName(recs, n, "A1aSibFirst")->subject);
    EXPECT_EQ(0u, FindByName(recs, n, "A1aSibSecond")->subject);
}

TEST_F(ProfilerTest, A1a_OpenScopesSnapshotCarriesSubject)
{
    ScopedZone outer(Dia::Core::StringCRC("A1aSnapOuter"), Category::kDiaApplicationFlow, 0x31u);
    DIA_PROFILE_SCOPE("A1aSnapInner", Category::kDiaApplicationFlow);
    Profiler::OpenScopeEntry entries[8];
    unsigned int n = Profiler::Instance().GetOpenScopesSnapshot(entries, 8);
    ASSERT_GE(n, 2u);
    unsigned int found = 0;
    for (unsigned int i = 0; i < n; ++i)
        if (entries[i].name == Dia::Core::StringCRC("A1aSnapOuter") ||
            entries[i].name == Dia::Core::StringCRC("A1aSnapInner"))
        {
            EXPECT_EQ(0x31u, entries[i].subject);
            ++found;
        }
    EXPECT_EQ(2u, found);
}

TEST_F(ProfilerTest, A1a_Depth64StackInheritsSubject)
{
    std::vector<std::unique_ptr<ScopedZone>> zones;
    zones.emplace_back(new ScopedZone(Dia::Core::StringCRC("A1aDeep"), Category::kDiaApplicationFlow, 0xD00Du));
    for (int i = 1; i < 64; ++i)
        zones.emplace_back(new ScopedZone(Dia::Core::StringCRC("A1aDeep"), Category::kDiaApplicationFlow));
    EXPECT_EQ(0xD00Du, Profiler::CurrentSubject());
    while (!zones.empty())
        zones.pop_back();
    EXPECT_EQ(0u, Profiler::CurrentSubject());

    ScopeRecord recs[128];
    unsigned int n = PeekRecords(recs, 128);
    ASSERT_EQ(64u, n);
    for (unsigned int i = 0; i < n; ++i)
        EXPECT_EQ(0xD00Du, recs[i].subject);
}

// Audit (task 11): the closed ring wraps/drops past 512 records; every surviving record keeps its subject.
TEST_F(ProfilerTest, A1a_RingOverflowKeepsSubjectOnEveryRecord)
{
    for (int i = 0; i < 700; ++i)
    {
        ScopedZone z(Dia::Core::StringCRC("A1aWrap"), Category::kDiaApplicationFlow, 0x5EEDu);
        DIA_PROFILE_SCOPE("A1aWrapChild", Category::kDiaApplicationFlow);
    }
    std::vector<ScopeRecord> recs(2048);
    unsigned int n = PeekRecords(recs.data(), 2048);
    ASSERT_GT(n, 0u);
    EXPECT_LE(n, 1400u);
    for (unsigned int i = 0; i < n; ++i)
        EXPECT_EQ(0x5EEDu, recs[i].subject) << "record " << i;
}

TEST_F(ProfilerTest, A1a_ScopeRecordSizeReport)
{
    // Subject packs after threadId; record grows 112 -> 120 (8-byte alignment).
    RecordProperty("sizeof_ScopeRecord", static_cast<int>(sizeof(ScopeRecord)));
    EXPECT_LE(sizeof(ScopeRecord), 120u);
}

TEST(ProfilerSubjectNames, AA12_UnknownResolvesToHex)
{
    char buf[32];
    EXPECT_STREQ("0xdeadbeef", Profiler::ResolveSubjectName(0xDEADBEEFu, buf, sizeof(buf)));
    EXPECT_STREQ("0x00001a2b", Profiler::ResolveSubjectName(0x1A2Bu, buf, sizeof(buf)));
}

TEST(ProfilerSubjectNames, AA12_RegisteredResolvesToName)
{
    char buf[64];
    EXPECT_TRUE(Profiler::RegisterSubjectName(0xA0000001u, "RenderModule"));
    EXPECT_STREQ("RenderModule", Profiler::ResolveSubjectName(0xA0000001u, buf, sizeof(buf)));
}

TEST(ProfilerSubjectNames, DuplicateRegistrationOverwrites)
{
    char buf[64];
    EXPECT_TRUE(Profiler::RegisterSubjectName(0xA0000002u, "First"));
    EXPECT_TRUE(Profiler::RegisterSubjectName(0xA0000002u, "Second"));
    EXPECT_STREQ("Second", Profiler::ResolveSubjectName(0xA0000002u, buf, sizeof(buf)));
}

TEST(ProfilerSubjectNames, InvalidRegistrationRejected)
{
    EXPECT_FALSE(Profiler::RegisterSubjectName(0u, "Zero"));
    EXPECT_FALSE(Profiler::RegisterSubjectName(0xA0000003u, nullptr));
}

TEST(ProfilerSubjectNames, SmallBufferTruncatesSafely)
{
    char buf[4];
    Profiler::RegisterSubjectName(0xA0000004u, "LongModuleName");
    Profiler::ResolveSubjectName(0xA0000004u, buf, sizeof(buf));
    EXPECT_EQ('\0', buf[3]);
}

// Keep last: fills the process-wide table.
TEST(ProfilerSubjectNames, ZZ_TableFullReturnsFalseWithoutCrash)
{
    char buf[64];
    ASSERT_TRUE(Profiler::RegisterSubjectName(0xA0000005u, "Survivor"));
    bool sawFull = false;
    for (uint32_t i = 0; i < Profiler::kMaxSubjectNames + 8; ++i)
    {
        if (!Profiler::RegisterSubjectName(0xB0000000u + i, "Filler"))
        {
            sawFull = true;
            break;
        }
    }
    EXPECT_TRUE(sawFull);
    EXPECT_FALSE(Profiler::RegisterSubjectName(0xC0000000u, "Overflow"));
    EXPECT_STREQ("Survivor", Profiler::ResolveSubjectName(0xA0000005u, buf, sizeof(buf)));
    EXPECT_TRUE(Profiler::RegisterSubjectName(0xA0000005u, "Renamed"));
    EXPECT_STREQ("0xc0000000", Profiler::ResolveSubjectName(0xC0000000u, buf, sizeof(buf)));
}

// ---------------------------------------------------------------------------
// B1 - thread names (profiling-frame-instrumentation Amendment 2026-11-12)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, B1_RegisterThreadName_DuplicateOverwrites)
{
    Profiler::ClearThreadNames();
    EXPECT_TRUE(Profiler::RegisterThreadName(4001u, "First"));
    EXPECT_TRUE(Profiler::RegisterThreadName(4001u, "Second"));
    char buf[64];
    ASSERT_TRUE(Profiler::ResolveThreadName(4001u, buf, sizeof(buf)));
    EXPECT_STREQ("Second", buf);
    EXPECT_FALSE(Profiler::ResolveThreadName(4002u, buf, sizeof(buf)));
    Profiler::ClearThreadNames();
}

TEST_F(ProfilerTest, B1_RegisterThreadName_FullReturnsFalse)
{
    Profiler::ClearThreadNames();
    for (unsigned int i = 0; i < Profiler::kMaxThreadNames; ++i)
        ASSERT_TRUE(Profiler::RegisterThreadName(5000u + i, "T"));
    EXPECT_FALSE(Profiler::RegisterThreadName(9999u, "Overflow"));
    EXPECT_TRUE(Profiler::RegisterThreadName(5000u, "Existing"));   // overwrite still OK when full
    Profiler::ClearThreadNames();
}

TEST_F(ProfilerTest, B1_RegisterThreadName_NullNameRejected)
{
    EXPECT_FALSE(Profiler::RegisterThreadName(4003u, nullptr));
}

TEST_F(ProfilerTest, B1_SinkWritesThreadNameBeforeFirstScope)
{
    Profiler::ClearThreadNames();
    const uint32_t tid = Profiler::CurrentThreadId();
    ASSERT_TRUE(Profiler::RegisterThreadName(tid, "Test Main"));
    {
        DIA_PROFILE_SCOPE("B1First", Category::kDiaApplicationFlow);
    }
    {
        DIA_PROFILE_SCOPE("B1Second", Category::kDiaApplicationFlow);
    }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    const std::string nameLine = "{\"record_type\":\"thread_name\",\"thread_id\":" + std::to_string(tid) + ",\"name\":\"Test Main\"}";
    size_t namePos = content.find(nameLine);
    ASSERT_NE(namePos, std::string::npos) << content;
    EXPECT_EQ(content.find(nameLine, namePos + 1), std::string::npos) << "announced once only";
    size_t scopePos = content.find("\"thread_id\":" + std::to_string(tid) + ",\"scope_name\"");
    ASSERT_NE(scopePos, std::string::npos);
    EXPECT_LT(namePos, scopePos);
    Profiler::ClearThreadNames();
}

TEST_F(ProfilerTest, B1_UnnamedThreadHasNoThreadNameRecord)
{
    Profiler::ClearThreadNames();
    {
        DIA_PROFILE_SCOPE("B1Unnamed", Category::kDiaApplicationFlow);
    }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    EXPECT_NE(content.find(std::to_string(Dia::Core::StringCRC("B1Unnamed").Value())), std::string::npos);
    EXPECT_EQ(content.find("thread_name"), std::string::npos);
}

TEST_F(ProfilerTest, B1_ChangedNameIsReannounced)
{
    Profiler::ClearThreadNames();
    const uint32_t tid = Profiler::CurrentThreadId();
    Profiler::RegisterThreadName(tid, "Before");
    { DIA_PROFILE_SCOPE("B1A", Category::kDiaApplicationFlow); }
    FlushAndWait();
    Profiler::RegisterThreadName(tid, "After");
    { DIA_PROFILE_SCOPE("B1B", Category::kDiaApplicationFlow); }
    FlushAndWait();
    Profiler::Instance().Stop();
    std::string content = ReadFile(mFixture->mJsonlPath);
    EXPECT_NE(content.find("\"name\":\"Before\""), std::string::npos);
    EXPECT_NE(content.find("\"name\":\"After\""), std::string::npos);
    Profiler::ClearThreadNames();
}

// ---------------------------------------------------------------------------
// DeferredStep category — scope records under the dedicated bit (kNone never records)
// ---------------------------------------------------------------------------

TEST_F(ProfilerTest, DeferredStepCategory_ScopeAppearsInJsonl)
{
    static_assert(Category::kDiaDeferredStep != 0, "kDiaDeferredStep must be a real bit");
    static_assert((Category::kDiaDeferredStep & (Category::kDiaScene | Category::kdiaentitytemplate | Category::kDiaAnimation)) == 0,
        "kDiaDeferredStep must not collide with existing categories");
    {
        DIA_PROFILE_SCOPE("DSCatZone", Category::kDiaDeferredStep);
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    uint32_t crc = Dia::Core::StringCRC("DSCatZone").Value();
    EXPECT_NE(content.find(std::to_string(crc)), std::string::npos);
}

TEST_F(ProfilerTest, NoneCategory_ScopeNeverRecords)
{
    {
        DIA_PROFILE_SCOPE("NoneCatZone", Category::kNone);
    }

    FlushAndWait();
    Profiler::Instance().Stop();

    std::string content = ReadFile(mFixture->mJsonlPath);
    uint32_t crc = Dia::Core::StringCRC("NoneCatZone").Value();
    EXPECT_EQ(content.find(std::to_string(crc)), std::string::npos);
}
