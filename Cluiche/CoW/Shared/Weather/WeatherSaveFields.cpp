#include <Shared/Weather/WeatherSaveFields.h>

namespace CoW {
namespace Weather {

namespace
{
    using Draoi::HexWorld::SavedKind;
    using Draoi::HexWorld::SavedScope;
    using Draoi::HexWorld::Weather::WeatherFields;

    const WorldStep::FieldDecl kDecls[kWeatherSaveFieldCount] = {
        { "moisture",         SavedKind::F32, SavedScope::PerCell },
        { "temperature",      SavedKind::F32, SavedScope::PerCell },
        { "pressure",         SavedKind::F32, SavedScope::PerCell },
        { "wind_direction",   SavedKind::F32, SavedScope::PerCell },
        { "wind_speed",       SavedKind::F32, SavedScope::PerCell },
        { "precipitation",    SavedKind::F32, SavedScope::PerCell },
        { "cloud_cover",      SavedKind::F32, SavedScope::PerCell },
        { "water_to_ground",  SavedKind::F32, SavedScope::PerCell },
        { "snow_pack",        SavedKind::F32, SavedScope::PerCell },
        { "pressure_anomaly", SavedKind::F32, SavedScope::PerCell },
    };

    template <class TFields, class TPtr>
    void Fill(TFields& f, TPtr (&out)[kWeatherSaveFieldCount])
    {
        auto p = [](auto& a) { return a.Size() == 0u ? nullptr : &a[0u]; };
        out[0] = p(f.moisture);
        out[1] = p(f.temperature);
        out[2] = p(f.pressure);
        out[3] = p(f.windDirection);
        out[4] = p(f.windSpeed);
        out[5] = p(f.precipitation);
        out[6] = p(f.cloudCover);
        out[7] = p(f.waterToGround);
        out[8] = p(f.snowPack);
        out[9] = p(f.pressureAnomaly);
    }
}

const WorldStep::FieldDecl* WeatherSaveFieldDecls() { return kDecls; }

void WeatherFieldPointers(const WeatherFields& fields, const float* (&out)[kWeatherSaveFieldCount]) { Fill(fields, out); }
void WeatherFieldPointers(WeatherFields& fields, float* (&out)[kWeatherSaveFieldCount]) { Fill(fields, out); }

void WriteWeatherFieldArrays(Json::Value& root, const WeatherFields& fields)
{
    const float* ptrs[kWeatherSaveFieldCount];
    WeatherFieldPointers(fields, ptrs);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        Draoi::HexWorld::WriteArray(root, kDecls[i].name, ptrs[i], fields.CellCount());
    }
}

bool ReadWeatherFieldArrays(const Json::Value& root, uint32_t cellCount, WeatherFields& fields)
{
    fields.Resize(cellCount);
    float* ptrs[kWeatherSaveFieldCount];
    WeatherFieldPointers(fields, ptrs);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const Json::Value& arr = root[kDecls[i].name];
        if (!arr.isArray() || arr.size() != cellCount) { return false; }
        for (uint32_t c = 0; c < cellCount; ++c)
        {
            if (!arr[c].isNumeric()) { return false; }
            ptrs[i][c] = static_cast<float>(arr[c].asDouble());
        }
    }
    return true;
}

} // namespace Weather
} // namespace CoW
