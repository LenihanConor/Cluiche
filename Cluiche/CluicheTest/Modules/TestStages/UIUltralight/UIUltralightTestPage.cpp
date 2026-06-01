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

} // namespace CluicheTest
