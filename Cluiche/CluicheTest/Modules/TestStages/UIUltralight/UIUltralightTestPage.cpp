#include "Modules/TestStages/UIUltralight/UIUltralightTestPage.h"
#include <DiaUI/BoundMethod.h>
#include <DiaCore/Strings/String64.h>

namespace CluicheTest {

UIUltralightTestPage::UIUltralightTestPage(IUIUltralightTestCallbacks* callbacks)
    : Dia::UI::Page()
    , mCallbacks(callbacks)
{}

void UIUltralightTestPage::InitializePage()
{
    Initialize(Dia::Core::FilePath("stage_ui", "ui_ultralight_test.html"));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnPageReady",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnPageReady_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnButtonClicked",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnButtonClicked_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("GetTestValue",
        Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &UIUltralightTestPage::GetTestValue)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("ReportReceivedValue",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::ReportReceivedValue_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnSliderChanged",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnSliderChanged_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("GetStatusFlags",
        Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &UIUltralightTestPage::GetStatusFlags_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("GetLiveMetrics",
        Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &UIUltralightTestPage::GetLiveMetrics_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnCallJsTest",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnCallJsTest_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnKeyReceived",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnKeyReceived_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnKeyInUiOnlyReceived",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnKeyInUiOnlyReceived_JS)));

    BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("OnKeyInGameAndUiReceived",
        Dia::UI::BoundMethod::MethodPtr(this, &UIUltralightTestPage::OnKeyInGameAndUiReceived_JS)));
}

Dia::UI::BoundMethodValue UIUltralightTestPage::GetTestValue(const Dia::UI::BoundMethodArgs& /*args*/)
{
    return Dia::UI::BoundMethodValue(Dia::Core::Containers::String64(kTestValue));
}

void UIUltralightTestPage::OnPageReady_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    if (mCallbacks)
        mCallbacks->OnPageReady();
}

void UIUltralightTestPage::OnButtonClicked_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    if (mCallbacks)
        mCallbacks->OnButtonClicked();
}

void UIUltralightTestPage::ReportReceivedValue_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks)
        mCallbacks->ReportReceivedValue(args);
}

void UIUltralightTestPage::OnSliderChanged_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks)
        mCallbacks->OnSliderChanged(args);
}

Dia::UI::BoundMethodValue UIUltralightTestPage::GetStatusFlags_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    return Dia::UI::BoundMethodValue(mCallbacks ? mCallbacks->GetStatusFlags() : 0);
}

Dia::UI::BoundMethodValue UIUltralightTestPage::GetLiveMetrics_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    Dia::Core::Containers::String64 s;
    if (mCallbacks)
        s = mCallbacks->GetLiveMetrics();
    return Dia::UI::BoundMethodValue(s);
}

void UIUltralightTestPage::OnCallJsTest_JS(const Dia::UI::BoundMethodArgs& /*args*/)
{
    if (mCallbacks)
        mCallbacks->OnCallJsTest();
}

void UIUltralightTestPage::OnKeyReceived_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks)
        mCallbacks->OnKeyReceived(args);
}

void UIUltralightTestPage::OnKeyInUiOnlyReceived_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks)
        mCallbacks->OnKeyInUiOnlyReceived(args);
}

void UIUltralightTestPage::OnKeyInGameAndUiReceived_JS(const Dia::UI::BoundMethodArgs& args)
{
    if (mCallbacks)
        mCallbacks->OnKeyInGameAndUiReceived(args);
}

} // namespace CluicheTest
