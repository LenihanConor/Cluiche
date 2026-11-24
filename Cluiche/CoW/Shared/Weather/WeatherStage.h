// The weather world-step stage: WorldStepStageBase instantiated over the Draoi weather simulation. The hook bodies are
// stubs in the scaffold (plan Task 1); Tasks 2-4 fill them in.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md
#pragma once

#include <DraoiHexWorldWeather/WeatherSimulation.h>
#include <Shared/Weather/WeatherCause.h>
#include <Shared/Weather/WeatherEntry.h>
#include <Shared/Weather/WeatherLatch.h>
#include <Shared/WorldStep/WorldStepStageBase.h>

namespace CoW {
namespace Weather {

typedef WorldStep::WorldStepStageBase<Draoi::HexWorld::Weather::WeatherSimulation, WeatherEntry, WeatherLatch>
    WeatherStageBase;

constexpr const char* kLogChannel = "WTH";

// The hook implementation. Constructed before the base (see HooksHolder) so the base can bind to it.
class WeatherHooks : public WeatherStageBase::Hooks
{
public:
    bool InitialiseSim(Draoi::HexWorld::Weather::WeatherSimulation& sim, const WorldStep::WorldView& world,
                       WeatherStageBase::CauseCode& cause) override;
    bool LatchInputs(WeatherLatch& latch, Dia::DeferredStep::StepKey key) override;
    void ApplyEntry(Draoi::HexWorld::Weather::WeatherSimulation& working, WeatherLatch& latch,
                    const WeatherEntry& entry) override;
    void RunStep(Draoi::HexWorld::Weather::WeatherSimulation& working, const WeatherLatch& latch,
                 Dia::ParallelExecutor::IParallelExecutor& executor) override;
    void PublishCommit(const Draoi::HexWorld::Weather::WeatherSimulation& committed, uint32_t stepDay) override;
};

namespace Detail {
struct HooksHolder
{
    WeatherHooks hooks;
};
} // namespace Detail

class WeatherStage : private Detail::HooksHolder, public WeatherStageBase
{
public:
    WeatherStage();
};

} // namespace Weather
} // namespace CoW
