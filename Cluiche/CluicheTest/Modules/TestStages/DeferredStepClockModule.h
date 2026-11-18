#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaDeferredStep/IStepKeySource.h>
#include <cstdint>

namespace CluicheTest {

// Key source for DeferredStepTestStage. Advances the step key by one per sim step.
//
// Manifest ordering rules (deferred_step_test_stage.diaapp, SimPU module list):
//   1. DeferredStepClockModule is listed BEFORE DeferredStepModule. DeferredStepModule reads the key once per
//      DoUpdate and calls Tick(key); the key must already have advanced this sim step or a due apply slips one step.
//   2. DeferredStepTestStageModule (the consumer) is listed AFTER DeferredStepModule. Stop runs in reverse list
//      order, so the consumer unregisters its channel while the scheduler is still alive
//      (DeferredStepModule::DoStop asserts no channels remain).
class DeferredStepClockModule : public Dia::ApplicationFlow::SimModule, public Dia::DeferredStep::IStepKeySource
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Test key source: advances the deferred-step key once per sim step";

    explicit DeferredStepClockModule(const Dia::Core::StringCRC& instanceId);

    Dia::DeferredStep::StepKey GetCurrentKey() const override { return mKey; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::DeferredStep::StepKey mKey = 0;
};

} // namespace CluicheTest
