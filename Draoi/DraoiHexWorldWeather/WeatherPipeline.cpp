// A4 weather pipeline: cellular-automaton passes (solar, pressure, wind, evaporation, transport, etc).
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
#include <DraoiHexWorldWeather/WeatherPipeline.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

void RunPipeline(PassContext&, Dia::ParallelExecutor::IParallelExecutor&)
{
    // Passes are added by later tasks, in the order listed in the spec.
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
