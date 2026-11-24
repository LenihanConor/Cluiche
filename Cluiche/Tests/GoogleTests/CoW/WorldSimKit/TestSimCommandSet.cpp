#include <gtest/gtest.h>

#include <string.h>

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>

#include <Shared/Debug/SimCommandSet.h>
#include <Shared/Debug/Testing/FakeSimCommandHost.h>

namespace CoW {
namespace Debug {
namespace Testing {

namespace {

class ErrorSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (e.level != Dia::Observation::Log::LogLevel::kError) { return; }
        if (strstr(e.message, "SimCmdTest") == nullptr) { return; }   // only this suite's owner text
        ++errors;
        if (strstr(e.message, "cow.test.third") != nullptr) { namesThird = true; }
    }
    const char* GetName() const override { return "SimCommandSetTestSink"; }
    unsigned errors = 0;
    bool namesThird = false;
};

Dia::DebugServer::SimHelpInfo Help()
{
    Dia::DebugServer::SimHelpInfo h;
    h.description = "test entry";
    h.noArgs = true;
    return h;
}

class SimCommandSetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&sink);
        const char* names[4] = { "cow.test.first", "cow.test.second", "cow.test.third", "cow.test.fourth" };
        const EntryKind kinds[4] = { EntryKind::kCommand, EntryKind::kQuery, EntryKind::kCommand, EntryKind::kQuery };
        for (uint32_t i = 0; i < 4; ++i)
        {
            entries[i].name = names[i];
            entries[i].kind = kinds[i];
            entries[i].help = Help();
            entries[i].handler = [](const Json::Value&) { return Json::Value(Json::objectValue); };
        }
    }
    void TearDown() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&sink);
        l.UnregisterThreadBuffer();
    }
    ErrorSink& Sink() { Dia::Observation::Log::Logger::Instance().FlushSync(); return sink; }

    ErrorSink sink;
    SimCommandEntry entries[4];
    FakeSimCommandHost host;
    SimCommandSet set;
};

}  // namespace

TEST_F(SimCommandSetTest, AllEntriesRegisterWithTheirKinds)
{
    EXPECT_TRUE(set.RegisterAll(host, entries, 4, "SimCmdTest"));
    ASSERT_EQ(4u, host.regCount);
    EXPECT_EQ(EntryKind::kCommand, host.regKinds[0]);
    EXPECT_EQ(EntryKind::kQuery, host.regKinds[1]);
    EXPECT_EQ(EntryKind::kCommand, host.regKinds[2]);
    EXPECT_EQ(4u, set.Count());
    EXPECT_EQ(0u, host.unregCount);
    EXPECT_EQ(0u, Sink().errors);
}

TEST_F(SimCommandSetTest, FailureOnEntryThreeRollsBackOneAndTwoEachWithItsOwnKind)
{
    host.failOnNth = 3;
    EXPECT_FALSE(set.RegisterAll(host, entries, 4, "SimCmdTest"));
    ASSERT_EQ(2u, host.unregCount);
    // reverse order: entry 2 (query) then entry 1 (command)
    EXPECT_EQ(Dia::Core::StringCRC("cow.test.second"), host.unregNames[0]);
    EXPECT_EQ(EntryKind::kQuery, host.unregKinds[0]);
    EXPECT_EQ(Dia::Core::StringCRC("cow.test.first"), host.unregNames[1]);
    EXPECT_EQ(EntryKind::kCommand, host.unregKinds[1]);
    EXPECT_EQ(0u, set.Count());
    EXPECT_EQ(3u, host.attempts);                       // entry 4 was never tried
}

TEST_F(SimCommandSetTest, FailureLogsExactlyOneErrorNamingTheFailedEntry)
{
    host.failOnNth = 3;
    set.RegisterAll(host, entries, 4, "SimCmdTest");
    EXPECT_EQ(1u, Sink().errors);
    EXPECT_TRUE(sink.namesThird);
}

TEST_F(SimCommandSetTest, FailureOnFirstEntryRollsBackNothing)
{
    host.failOnNth = 1;
    EXPECT_FALSE(set.RegisterAll(host, entries, 4, "SimCmdTest"));
    EXPECT_EQ(0u, host.unregCount);
    EXPECT_EQ(1u, Sink().errors);
}

TEST_F(SimCommandSetTest, UnregisterAllTwiceIsSafeAndUnregistersEachOnce)
{
    set.RegisterAll(host, entries, 4, "SimCmdTest");
    set.UnregisterAll();
    set.UnregisterAll();
    EXPECT_EQ(4u, host.unregCount);
    EXPECT_EQ(0u, set.Count());
}

TEST_F(SimCommandSetTest, UnregisterAllOnAnEmptyOrFailedSetDoesNothing)
{
    set.UnregisterAll();
    EXPECT_EQ(0u, host.unregCount);
    host.failOnNth = 2;
    set.RegisterAll(host, entries, 4, "SimCmdTest");
    const uint32_t after = host.unregCount;
    set.UnregisterAll();
    EXPECT_EQ(after, host.unregCount);
}

TEST_F(SimCommandSetTest, ReRegisterAfterAFailureSucceeds)
{
    host.failOnNth = 3;
    EXPECT_FALSE(set.RegisterAll(host, entries, 4, "SimCmdTest"));
    EXPECT_TRUE(set.RegisterAll(host, entries, 4, "SimCmdTest"));   // attempts 4..7, none fail
    EXPECT_EQ(4u, set.Count());
}

TEST_F(SimCommandSetTest, DestructorUnregisters)
{
    {
        SimCommandSet scoped;
        scoped.RegisterAll(host, entries, 2, "SimCmdTest");
    }
    EXPECT_EQ(2u, host.unregCount);
}

TEST_F(SimCommandSetTest, TooManyEntriesIsRejectedWithOneError)
{
    EXPECT_FALSE(set.RegisterAll(host, entries, SimCommandSet::kMaxEntries + 1, "SimCmdTest"));
    EXPECT_EQ(0u, host.attempts);
    EXPECT_EQ(1u, Sink().errors);
}

TEST_F(SimCommandSetTest, AdapterRollbackRemovesARegisteredCommandFromTheRealHost)
{
    Dia::DebugServer::SimQueryHost real;
    SimQueryHostAdapter adapter(real);
    entries[2].help.description = nullptr;              // the real host rejects help-less entries: entry 3 fails
    EXPECT_FALSE(set.RegisterAll(adapter, entries, 4, "SimCmdTest"));
    EXPECT_FALSE(real.Has(Dia::Core::StringCRC("cow.test.first")));    // a command, rolled back
    EXPECT_FALSE(real.Has(Dia::Core::StringCRC("cow.test.second")));   // a query, rolled back

    entries[2].help = Help();
    EXPECT_TRUE(set.RegisterAll(adapter, entries, 4, "SimCmdTest"));
    EXPECT_TRUE(real.Has(Dia::Core::StringCRC("cow.test.first")));
    set.UnregisterAll();
    EXPECT_FALSE(real.Has(Dia::Core::StringCRC("cow.test.first")));
    EXPECT_FALSE(real.Has(Dia::Core::StringCRC("cow.test.third")));
}

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
