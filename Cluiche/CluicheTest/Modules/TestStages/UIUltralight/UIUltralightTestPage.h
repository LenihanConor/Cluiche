#pragma once
#include <DiaUI/Page.h>

namespace Dia { namespace UI { struct BoundMethodArgs; class BoundMethodValue; } }

namespace CluicheTest {

class IUIUltralightTestCallbacks
{
public:
    virtual ~IUIUltralightTestCallbacks() = default;
    virtual void OnPageReady() = 0;
    virtual void OnButtonClicked() = 0;
    virtual void ReportReceivedValue(const Dia::UI::BoundMethodArgs& args) = 0;
};

class UIUltralightTestPage : public Dia::UI::Page
{
public:
    explicit UIUltralightTestPage(IUIUltralightTestCallbacks* callbacks);

    void InitializePage();

    static constexpr const char* kTestValue = "dia_test_value_42";

    Dia::UI::BoundMethodValue GetTestValue(const Dia::UI::BoundMethodArgs& args);

private:
    void OnPageReady_JS(const Dia::UI::BoundMethodArgs& args);
    void OnButtonClicked_JS(const Dia::UI::BoundMethodArgs& args);
    void ReportReceivedValue_JS(const Dia::UI::BoundMethodArgs& args);

    IUIUltralightTestCallbacks* mCallbacks;
};

} // namespace CluicheTest
