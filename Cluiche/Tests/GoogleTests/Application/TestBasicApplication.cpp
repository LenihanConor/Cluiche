#include <gtest/gtest.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>

using namespace Dia::Application;
using namespace Dia::Core;

// Basic test - just verify compilation and linkage
TEST(BasicApplication, CanInstantiateClasses)
{
    // Test that the basic DiaApplication classes can be instantiated
    // This verifies that the libs are linked correctly

    class TestPU : public ProcessingUnit
    {
    public:
        TestPU() : ProcessingUnit(StringCRC("TestPU"), -1.0f, 16, 16) {}
        virtual bool FlaggedToStopUpdating() const override { return true; }  // Always stop immediately
    };

    class TestMod : public Module
    {
    public:
        TestMod(ProcessingUnit* pu) : Module(pu, StringCRC("TestMod"), RunningEnum::kUpdate) {}
        virtual StateObject::OpertionResponse DoStart(const IStartData*) override { return StateObject::OpertionResponse::kImmediate; }
        virtual void DoUpdate() override {}
        virtual void DoStop() override {}
    };

    class TestPh : public Phase
    {
    public:
        TestPh(ProcessingUnit* pu) : Phase(pu, StringCRC("TestPh")) {}
        virtual bool FlaggedToStopUpdating() const override { return true; }  // Always stop immediately
    };

    TestPU pu;
    TestMod mod(&pu);
    TestPh phase(&pu);

    EXPECT_TRUE(true);  // If we got here, instantiation worked
}

TEST(BasicApplication, MessageBusExists)
{
    class TestPU : public ProcessingUnit
    {
    public:
        TestPU() : ProcessingUnit(StringCRC("TestPU"), -1.0f, 16, 16) {}
        virtual bool FlaggedToStopUpdating() const override { return true; }
    };

    TestPU pu;
    MessageBus& bus = pu.GetMessageBus();

    // Just verify we can get the message bus
    EXPECT_EQ(bus.GetQueuedMessageCount(), 0u);
}

TEST(BasicApplication, ErrorHandlingExists)
{
    class TestPU : public ProcessingUnit
    {
    public:
        TestPU() : ProcessingUnit(StringCRC("TestPU"), -1.0f, 16, 16) {}
        virtual bool FlaggedToStopUpdating() const override { return true; }
    };

    TestPU pu;

    // Verify error history starts empty
    EXPECT_EQ(pu.GetErrorHistory().size(), 0u);

    // Report an error
    ErrorInfo error(ErrorCode::kUnknown, StringCRC("Test"), "Test error");
    pu.ReportError(error);

    // Verify it was recorded
    EXPECT_EQ(pu.GetErrorHistory().size(), 1u);
    EXPECT_EQ(pu.GetErrorHistory()[0].code, ErrorCode::kUnknown);
}
