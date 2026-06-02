#include "Modules/TestStages/UIUltralightTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaInput/EMouseButton.h>
#include <cstdio>

namespace CluicheTest {

const Dia::Core::StringCRC UIUltralightTestStageModule::kTypeId("UIUltralightTestStageModule");

UIUltralightTestStageModule::UIUltralightTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
    , mPage(this)
{}

Dia::Core::StringCRC UIUltralightTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("CluicheTest");
}

const Dia::Core::StringCRC* UIUltralightTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("ui.page_loaded"),
        Dia::Core::StringCRC("ui.js_to_cpp_callback_fired"),
        Dia::Core::StringCRC("ui.round_trip_value_correct"),
        Dia::Core::StringCRC("ui.pixel_buffer_non_empty"),
        Dia::Core::StringCRC("ui.mouse_click_handled"),
        Dia::Core::StringCRC("ui.deterministic_reload"),
    };
    outCount = 6;
    return names;
}

bool UIUltralightTestStageModule::AreDependenciesReady()
{
    auto* ui = mUI.Get();
    return ui && ui->HasStarted();
}

void UIUltralightTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    DIA_LOG_INFO("CluicheTest", "UIUltralightTestStageModule::OnStart (entry %u)", GetEntryCount());

    mPageLoaded          = false;
    mPageReadyFired      = false;
    mButtonClickedFired  = false;
    mRoundTripCorrect    = false;
    mPixelBufferNonEmpty = false;
    mMouseClickHandled   = false;
    mMouseInjected       = false;
    mFramesUntilLoaded   = 0;
    mRoundTripCount      = 0;
    mSliderValue         = 50;

    mPage.InitializePage();
    mUI->LoadPage(mPage);

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricFramesUntilLoaded)
        mMetricFramesUntilLoaded = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.ui.frames_until_loaded"));
    if (!mMetricRoundTripCount)
        mMetricRoundTripCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.ui.round_trip_count"));

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.page_loaded"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPageLoaded, mPageLoaded ? "page loaded" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.js_to_cpp_callback_fired"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool passed = mPageReadyFired && mButtonClickedFired;
            return { passed, passed ? "OnPageReady + OnButtonClicked fired" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.round_trip_value_correct"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRoundTripCorrect, mRoundTripCorrect ? "round-trip match" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.pixel_buffer_non_empty"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPixelBufferNonEmpty, mPixelBufferNonEmpty ? "buffer has non-zero bytes" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.mouse_click_handled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mMouseClickHandled, mMouseClickHandled ? "mouse click handled" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.deterministic_reload"),
        [this]() -> Dia::Automation::CheckpointResult {
            if (!mDeterminismReady) return { false, "second run not complete", 0.0f };
            unsigned int diff = mFramesUntilLoaded > mRun1FramesUntilLoaded
                ? mFramesUntilLoaded - mRun1FramesUntilLoaded
                : mRun1FramesUntilLoaded - mFramesUntilLoaded;
            bool passed = diff <= 1;
            return { passed, passed ? "deterministic" : "frame counts differ", 0.0f };
        });
}

void UIUltralightTestStageModule::OnUpdate(float /*deltaTime*/)
{
    auto* ui = mUI.Get();
    if (!ui) return;

    auto* sys = ui->GetUISystem();
    if (!sys) return;

    // Poll page load
    if (!mPageLoaded && sys->IsPageLoaded())
    {
        mPageLoaded = true;
        mFramesUntilLoaded = GetFrameCount();
        DIA_LOG_INFO("CluicheTest", "Page loaded at frame %u", mFramesUntilLoaded);
        if (mMetricFramesUntilLoaded)
            mMetricFramesUntilLoaded->Set(static_cast<double>(mFramesUntilLoaded));
    }

    if (!mPageLoaded)
        return;

    // Sample pixel buffer (every 1024 bytes to avoid iterating 3.5 MB)
    if (!mPixelBufferNonEmpty)
    {
        Dia::UI::UIDataBuffer buf;
        sys->FetchUIDataBuffer(buf);
        if (buf.GetBufferSize() > 0 && buf.GetBuffer())
        {
            const unsigned char* data = buf.GetBuffer();
            const int stride = 1024;
            const int size   = buf.GetBufferSize();
            for (int i = 0; i < size; i += stride)
            {
                if (data[i] != 0) { mPixelBufferNonEmpty = true; break; }
            }
        }
    }

    // Inject mouse click at the HTML button. Button is in the top-right panel:
    // right:16px, width:290px, top:16px. Centre of button row ≈ y=78.
    // X at panel centre for a 2752-wide window (80% of 3440): 2752-16-145=2591.
    if (!mMouseInjected)
    {
        sys->InjectMouseClick(Dia::Input::EMouseButton::kLeft, 2591, 78);
        mMouseInjected = true;
    }

    // Update metrics
    if (mMetricRoundTripCount)
        mMetricRoundTripCount->Set(static_cast<double>(mRoundTripCount));

    // All 5 non-determinism checkpoints resolved — report pass and save run-1 data
    if (!IsResolved()
        && mPageLoaded
        && mPageReadyFired
        && mButtonClickedFired
        && mRoundTripCorrect
        && mPixelBufferNonEmpty
        && mMouseClickHandled)
    {
        if (mRun1FramesUntilLoaded == 0)
        {
            // First run — save load time for determinism comparison
            mRun1FramesUntilLoaded = mFramesUntilLoaded;
        }
        else
        {
            // Second run — determinism check now possible
            mDeterminismReady = true;
        }
        ReportPassed();
    }
}

void UIUltralightTestStageModule::OnStop()
{
    DIA_LOG_INFO("CluicheTest", "UIUltralightTestStageModule::OnStop");

    if (auto* ui = mUI.Get())
        ui->UnloadPage();

    mMetricFramesUntilLoaded = nullptr;
    mMetricRoundTripCount    = nullptr;
}

// IUIUltralightTestCallbacks

void UIUltralightTestStageModule::OnPageReady()
{
    mPageReadyFired = true;
}

void UIUltralightTestStageModule::OnButtonClicked()
{
    mButtonClickedFired  = true;
    mMouseClickHandled   = true;
}

void UIUltralightTestStageModule::ReportReceivedValue(const Dia::UI::BoundMethodArgs& args)
{
    if (args.Size() > 0 && args.At(0).IsString())
    {
        const auto& received = args.At(0).GetString();
        if (received == Dia::Core::Containers::String64(UIUltralightTestPage::kTestValue))
        {
            mRoundTripCorrect = true;
            mRoundTripCount++;
        }
    }
}

void UIUltralightTestStageModule::OnSliderChanged(const Dia::UI::BoundMethodArgs& args)
{
    if (args.Size() > 0 && args.At(0).IsDouble())
        mSliderValue = static_cast<int>(args.At(0).GetDouble());
    else if (args.Size() > 0 && args.At(0).IsInteger())
        mSliderValue = args.At(0).GetInteger();
}

int UIUltralightTestStageModule::GetStatusFlags()
{
    int flags = 0;
    if (mPageLoaded)          flags |= (1 << 0);
    if (mPageReadyFired)      flags |= (1 << 1);
    if (mButtonClickedFired)  flags |= (1 << 2);
    if (mRoundTripCorrect)    flags |= (1 << 3);
    if (mPixelBufferNonEmpty) flags |= (1 << 4);
    if (mMouseClickHandled)   flags |= (1 << 5);
    return flags;
}

Dia::Core::Containers::String64 UIUltralightTestStageModule::GetLiveMetrics()
{
    // Format: "frame=N,load=N,trips=N,slider=N"
    // String64 is 64 chars — keep values compact.
    char buf[64];
    snprintf(buf, sizeof(buf), "frame=%u,load=%u,trips=%u,slider=%d",
        GetFrameCount(), mFramesUntilLoaded, mRoundTripCount, mSliderValue);
    return Dia::Core::Containers::String64(buf);
}

} // namespace CluicheTest

namespace { using UIUltralightTestStageModule_ = CluicheTest::UIUltralightTestStageModule; }
DIA_MODULE(UIUltralightTestStageModule_);
DIA_DESCRIBE(UIUltralightTestStageModule_::kTypeId, "Test stage for the Ultralight UI system: page loading, JS bridge, and layout testing.");
