////////////////////////////////////////////////////////////////////////////////
// Filename: FakeCanvas.h
//
// Test double for ICanvas. Records every rendering call without touching
// a display — safe to use in headless CI environments.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace TestFixtures
{
    class FakeCanvas : public Dia::Graphics::ICanvas
    {
    public:
        FakeCanvas()
            : mInitializeCallCount(0)
            , mStartFrameCallCount(0)
            , mProcessFrameCallCount(0)
            , mEndFrameCallCount(0)
            , mSetActiveContextCallCount(0)
        {}

        // Inspection
        int InitializeCallCount()      const { return mInitializeCallCount; }
        int StartFrameCallCount()      const { return mStartFrameCallCount; }
        int ProcessFrameCallCount()    const { return mProcessFrameCallCount; }
        int EndFrameCallCount()        const { return mEndFrameCallCount; }
        int TotalRenderCallCount()     const { return mStartFrameCallCount + mProcessFrameCallCount + mEndFrameCallCount; }
        int SetActiveContextCallCount() const { return mSetActiveContextCallCount; }

        const Dia::Graphics::ICanvas::Settings& LastSettings() const { return mLastSettings; }

        virtual void Initialize(const Settings& settings) override
        {
            mInitializeCallCount++;
            mLastSettings = settings;
        }

        virtual void SetCanvasSize(const Dia::Maths::Vector2D&) override {}

        virtual void SetActiveContext(bool) override
        {
            mSetActiveContextCallCount++;
        }

        virtual void StartFrame(const Dia::Graphics::FrameData&) override
        {
            mStartFrameCallCount++;
        }

        virtual void ProcessFrame(const Dia::Graphics::FrameData&) override
        {
            mProcessFrameCallCount++;
        }

        virtual void EndFrame(const Dia::Graphics::FrameData&) override
        {
            mEndFrameCallCount++;
        }

    private:
        int mInitializeCallCount;
        int mStartFrameCallCount;
        int mProcessFrameCallCount;
        int mEndFrameCallCount;
        int mSetActiveContextCallCount;
        Settings mLastSettings;
    };

} // namespace TestFixtures
