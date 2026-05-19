#include <gtest/gtest.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaObservation/Health/IHealthTransitionSink.h>
#include <DiaCore/CRC/StringCRC.h>

#include <thread>
#include <atomic>

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

TEST_F(HealthRegistryTest, UnregisterRemovesFromSnapshot)
{
    MockHealthReporter r(Dia::Core::StringCRC("mod.b"));
    HealthRegistry::Instance().Register(&r);
    HealthRegistry::Instance().Unregister(&r);

    HealthRegistry::ReporterSnapshot snaps[32];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 32, count);
    EXPECT_EQ(count, 0u);
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

TEST_F(HealthRegistryTest, MaxCapacity_SilentlyIgnores33rd)
{
    MockHealthReporter reporters[33] = {
        MockHealthReporter(Dia::Core::StringCRC("r00")),
        MockHealthReporter(Dia::Core::StringCRC("r01")),
        MockHealthReporter(Dia::Core::StringCRC("r02")),
        MockHealthReporter(Dia::Core::StringCRC("r03")),
        MockHealthReporter(Dia::Core::StringCRC("r04")),
        MockHealthReporter(Dia::Core::StringCRC("r05")),
        MockHealthReporter(Dia::Core::StringCRC("r06")),
        MockHealthReporter(Dia::Core::StringCRC("r07")),
        MockHealthReporter(Dia::Core::StringCRC("r08")),
        MockHealthReporter(Dia::Core::StringCRC("r09")),
        MockHealthReporter(Dia::Core::StringCRC("r10")),
        MockHealthReporter(Dia::Core::StringCRC("r11")),
        MockHealthReporter(Dia::Core::StringCRC("r12")),
        MockHealthReporter(Dia::Core::StringCRC("r13")),
        MockHealthReporter(Dia::Core::StringCRC("r14")),
        MockHealthReporter(Dia::Core::StringCRC("r15")),
        MockHealthReporter(Dia::Core::StringCRC("r16")),
        MockHealthReporter(Dia::Core::StringCRC("r17")),
        MockHealthReporter(Dia::Core::StringCRC("r18")),
        MockHealthReporter(Dia::Core::StringCRC("r19")),
        MockHealthReporter(Dia::Core::StringCRC("r20")),
        MockHealthReporter(Dia::Core::StringCRC("r21")),
        MockHealthReporter(Dia::Core::StringCRC("r22")),
        MockHealthReporter(Dia::Core::StringCRC("r23")),
        MockHealthReporter(Dia::Core::StringCRC("r24")),
        MockHealthReporter(Dia::Core::StringCRC("r25")),
        MockHealthReporter(Dia::Core::StringCRC("r26")),
        MockHealthReporter(Dia::Core::StringCRC("r27")),
        MockHealthReporter(Dia::Core::StringCRC("r28")),
        MockHealthReporter(Dia::Core::StringCRC("r29")),
        MockHealthReporter(Dia::Core::StringCRC("r30")),
        MockHealthReporter(Dia::Core::StringCRC("r31")),
        MockHealthReporter(Dia::Core::StringCRC("r32"))
    };

    for (int i = 0; i < 33; ++i)
        HealthRegistry::Instance().Register(&reporters[i]);

    HealthRegistry::ReporterSnapshot snaps[33];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, 33, count);
    EXPECT_EQ(count, 32u);
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
