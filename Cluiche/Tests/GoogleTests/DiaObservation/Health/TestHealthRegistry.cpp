#include <gtest/gtest.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaObservation/Health/IHealthTransitionSink.h>
#include <DiaCore/CRC/StringCRC.h>

#include <thread>
#include <atomic>
#include <cstdio>
#include <deque>

using namespace Dia::Observation::Health;
using namespace Dia::Observation::Testing;

namespace
{
    struct CaptureTransitionSink : public IHealthTransitionSink
    {
        static const unsigned int kMax = 32;
        HealthRegistry::Transition transitions[kMax];
        unsigned int count = 0;

        void OnTransition(const HealthRegistry::Transition& t) override
        {
            if (count < kMax)
                transitions[count++] = t;
        }
    };
}

struct HealthRegistryTest : ::testing::Test
{
    HealthFixture fixture;
};

TEST_F(HealthRegistryTest, RegisterAndSnapshot)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.a"));
    HealthRegistry::Instance().Register(&r);

    HealthRegistry::ReporterSnapshot snaps[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 32, count);

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(snaps[0].name.Value(), Dia::Core::StringCRC("mod.a").Value());
    EXPECT_EQ(snaps[0].health.status, HealthStatus::kOK);
}

TEST_F(HealthRegistryTest, UnregisterRetainsInSnapshot)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.b"));
    HealthRegistry::Instance().Register(&r);
    HealthRegistry::Instance().Unregister(&r);

    // Retired reporters stay in the snapshot so health.json captures modules
    // that stopped before WriteHealthJson() runs at session end.
    HealthRegistry::ReporterSnapshot snaps[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 32, count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(snaps[0].name.Value(), Dia::Core::StringCRC("mod.b").Value());
    EXPECT_EQ(snaps[0].health.status, HealthStatus::kOK);
}

TEST_F(HealthRegistryTest, PollTransitions_DetectsChange)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.c"));
    HealthRegistry::Instance().Register(&r);

    r.SetFailing(Dia::Core::StringCRC("broken"));

    HealthRegistry::Transition out[32];
    unsigned int outCount = 0;
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);

    EXPECT_EQ(outCount, 1u);
    EXPECT_EQ(out[0].oldStatus, HealthStatus::kOK);
    EXPECT_EQ(out[0].newStatus, HealthStatus::kFailing);
}

TEST_F(HealthRegistryTest, PollTransitions_NoChangeNoOutput)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.d"));
    HealthRegistry::Instance().Register(&r);

    HealthRegistry::Transition out[32];
    unsigned int outCount = 0;
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);
    EXPECT_EQ(outCount, 0u);

    // Poll again — still no change
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);
    EXPECT_EQ(outCount, 0u);
}

TEST_F(HealthRegistryTest, PollTransitions_SubsequentChangeDetected)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.e"));
    HealthRegistry::Instance().Register(&r);

    r.SetDegraded(Dia::Core::StringCRC("slow"));

    HealthRegistry::Transition out[32];
    unsigned int outCount = 0;
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);
    EXPECT_EQ(outCount, 1u);

    // Now recover
    r.SetOK();
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);
    EXPECT_EQ(outCount, 1u);
    EXPECT_EQ(out[0].oldStatus, HealthStatus::kDegraded);
    EXPECT_EQ(out[0].newStatus, HealthStatus::kOK);
}

TEST_F(HealthRegistryTest, TransitionSink_ReceivesCallbacks)
{
    CaptureTransitionSink sink;
    HealthRegistry::Instance().RegisterTransitionSink(&sink);

    MockHealthReporter r(Dia::Core::StringCRC("mod.f"));
    HealthRegistry::Instance().Register(&r);
    r.SetFailing(Dia::Core::StringCRC("err"));

    HealthRegistry::Transition out[32];
    unsigned int outCount = 0;
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);

    EXPECT_EQ(sink.count, 1u);
    EXPECT_EQ(sink.transitions[0].newStatus, HealthStatus::kFailing);

    HealthRegistry::Instance().UnregisterTransitionSink(&sink);
}

TEST_F(HealthRegistryTest, UnregisterTransitionSink_StopsCallbacks)
{
    CaptureTransitionSink sink;
    HealthRegistry::Instance().RegisterTransitionSink(&sink);
    HealthRegistry::Instance().UnregisterTransitionSink(&sink);

    MockHealthReporter r(Dia::Core::StringCRC("mod.g"));
    HealthRegistry::Instance().Register(&r);
    r.SetFailing(Dia::Core::StringCRC("err"));

    HealthRegistry::Transition out[32];
    unsigned int outCount = 0;
    HealthRegistry::Instance().PollTransitions(out, 32, outCount);

    EXPECT_EQ(sink.count, 0u);
}

TEST_F(HealthRegistryTest, MaxCapacity_Accepts64AndDrops65th)
{
    const unsigned int kCount = 65;
    std::deque<MockHealthReporter> reporters;
    for (unsigned int i = 0; i < kCount; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "cap.r%02u", i);
        reporters.emplace_back(Dia::Core::StringCRC(name));
    }

    for (unsigned int i = 0; i < 64; ++i)
        HealthRegistry::Instance().Register(&reporters[i]);

    EXPECT_EQ(HealthRegistry::Instance().GetDroppedCount(), 0u);

    // 65th is dropped without crashing and is counted (a Warning is also logged).
    HealthRegistry::Instance().Register(&reporters[64]);
    EXPECT_EQ(HealthRegistry::Instance().GetDroppedCount(), 1u);

    HealthRegistry::ReporterSnapshot snaps[80];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 80, count);
    EXPECT_EQ(count, 64u);
    for (unsigned int i = 0; i < count; ++i)
        EXPECT_NE(snaps[i].name.Value(), Dia::Core::StringCRC("cap.r64").Value());
}

TEST_F(HealthRegistryTest, DuplicateRegister_Ignored)
{
    MockHealthReporter r(Dia::Core::StringCRC("dup"));
    HealthRegistry::Instance().Register(&r);
    HealthRegistry::Instance().Register(&r);

    HealthRegistry::ReporterSnapshot snaps[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 32, count);
    EXPECT_EQ(count, 1u);
}

TEST_F(HealthRegistryTest, NullRegister_Ignored)
{
    HealthRegistry::Instance().Register(nullptr);

    HealthRegistry::ReporterSnapshot snaps[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 32, count);
    EXPECT_EQ(count, 0u);
}

TEST_F(HealthRegistryTest, ConcurrentSetAndPoll)
{
    MockHealthReporter r(Dia::Core::StringCRC("concurrent"));
    HealthRegistry::Instance().Register(&r);

    std::atomic<bool> done{false};

    std::thread setter([&]()
    {
        for (int i = 0; i < 100; ++i)
        {
            r.SetDegraded(Dia::Core::StringCRC("oops"));
            r.SetOK();
        }
        done.store(true);
    });

    while (!done.load())
    {
        HealthRegistry::Transition out[32];
        unsigned int outCount = 0;
        HealthRegistry::Instance().PollTransitions(out, 32, outCount);
    }

    setter.join();
}
