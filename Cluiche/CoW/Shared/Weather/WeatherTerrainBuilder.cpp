#include <Shared/Weather/WeatherTerrainBuilder.h>

namespace CoW {
namespace Weather {

bool BuildWeatherTerrain(const Draoi::HexWorld::WorldStructure& world, Draoi::HexWorld::Weather::WeatherTerrain& out)
{
    out = Draoi::HexWorld::Weather::WeatherTerrain();
    const int count = world.CellCount();
    if (count <= 0 || world.dimensions.width <= 0 || world.dimensions.height <= 0
        || count != world.dimensions.width * world.dimensions.height)
    {
        return false;
    }
    const uint32_t n = static_cast<uint32_t>(count);
    out.width = static_cast<uint32_t>(world.dimensions.width);
    out.height = static_cast<uint32_t>(world.dimensions.height);
    out.elevation.Reserve(n);
    out.latitude.Reserve(n);
    out.waterSource.Reserve(n);
    out.biome.Reserve(n);
    const Draoi::HexWorld::Cell* cells = world.Cells();
    for (uint32_t i = 0; i < n; ++i)
    {
        out.elevation.Add(cells[i].elevation);
        out.latitude.Add(cells[i].latitude);
        out.waterSource.Add(cells[i].waterSource);
        out.biome.Add(cells[i].biome);
    }
    return true;
}

} // namespace Weather
} // namespace CoW
