#include <gtest/gtest.h>

#include <DiaCore/Frame/FrameStream.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Interface/ICanvas.h>

#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

#include "Fixtures/FakeCanvas.h"
#include "Fixtures/ProcessingUnitFixture.h"

#include <thread>
#include <atomic>
#include <chrono>

using namespace Dia::Application;
using namespace Dia::Core;
using namespace TestFixtures;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

class FrameTestPU : public ProcessingUnit
{
public:
    FrameTestPU(const char* id = "FrameTestPU")
        : ProcessingUnit(StringCRC(id), -1.0f, 16, 16)
        , mUpdateCount(0), mMaxUpdates(0) {}

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

class FrameTestPhase : public Phase
{
public:
    FrameTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

// Module that writes a FrameData into a shared FrameStream each update
class FrameProducerModule : public Module
{
public:
    FrameProducerModule(ProcessingUnit* pu,
                        Dia::Core::FrameStream<Dia::Graphics::FrameData>* stream)
        : Module(pu, StringCRC("FrameProducer"), RunningEnum::kUpdate)
        , mStream(stream)
        , mFrameCount(0)
    {}

    int GetFrameCount() const { return mFrameCount; }

    virtual void DoUpdate() override
    {
        Dia::Graphics::FrameData frame;
        // Each successive timestamp must be strictly greater
        auto ts = Dia::Core::TimeAbsolute::CreateFromMilliseconds(++mFrameCount * 16);
        mStream->InsertCopyOfDataToStream(frame, ts);
    }

private:
    Dia::Core::FrameStream<Dia::Graphics::FrameData>* mStream;
    int mFrameCount;
};

// Module that reads from the FrameStream and renders via ICanvas
class FrameConsumerModule : public Module
{
public:
    FrameConsumerModule(ProcessingUnit* pu,
                        Dia::Core::FrameStream<Dia::Graphics::FrameData>* stream,
                        Dia::Graphics::ICanvas* canvas)
        : Module(pu, StringCRC("FrameConsumer"), RunningEnum::kUpdate)
        , mStream(stream)
        , mCanvas(canvas)
        , mRenderCount(0)
    {}

    int GetRenderCount() const { return mRenderCount; }

    virtual void DoUpdate() override
    {
        Dia::Core::TimeAbsolute ts = Dia::Core::TimeAbsolute::Zero();
        const Dia::Graphics::FrameData* frame = mStream->FetchLatestData(ts);
        if (frame != nullptr)
        {
            mCanvas->RenderFrame(*frame);
            mRenderCount++;
        }
    }

private:
    Dia::Core::FrameStream<Dia::Graphics::FrameData>* mStream;
    Dia::Graphics::ICanvas* mCanvas;
    int mRenderCount;
};

//-----------------------------------------------------------------------------
// Tests: FakeCanvas
//-----------------------------------------------------------------------------

TEST(FrameStreamPipeline, FakeCanvasRecordsRenderFrame)
{
    FakeCanvas canvas;
    Dia::Graphics::FrameData frame;
    canvas.RenderFrame(frame);

    EXPECT_EQ(canvas.StartFrameCallCount(), 1);
    EXPECT_EQ(canvas.ProcessFrameCallCount(), 1);
    EXPECT_EQ(canvas.EndFrameCallCount(), 1);
    EXPECT_EQ(canvas.TotalRenderCallCount(), 3);
}

TEST(FrameStreamPipeline, FakeCanvasRecordsInitialize)
{
    FakeCanvas canvas;
    Dia::Graphics::ICanvas::Settings settings;
    canvas.Initialize(settings);

    EXPECT_EQ(canvas.InitializeCallCount(), 1);
}

//-----------------------------------------------------------------------------
// Tests: FrameStream core behaviour
//-----------------------------------------------------------------------------

TEST(FrameStreamPipeline, InsertAndFetchLatest)
{
    Dia::Core::FrameStream<int> stream;

    auto t1 = Dia::Core::TimeAbsolute::CreateFromMilliseconds(10);
    auto t2 = Dia::Core::TimeAbsolute::CreateFromMilliseconds(20);

    stream.InsertCopyOfDataToStream(111, t1);
    stream.InsertCopyOfDataToStream(222, t2);

    Dia::Core::TimeAbsolute outTs = Dia::Core::TimeAbsolute::Zero();
    const int* value = stream.FetchLatestData(outTs);

    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 222);
    EXPECT_EQ(outTs, t2);
}

TEST(FrameStreamPipeline, FetchLatestReturnsNullOnEmptyStream)
{
    Dia::Core::FrameStream<int> stream;
    Dia::Core::TimeAbsolute outTs = Dia::Core::TimeAbsolute::Zero();
    const int* value = stream.FetchLatestData(outTs);
    EXPECT_EQ(value, nullptr);
}

TEST(FrameStreamPipeline, FetchClosestToTime)
{
    Dia::Core::FrameStream<int> stream;
    stream.InsertCopyOfDataToStream(10, Dia::Core::TimeAbsolute::CreateFromMilliseconds(10));
    stream.InsertCopyOfDataToStream(20, Dia::Core::TimeAbsolute::CreateFromMilliseconds(20));
    stream.InsertCopyOfDataToStream(30, Dia::Core::TimeAbsolute::CreateFromMilliseconds(30));

    Dia::Core::TimeAbsolute outTs = Dia::Core::TimeAbsolute::Zero();
    // Query at 15ms — closest without exceeding is at 10ms
    const int* value = stream.FetchDataClosestToTime(
        Dia::Core::TimeAbsolute::CreateFromMilliseconds(15), outTs);

    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 10);
}

TEST(FrameStreamPipeline, GarbageCollectRemovesOldFrames)
{
    Dia::Core::FrameStream<int> stream;
    stream.InsertCopyOfDataToStream(1, Dia::Core::TimeAbsolute::CreateFromMilliseconds(10));
    stream.InsertCopyOfDataToStream(2, Dia::Core::TimeAbsolute::CreateFromMilliseconds(20));
    stream.InsertCopyOfDataToStream(3, Dia::Core::TimeAbsolute::CreateFromMilliseconds(30));

    // Collect everything up to and including 20ms
    stream.GarbageCollectAllFramesOlderThan(
        Dia::Core::TimeAbsolute::CreateFromMilliseconds(20));

    Dia::Core::TimeAbsolute outTs = Dia::Core::TimeAbsolute::Zero();
    const int* latest = stream.FetchLatestData(outTs);
    ASSERT_NE(latest, nullptr);
    EXPECT_EQ(*latest, 3);

    // Frame at 10ms and 20ms should be gone; only 30ms remains
    Dia::Core::Containers::DynamicArrayC<const int*, 32> buffer;
    stream.FetchAllDataUpToTime(
        Dia::Core::TimeAbsolute::CreateFromMilliseconds(25), buffer);
    EXPECT_EQ(buffer.Size(), 0u);  // 20ms was collected; 30ms > 25ms
}

//-----------------------------------------------------------------------------
// Tests: Producer/Consumer modules sharing a FrameStream via FakeCanvas
//-----------------------------------------------------------------------------

TEST(FrameStreamPipeline, ProducerWritesConsumerRenders)
{
    Dia::Core::FrameStream<Dia::Graphics::FrameData> sharedStream;
    FakeCanvas canvas;

    FrameTestPU pu;
    FrameTestPhase phase(&pu, StringCRC("Phase"));

    FrameProducerModule producer(&pu, &sharedStream);
    FrameConsumerModule consumer(&pu, &sharedStream, &canvas);

    phase.AddModule(&producer);
    phase.AddModule(&consumer);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    pu.SetMaxUpdates(3);
    pu.Update();

    EXPECT_EQ(producer.GetFrameCount(), 3);
    // Consumer reads after producer writes in same update cycle;
    // first update produces frame 1, consumer reads it → 3 renders total
    EXPECT_EQ(consumer.GetRenderCount(), 3);
    EXPECT_EQ(canvas.TotalRenderCallCount(), 9);  // 3 per RenderFrame call

    pu.Stop();
}

TEST(FrameStreamPipeline, ConsumerHandlesEmptyStreamGracefully)
{
    Dia::Core::FrameStream<Dia::Graphics::FrameData> sharedStream;
    FakeCanvas canvas;

    FrameTestPU pu;
    FrameTestPhase phase(&pu, StringCRC("Phase"));

    // Consumer only — no producer
    FrameConsumerModule consumer(&pu, &sharedStream, &canvas);

    phase.AddModule(&consumer);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    pu.SetMaxUpdates(2);
    pu.Update();

    EXPECT_EQ(consumer.GetRenderCount(), 0);
    EXPECT_EQ(canvas.TotalRenderCallCount(), 0);

    pu.Stop();
}

//-----------------------------------------------------------------------------
// Tests: Thread-safety — writer on background thread, reader on main thread
//-----------------------------------------------------------------------------

TEST(FrameStreamPipeline, ConcurrentWriteReadDoesNotCorrupt)
{
    Dia::Core::FrameStream<int> stream;
    std::atomic<bool> stop{ false };
    std::atomic<int>  writeCount{ 0 };
    std::atomic<int>  readCount{ 0 };

    // Writer thread: inserts 100 frames with strictly increasing timestamps
    std::thread writer([&]()
    {
        for (int i = 1; i <= 100 && !stop.load(); ++i)
        {
            auto ts = Dia::Core::TimeAbsolute::CreateFromMilliseconds(i * 10);
            stream.InsertCopyOfDataToStream(i, ts);
            writeCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Reader thread: fetches latest while writer is running
    std::thread reader([&]()
    {
        for (int attempt = 0; attempt < 100 && !stop.load(); ++attempt)
        {
            Dia::Core::TimeAbsolute outTs = Dia::Core::TimeAbsolute::Zero();
            const int* value = stream.FetchLatestData(outTs);
            if (value != nullptr)
                readCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(75));
        }
    });

    writer.join();
    reader.join();
    stop = true;

    EXPECT_EQ(writeCount.load(), 100);
    // At least one read succeeded (stream had data by then)
    EXPECT_GT(readCount.load(), 0);
}
