////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingUnitFixture.h
//
// Shared GoogleTest fixture for ProcessingUnit integration tests.
// Eliminates the repeated inline boilerplate found across unit test files
// and provides a stable harness for multi-phase, multi-module scenarios.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <gtest/gtest.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaApplicationFlow/ApplicationStateObject.h>

namespace TestFixtures
{
    //--------------------------------------------------------------------------
    // MinimalPU — concrete ProcessingUnit that stops after N updates
    //--------------------------------------------------------------------------
    class MinimalPU : public Dia::Application::ProcessingUnit
    {
    public:
        explicit MinimalPU(const char* id = "TestPU", int maxUpdates = 0)
            : ProcessingUnit(Dia::Core::StringCRC(id), -1.0f, 16, 16)
            , mUpdateCount(0)
            , mMaxUpdates(maxUpdates)
        {}

        void SetMaxUpdates(int n) { mMaxUpdates = n; mUpdateCount = 0; }

        virtual bool FlaggedToStopUpdating() const override
        {
            if (mMaxUpdates <= 0) return true;
            return (mUpdateCount++ >= mMaxUpdates);
        }

    private:
        mutable int mUpdateCount;
        int mMaxUpdates;
    };

    //--------------------------------------------------------------------------
    // MinimalPhase — concrete Phase; always stops updating (single-step)
    //--------------------------------------------------------------------------
    class MinimalPhase : public Dia::Application::Phase
    {
    public:
        MinimalPhase(Dia::Application::ProcessingUnit* pu, const char* id)
            : Phase(pu, Dia::Core::StringCRC(id))
        {}

        virtual bool FlaggedToStopUpdating() const override { return true; }
    };

    //--------------------------------------------------------------------------
    // CountingModule — records every lifecycle event for assertion
    //--------------------------------------------------------------------------
    class CountingModule : public Dia::Application::Module
    {
    public:
        CountingModule(Dia::Application::ProcessingUnit* pu, const char* id,
                       RunningEnum mode = RunningEnum::kUpdate)
            : Module(pu, Dia::Core::StringCRC(id), mode)
            , startCount(0)
            , updateCount(0)
            , stopCount(0)
            , retainCount(0)
        {}

        int startCount;
        int updateCount;
        int stopCount;
        int retainCount;

        virtual Dia::Application::StateObject::OpertionResponse DoStart(
            const Dia::Application::StateObject::IStartData*) override
        {
            startCount++;
            return Dia::Application::StateObject::OpertionResponse::kImmediate;
        }

        virtual void DoUpdate() override { updateCount++; }
        virtual void DoStop()   override { stopCount++; }

        virtual void DoRetainThroughTransition(
            const Dia::Application::Phase*, const Dia::Application::Phase*) override
        {
            retainCount++;
        }
    };

    //--------------------------------------------------------------------------
    // ProcessingUnitFixture — base class for integration tests
    //
    // Subclass this and override SetUp/TearDown as needed.
    // Provides a single-phase PU wired and ready to Start/Update/Stop.
    //--------------------------------------------------------------------------
    class ProcessingUnitFixture : public ::testing::Test
    {
    protected:
        ProcessingUnitFixture()
            : pu("FixturePU")
            , phase(&pu, "FixturePhase")
        {}

        virtual void SetUp() override
        {
            pu.AddModule(&module);
            phase.AddModule(&module);
            pu.AddPhase(&phase);
            pu.SetInitialPhase(&phase);
            pu.Initialize();
        }

        virtual void TearDown() override
        {
            if (pu.HasStarted())
                pu.Stop();
        }

        MinimalPU      pu;
        MinimalPhase   phase;
        CountingModule module{ &pu, "FixtureModule" };
    };

} // namespace TestFixtures
