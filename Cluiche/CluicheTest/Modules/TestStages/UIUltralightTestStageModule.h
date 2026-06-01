#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/UIUltralight/UIUltralightTestPage.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String64.h>
#include "Modules/UIModule.h"

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace CluicheTest {

class UIUltralightTestStageModule
    : public TestStageModuleBase
    , public IUIUltralightTestCallbacks
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Validates DiaUIUltralight: page load, JS-C++ round-trip, pixel buffer, mouse input";

    explicit UIUltralightTestStageModule(const Dia::Core::StringCRC& instanceId);

    // IUIUltralightTestCallbacks
    void OnPageReady() override;
    void OnButtonClicked() override;
    void ReportReceivedValue(const Dia::UI::BoundMethodArgs& args) override;
    void OnSliderChanged(const Dia::UI::BoundMethodArgs& args) override;
    int  GetStatusFlags() override;
    Dia::Core::Containers::String64 GetLiveMetrics() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    bool PersistsAcrossEntries() const override { return true; }

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::UIModule> mUI{this};
    UIUltralightTestPage mPage{this};

    bool mPageLoaded          = false;
    bool mPageReadyFired      = false;
    bool mButtonClickedFired  = false;
    bool mRoundTripCorrect    = false;
    bool mPixelBufferNonEmpty = false;
    bool mMouseClickHandled   = false;
    bool mMouseInjected       = false;

    unsigned int mFramesUntilLoaded = 0;
    unsigned int mRoundTripCount    = 0;
    int          mSliderValue       = 50;

    // Persists across entries for determinism check
    unsigned int mRun1FramesUntilLoaded = 0;
    bool         mDeterminismReady      = false;

    Dia::Observation::Metric::Gauge* mMetricFramesUntilLoaded = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRoundTripCount    = nullptr;
};

} // namespace CluicheTest
