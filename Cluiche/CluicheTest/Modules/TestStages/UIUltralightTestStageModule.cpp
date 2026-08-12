#include "Modules/TestStages/UIUltralightTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaInput/EMouseButton.h>
#include <DiaInput/EKey.h>
#include <DiaInput/InputRouter.h>
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
        Dia::Core::StringCRC("ui.call_js_observed"),
        Dia::Core::StringCRC("ui.key_event_handled"),
        Dia::Core::StringCRC("ui.mode_stack_transitions"),
        Dia::Core::StringCRC("ui.keyboard_suppressed_in_ui_only"),
    };
    outCount = 10;
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

    mCallJsObserved             = false;
    mKeyEventHandled            = false;
    mModeTransitionsOk          = false;
    mKeyboardSuppressedInUiOnly = false;
    mCallJsTriggered            = false;
    mKeyInjected                = false;
    mModeTransitionsTested      = false;
    mUiOnlyKeyInjected          = false;

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

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.call_js_observed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCallJsObserved, mCallJsObserved ? "CallJSFunction→JS→C++ callback confirmed" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.key_event_handled"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mKeyEventHandled, mKeyEventHandled ? "InjectKeyDown reached JS" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.mode_stack_transitions"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mModeTransitionsOk, mModeTransitionsOk ? "InputRouter push/pop correct" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ui.keyboard_suppressed_in_ui_only"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mKeyboardSuppressedInUiOnly, mKeyboardSuppressedInUiOnly ? "keyboard reached UI in kUIOnly mode" : "pending", 0.0f };
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

    // Inject mouse click at the HTML button
    if (!mMouseInjected)
    {
        sys->InjectMouseClick(Dia::Input::EMouseButton::kLeft, 2591, 78);
        mMouseInjected = true;
    }

    // Mode stack transitions — pure C++ test of InputRouter, runs once after page loads
    if (!mModeTransitionsTested)
    {
        Dia::Input::InputRouter testRouter;
        bool ok = true;
        ok &= (testRouter.GetCurrentInputMode() == Dia::Input::EInputRouting::kGameOnly);
        testRouter.PushInputMode(Dia::Input::EInputRouting::kUIOnly);
        ok &= (testRouter.GetCurrentInputMode() == Dia::Input::EInputRouting::kUIOnly);
        testRouter.PushInputMode(Dia::Input::EInputRouting::kGameAndUI);
        ok &= (testRouter.GetCurrentInputMode() == Dia::Input::EInputRouting::kGameAndUI);
        testRouter.PopInputMode();
        ok &= (testRouter.GetCurrentInputMode() == Dia::Input::EInputRouting::kUIOnly);
        testRouter.PopInputMode();
        ok &= (testRouter.GetCurrentInputMode() == Dia::Input::EInputRouting::kGameOnly);
        mModeTransitionsOk = ok;
        mModeTransitionsTested = true;
    }

    // CallJSFunction test — call testCallJs() which triggers app.OnCallJsTest()
    if (!mCallJsTriggered)
    {
        sys->CallJSFunction("testCallJs", "");
        mCallJsTriggered = true;
    }

    // Key injection test — inject Return once CallJSFunction test is done
    if (mCallJsObserved && !mKeyInjected)
    {
        sys->InjectKeyDown(Dia::Input::EKey(Dia::Input::EKey::Return), 0);
        mKeyInjected = true;
    }

    // Keyboard-in-UIOnly test — push kUIOnly, inject Tab, JS should fire OnKeyInUiOnlyReceived
    if (mKeyEventHandled && !mUiOnlyKeyInjected)
    {
        ui->PushInputMode(Dia::Input::EInputRouting::kUIOnly);
        sys->InjectKeyDown(Dia::Input::EKey(Dia::Input::EKey::Tab), 0);
        mUiOnlyKeyInjected = true;
    }

    // Update metrics
    if (mMetricRoundTripCount)
        mMetricRoundTripCount->Set(static_cast<double>(mRoundTripCount));

    // All 5 original non-determinism checkpoints + 4 bridge checkpoints resolved
    if (!IsResolved()
        && mPageLoaded
        && mPageReadyFired
        && mButtonClickedFired
        && mRoundTripCorrect
        && mPixelBufferNonEmpty
        && mMouseClickHandled
        && mCallJsObserved
        && mKeyEventHandled
        && mModeTransitionsOk
        && mKeyboardSuppressedInUiOnly)
    {
        if (mRun1FramesUntilLoaded == 0)
        {
            mRun1FramesUntilLoaded = mFramesUntilLoaded;
        }
        else
        {
            mDeterminismReady = true;
        }
        ReportPassed();
    }
}

void UIUltralightTestStageModule::OnStop()
{
    DIA_LOG_INFO("CluicheTest", "UIUltralightTestStageModule::OnStop");

    // Pop any routing mode pushed during testing
    auto* ui = mUI.Get();
    if (ui && mUiOnlyKeyInjected)
        ui->PopInputMode();

    if (ui)
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

void UIUltralightTestStageModule::OnCallJsTest()
{
    mCallJsObserved = true;
}

void UIUltralightTestStageModule::OnKeyReceived(const Dia::UI::BoundMethodArgs& /*args*/)
{
    mKeyEventHandled = true;
}

void UIUltralightTestStageModule::OnKeyInUiOnlyReceived(const Dia::UI::BoundMethodArgs& /*args*/)
{
    auto* ui = mUI.Get();
    if (ui && ui->GetCurrentInputMode() == Dia::Input::EInputRouting::kUIOnly)
        mKeyboardSuppressedInUiOnly = true;
    // Pop the UIOnly mode pushed in OnUpdate
    if (ui)
        ui->PopInputMode();
    mUiOnlyKeyInjected = false; // prevent double pop in OnStop
}

int UIUltralightTestStageModule::GetStatusFlags()
{
    int flags = 0;
    if (mPageLoaded)                  flags |= (1 << 0);
    if (mPageReadyFired)              flags |= (1 << 1);
    if (mButtonClickedFired)          flags |= (1 << 2);
    if (mRoundTripCorrect)            flags |= (1 << 3);
    if (mPixelBufferNonEmpty)         flags |= (1 << 4);
    if (mMouseClickHandled)           flags |= (1 << 5);
    if (mCallJsObserved)              flags |= (1 << 6);
    if (mKeyEventHandled)             flags |= (1 << 7);
    if (mModeTransitionsOk)           flags |= (1 << 8);
    if (mKeyboardSuppressedInUiOnly)  flags |= (1 << 9);
    return flags;
}

Dia::Core::Containers::String64 UIUltralightTestStageModule::GetLiveMetrics()
{
    char buf[64];
    snprintf(buf, sizeof(buf), "frame=%u,load=%u,trips=%u,slider=%d",
        GetFrameCount(), mFramesUntilLoaded, mRoundTripCount, mSliderValue);
    return Dia::Core::Containers::String64(buf);
}

} // namespace CluicheTest

namespace { using UIUltralightTestStageModule_ = CluicheTest::UIUltralightTestStageModule; }
DIA_MODULE(UIUltralightTestStageModule_);
DIA_DESCRIBE(UIUltralightTestStageModule_::kTypeId, "Test stage for the Ultralight UI system: page loading, JS bridge, keyboard injection, and input routing.");
