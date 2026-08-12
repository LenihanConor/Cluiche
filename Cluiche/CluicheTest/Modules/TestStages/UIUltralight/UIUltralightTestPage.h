#pragma once
#include <DiaUI/Page.h>
#include <DiaCore/Strings/String64.h>

namespace Dia { namespace UI { class BoundMethodArgs; class BoundMethodValue; } }

namespace CluicheTest {

class IUIUltralightTestCallbacks
{
public:
    virtual ~IUIUltralightTestCallbacks() = default;
    virtual void OnPageReady() = 0;
    virtual void OnButtonClicked() = 0;
    virtual void ReportReceivedValue(const Dia::UI::BoundMethodArgs& args) = 0;
    virtual void OnSliderChanged(const Dia::UI::BoundMethodArgs& args) = 0;

    // Polled by JS each second: bitmask of checkpoint states
    virtual int  GetStatusFlags() = 0;
    // Returns "frame=N,load=N,trips=N,slider=N" so JS can display live metrics in one call
    virtual Dia::Core::Containers::String64 GetLiveMetrics() = 0;

    // Game Input Bridge test callbacks
    virtual void OnCallJsTest() = 0;
    virtual void OnKeyReceived(const Dia::UI::BoundMethodArgs& args) = 0;
    virtual void OnKeyInUiOnlyReceived(const Dia::UI::BoundMethodArgs& args) = 0;
};

class UIUltralightTestPage : public Dia::UI::Page
{
public:
    explicit UIUltralightTestPage(IUIUltralightTestCallbacks* callbacks);

    void InitializePage();

    static constexpr const char* kTestValue = "dia_test_value_42";

    Dia::UI::BoundMethodValue GetTestValue(const Dia::UI::BoundMethodArgs& args);
    Dia::UI::BoundMethodValue GetStatusFlags_JS(const Dia::UI::BoundMethodArgs& args);
    Dia::UI::BoundMethodValue GetLiveMetrics_JS(const Dia::UI::BoundMethodArgs& args);

private:
    void OnPageReady_JS(const Dia::UI::BoundMethodArgs& args);
    void OnButtonClicked_JS(const Dia::UI::BoundMethodArgs& args);
    void ReportReceivedValue_JS(const Dia::UI::BoundMethodArgs& args);
    void OnSliderChanged_JS(const Dia::UI::BoundMethodArgs& args);
    void OnCallJsTest_JS(const Dia::UI::BoundMethodArgs& args);
    void OnKeyReceived_JS(const Dia::UI::BoundMethodArgs& args);
    void OnKeyInUiOnlyReceived_JS(const Dia::UI::BoundMethodArgs& args);

    IUIUltralightTestCallbacks* mCallbacks;
};

} // namespace CluicheTest
