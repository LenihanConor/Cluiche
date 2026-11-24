#include <Shared/Weather/WeatherStage.h>

namespace CoW {
namespace Weather {

using Draoi::HexWorld::Weather::WeatherSimulation;

// Scaffold stubs. Task 2: InitialiseSim; Task 3: LatchInputs, RunStep, PublishCommit; Task 4: ApplyEntry.
bool WeatherHooks::InitialiseSim(WeatherSimulation&, const WorldStep::WorldView&, WeatherStageBase::CauseCode&)
{
    return true;
}

bool WeatherHooks::LatchInputs(WeatherLatch&, Dia::DeferredStep::StepKey)
{
    return true;
}

void WeatherHooks::ApplyEntry(WeatherSimulation&, WeatherLatch&, const WeatherEntry&) {}

void WeatherHooks::RunStep(WeatherSimulation&, const WeatherLatch&, Dia::ParallelExecutor::IParallelExecutor&) {}

void WeatherHooks::PublishCommit(const WeatherSimulation&, uint32_t) {}

WeatherStage::WeatherStage()
    : Detail::HooksHolder()
    , WeatherStageBase(hooks, World::kWeather, kLogChannel, &CauseTextRaw)
{
}

} // namespace Weather
} // namespace CoW
