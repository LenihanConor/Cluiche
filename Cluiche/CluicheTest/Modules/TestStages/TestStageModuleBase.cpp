#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaAutomation/AutomationService.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Capture/DiaCapture.h>
#include <DiaCaptureTest/MetricsWriter.h>
#include <filesystem>
#include <windows.h>

// Full instantiation lives here — header only forward-declares.
template class Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>;

namespace CluicheTest {

TestStageModuleBase::TestStageModuleBase(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mAutomationServiceStream(new Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>(this, Dia::Core::StringCRC("AutomationService")))
{}

TestStageModuleBase::~TestStageModuleBase()
{
    delete mAutomationServiceStream;
}

Dia::ApplicationFlow::StartResult TestStageModuleBase::DoStart()
{
    if (!mAutomationServiceStream->IsAvailable())
    {
        if (mStartWaitFrames == 0)
            DIA_LOG_INFO("TestStage", "%s: waiting for AutomationService", GetStageName().AsChar());
        ++mStartWaitFrames;
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    if (!AreDependenciesReady())
    {
        if (mStartWaitFrames == 0)
            DIA_LOG_INFO("TestStage", "%s: waiting for dependencies (AreDependenciesReady=false)", GetStageName().AsChar());
        else if (mStartWaitFrames % 30 == 0)
            DIA_LOG_WARNING("TestStage", "%s: still waiting for dependencies after %u frames", GetStageName().AsChar(), mStartWaitFrames);
        ++mStartWaitFrames;
        return Dia::ApplicationFlow::StartResult::kLoading;
    }

    if (mStartWaitFrames > 0)
        DIA_LOG_INFO("TestStage", "%s: dependencies ready after %u frames, starting", GetStageName().AsChar(), mStartWaitFrames);
    mStartWaitFrames = 0;

    ++mEntryCount;
    mFrameCount = 0;
    mResolved = false;
    mStarted = false;
    mNavigationReleased = false;
    mAwaitingCapture = false;
    mCaptureWasPassed = false;
    mCaptureFrameTarget = 0;
    mAbortRequested.store(false, std::memory_order_relaxed);

    unsigned int checkpointCount = 0;
    const auto* checkpointNames = GetCheckpointNames(checkpointCount);
    TestResultsRegistry::GetInstance().SetRunning(
        GetStageName(), GetBudgetFrames(), checkpointNames, checkpointCount);

    // Attach $lifecycle tap now — only one stage is active at a time, so we
    // stay safely within kMaxTaps. Detach in DoStop.
    if (mLifecycleStore && mLifecycleTap.id == 0)
    {
        mLifecycleTap = mLifecycleStore->AttachTap(
            [this](const void* payload, unsigned int /*size*/, const Dia::Core::StringCRC& /*id*/)
            {
                const auto* ev = static_cast<const Dia::ApplicationFlow::LifecycleEvent*>(payload);
                if (ev->kind == Dia::ApplicationFlow::LifecycleEventKind::kAutomationAbortRequested)
                    mAbortRequested.store(true, std::memory_order_release);
            });
    }

    OnStart(&mAutomationServiceStream->Get());

    mStarted = true;
    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageModuleBase::DoUpdate(float deltaTime)
{
    if (!mResolved)
    {
        ++mFrameCount;
        TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);
    }

    // Check abort flag set by the $lifecycle tap on MainPU. atomic load with
    // acquire pairs with the release store in the tap callback.
    // Guard on mStarted: stale abort from the previous stage's _try_return_boot
    // can arrive before DoStart completes. Releasing navigation hold mid-start
    // triggers a transition while the module is still loading, which crashes.
    if (mStarted && !mResolved && mAbortRequested.load(std::memory_order_acquire))
    {
        mAbortRequested.store(false, std::memory_order_relaxed);
        DIA_LOG_INFO("TestStage", "%s: abort requested — reporting failed at frame %u",
            GetStageName().AsChar(), mFrameCount);
        ReportFailed();
    }

    OnUpdate(deltaTime);

    if (!mResolved && mFrameCount >= GetBudgetFrames())
    {
        mResolved = true;
        OnTimeout();
        TestResultsRegistry::GetInstance().SetTimeout(GetStageName());
    }

    // Once logic is resolved, wait for the render thread to confirm it has presented
    // at least one frame after we requested capture, then fire the actual screenshot.
    if (mAwaitingCapture)
    {
        const Dia::Graphics::RenderFence* fence = mRenderFence.FetchLatest();
        if (fence != nullptr && fence->presentedFrame >= mCaptureFrameTarget)
        {
            FireCapture();
            mAwaitingCapture = false;
        }
    }

    // Single release point for all paths (pass / fail / abort / timeout).
    // Fires after capture is done so the screenshot lands before Boot loads.
    if (mResolved && !mAwaitingCapture && !mNavigationReleased)
    {
        mNavigationReleased = true;
        auto* svc = GetAutomationService();
        if (svc)
        {
            bool ok = false;
            svc->ReleaseNavigationHold(Dia::Core::StringCRC("Boot"), &ok, nullptr);
        }
    }
}

Dia::ApplicationFlow::StopResult TestStageModuleBase::DoStop()
{
    // Detach $lifecycle tap before tearing down — prevents the callback firing
    // on a partially-destroyed module during shutdown.
    if (mLifecycleTap.id != 0 && mLifecycleStore)
    {
        mLifecycleStore->DetachTap(mLifecycleTap);
        mLifecycleTap = Dia::ApplicationFlow::TapHandle{0};
    }
    mAbortRequested.store(false, std::memory_order_relaxed);

    // Fallback: if stage is force-stopped before the fence arrives, capture what's current.
    if (mAwaitingCapture)
    {
        FireCapture();
        mAwaitingCapture = false;
    }

    OnStop();

    if (mAutomationServiceStream->IsAvailable())
        mAutomationServiceStream->Get().UnregisterCheckpoints(this);

    mFrameCount = 0;
    mResolved = false;
    mStarted = false;

    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestStageModuleBase::ReportPassed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetPassed(GetStageName(), mFrameCount);

    // Defer the screenshot until the render thread has presented the frame containing
    // the test's draw output. Read current fence to know the minimum target.
    const Dia::Graphics::RenderFence* fence = mRenderFence.FetchLatest();
    mCaptureFrameTarget = (fence != nullptr ? fence->presentedFrame : 0) + 1;
    mCaptureWasPassed = true;
    mAwaitingCapture = true;
}

void TestStageModuleBase::ReportFailed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetFailed(GetStageName(), mFrameCount);

    const Dia::Graphics::RenderFence* fence = mRenderFence.FetchLatest();
    mCaptureFrameTarget = (fence != nullptr ? fence->presentedFrame : 0) + 1;
    mCaptureWasPassed = false;
    mAwaitingCapture = true;
}

void TestStageModuleBase::FireCapture()
{
    if (mCaptureWasPassed)
        DIA_CAPTURE(GetStageName(), "passed");
    else
        DIA_CAPTURE(GetStageName(), "failed");

    WriteMetrics(mCaptureWasPassed);
}

void TestStageModuleBase::WriteMetrics(bool passed)
{
    const char* tag = GetStageName().AsChar();

    // Build absolute path from exe location: <exeDir>/../../../../out/CluicheTest/captures/metrics/
    char exePath[512] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath) - 1);
    char* lastSlash = strrchr(exePath, '\\');
    if (!lastSlash) lastSlash = strrchr(exePath, '/');
    if (lastSlash) *(lastSlash + 1) = '\0';

    char path[768];
    snprintf(path, sizeof(path), "%s../../../../out/CluicheTest/captures/metrics/%s_metrics.json",
        exePath, tag);

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    Dia::CaptureTest::MetricEntry entries[3];
    entries[0] = { "frame_count", static_cast<float>(mFrameCount) };
    entries[1] = { "passed",      passed ? 1.0f : 0.0f };
    entries[2] = { "entry_count", static_cast<float>(mEntryCount) };

    Dia::CaptureTest::MetricsWriter::Write(path, tag, mFrameCount, entries, 3);
}

Dia::Automation::AutomationService* TestStageModuleBase::GetAutomationService()
{
    return mAutomationServiceStream->IsAvailable() ? &mAutomationServiceStream->Get() : nullptr;
}

void TestStageModuleBase::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAutomationServiceStream->Connect(app);
    mRenderFence.Connect(app);

    // Store the $lifecycle store pointer for use in DoStart/DoStop.
    // We do NOT attach a tap here — OnConnectStreams fires for every stage module
    // at startup, and kMaxTaps is 8. Instead we attach in DoStart and detach in
    // DoStop so only the active stage holds a tap slot.
    using namespace Dia::ApplicationFlow;
    IStreamStore* istore = app.FindStream(Reserved::LifecycleStreamId());
    if (istore)
        mLifecycleStore = static_cast<EventStreamStore<LifecycleEvent>*>(istore);
}

} // namespace CluicheTest
