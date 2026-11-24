// A5 -> A4 seam (A4 WeatherModule spec, "Header placement", AC 40). Standalone on purpose: HydrologyModule implements it
// without including the weather module. Includes only the Draoi weather types.
#pragma once

#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace CoW {
namespace Weather {

// Implemented by A5's HydrologyModule. Called only inside the stage's Capture on SimPU, never from a worker.
// Fills `out` with copies of A5's last committed week (surfaceWetness, openWater, glacierEligible) plus its
// glacierCheckWeek and glacierSnowFloor (HydrologyInputs carries them), and sets out.present. Returns false (and leaves
// `out` untouched) when A5 has nothing to give yet.
class IHydrologyInputsProvider
{
public:
    virtual ~IHydrologyInputsProvider() {}
    virtual bool GetInputs(Draoi::HexWorld::Weather::HydrologyInputs& out) = 0;
};

} // namespace Weather
} // namespace CoW
