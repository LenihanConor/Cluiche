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
    mAwaitingCapture = false;
    mCaptureWasPassed = false;
    mCaptureFrameTarget = 0;

    unsigned int checkpointCount = 0;
    const auto* checkpointNames = GetCheckpointNames(checkpointCount);
    TestResultsRegistry::GetInstance().SetRunning(
        GetStageName(), GetBudgetFrames(), checkpointNames, checkpointCount);

    OnStart(&mAutomationServiceStream->Get());

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageModuleBase::DoUpdate(float deltaTime)
{
    if (!mResolved)
    {
        ++mFrameCount;
        TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);
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
}

Dia::ApplicationFlow::StopResult TestStageModuleBase::DoStop()
{
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
}

} // namespace CluicheTest
