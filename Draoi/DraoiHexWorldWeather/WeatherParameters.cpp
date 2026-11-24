#include <DraoiHexWorldWeather/WeatherParameters.h>

#include <DraoiHexWorld/BiomeRegistry.h>

#include <type_traits>

namespace Draoi {
namespace HexWorld {
namespace Weather {

static_assert(kMaxBaselineBiomes == BiomeRegistry::kMaxBiomes, "baseline columns must hold every registry biome");
static_assert(std::is_standard_layout_v<WeatherParameters>, "ParamBinding requires a standard-layout struct");

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
