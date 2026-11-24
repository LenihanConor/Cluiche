#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

namespace
{
    template <class T>
    void Zero(Dia::Core::Containers::DynamicArray<T>& a, uint32_t n)
    {
        a.RemoveAll();
        a.Reserve(n);
        for (uint32_t i = 0; i < n; ++i) a.Add(T());
    }
}

void WeatherFields::Resize(uint32_t n)
{
    Zero(moisture, n);    Zero(temperature, n);   Zero(pressure, n);   Zero(windDirection, n);
    Zero(windSpeed, n);   Zero(precipitation, n); Zero(cloudCover, n); Zero(waterToGround, n);
    Zero(snowPack, n);    Zero(pressureAnomaly, n);
}

void WeatherHandoff::Resize(uint32_t n)
{
    Zero(glacierInput, n);
    Zero(influenceWater, n);
}

void HydrologyInputs::Resize(uint32_t n)
{
    Zero(surfaceWetness, n);
    Zero(openWater, n);
    Zero(glacierEligible, n);
}

bool WeatherTerrain::IsConsistent() const
{
    const uint64_t n = static_cast<uint64_t>(width) * height;
    return n != 0 && elevation.Size() == n && latitude.Size() == n && waterSource.Size() == n && biome.Size() == n;
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
