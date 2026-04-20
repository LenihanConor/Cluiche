////////////////////////////////////////////////////////////////////////////////
// Filename: FakeInputSource.h
//
// Test double for IInputSource. Pre-load synthetic events with QueueEvent()
// and they will be emitted on the next Poll() call.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaInput/IInputSource.h>
#include <DiaInput/EventData.h>
#include <DiaInput/Event.h>

#include <vector>

namespace TestFixtures
{
    class FakeInputSource : public Dia::Input::IInputSource
    {
    public:
        FakeInputSource(Priority priority = Priority::Normal)
            : IInputSource(priority)
            , mPollCallCount(0)
            , mStartFrameCallCount(0)
            , mEndFrameCallCount(0)
        {}

        // Pre-load an event to be emitted on the next Poll()
        void QueueEvent(const Dia::Input::Event& e)
        {
            mPendingEvents.push_back(e);
        }

        // Convenience: queue a key-press event
        void QueueKeyPress(int keyCode)
        {
            Dia::Input::Event e;
            e.type = Dia::Input::Event::EType::kKeyPressed;
            e.key.code    = keyCode;
            e.key.alt     = false;
            e.key.control = false;
            e.key.shift   = false;
            e.key.system  = false;
            mPendingEvents.push_back(e);
        }

        // Convenience: queue a key-release event
        void QueueKeyRelease(int keyCode)
        {
            Dia::Input::Event e;
            e.type = Dia::Input::Event::EType::kKeyReleased;
            e.key.code    = keyCode;
            e.key.alt     = false;
            e.key.control = false;
            e.key.shift   = false;
            e.key.system  = false;
            mPendingEvents.push_back(e);
        }

        // Inspection
        int PollCallCount()       const { return mPollCallCount; }
        int StartFrameCallCount() const { return mStartFrameCallCount; }
        int EndFrameCallCount()   const { return mEndFrameCallCount; }

        virtual void StartFrame() override { mStartFrameCallCount++; }
        virtual void EndFrame()   override { mEndFrameCallCount++; }

        virtual void Poll(Dia::Input::EventData& outStream) override
        {
            mPollCallCount++;
            for (const auto& e : mPendingEvents)
                outStream.Add(e);
            mPendingEvents.clear();
        }

    private:
        std::vector<Dia::Input::Event> mPendingEvents;
        int mPollCallCount;
        int mStartFrameCallCount;
        int mEndFrameCallCount;
    };

} // namespace TestFixtures
