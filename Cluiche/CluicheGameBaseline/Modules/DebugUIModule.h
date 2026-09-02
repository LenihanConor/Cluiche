#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Cluiche { namespace AppFlow {

class DebugUIModule : public Dia::ApplicationFlow::RenderModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "ImGui frame begin/end for render-thread debug UI";
    explicit DebugUIModule(const Dia::Core::StringCRC& instanceId);

    bool IsFrameActive() const { return mFrameActive; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    class HealthReporter : public Dia::Observation::Health::HealthReporterBase {
    public:
        Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("DebugUIModule"); }
    };

    bool mFrameActive = false;
    HealthReporter mHealth;
};

} } // namespace Cluiche::AppFlow
