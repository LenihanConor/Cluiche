#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Cluiche { namespace AppFlow {

class DebugUIModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit DebugUIModule(const Dia::Core::StringCRC& instanceId);

    bool IsFrameActive() const { return mFrameActive; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
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
