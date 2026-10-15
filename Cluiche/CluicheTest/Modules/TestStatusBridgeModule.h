#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "Modules/StageStatusModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

namespace CluicheTest {

// Maps a TestResultsRegistry result into the generic StageStatusModule table used by the boot menu.
class TestStatusBridgeModule : public Dia::ApplicationFlow::RenderModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Publishes TestResultsRegistry results to StageStatusModule for the boot menu";
    explicit TestStatusBridgeModule(const Dia::Core::StringCRC& instanceId);

    // Pure mapping. A null result means the stage has not run. Label is a string literal.
    static Cluiche::AppFlow::StageStatusModule::StageStatus MapResultToStatus(const StageResult* result);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void Publish();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::StageStatusModule> mStageStatus{this, Dia::Core::StringCRC("StageStatus")};
    static constexpr unsigned int kMaxStages = 32;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> mStages;
};

} // namespace CluicheTest
