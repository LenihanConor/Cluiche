#include <Shared/Weather/WeatherSaveFields.h>

#include <DiaObservation/Log/DiaLog.h>
#include <Shared/CoWCalendar.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace Weather {

namespace
{
    constexpr const char* kLogChannel = "WTH";
    using Draoi::HexWorld::Weather::WeatherTerrain;
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

WorldStep::RecordSpec WeatherSaveRecordSpec(const Draoi::HexWorld::SavedField (&fields)[kWeatherSaveFieldCount])
{
    WorldStep::RecordSpec spec;
    spec.fields = fields;
    spec.fieldCount = kWeatherSaveFieldCount;
    spec.caps.maxBasins = 0;
    spec.hasBiome = true;
    spec.pendingBiome = false;   // biome is not step-buffered: the pending blob holds the ten arrays only
    return spec;
}

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

bool EncodeRecord(const Draoi::HexWorld::Weather::WeatherSimulation& sim, uint64_t paramsHash,
                  const Draoi::HexWorld::BiomeRegistry& registry, Json::Value& out)
{
    if (!sim.IsInitialised())
    {
        Draoi::HexWorld::WriteNotReady(out);
        return true;
    }
    const uint64_t stepDay = sim.GetCommittedWeek() * kDaysPerWeek;
    if (sim.GetCommittedWeek() > static_cast<uint64_t>(INT32_MAX) / kDaysPerWeek)
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: weather save refused, week %llu overflows step_day",
                      static_cast<unsigned long long>(sim.GetCommittedWeek()));
        return false;
    }
    const WeatherTerrain& terrain = sim.Terrain();
    Draoi::HexWorld::SaveHeader header;
    header.width      = terrain.width;
    header.height     = terrain.height;
    header.cellCount  = sim.Fields().CellCount();
    header.stepDay    = static_cast<int32_t>(stepDay);
    header.paramsHash = paramsHash;
    header.basinCount = 0;
    header.worldHash  = 0;
    Draoi::HexWorld::WriteHeader(out, header);
    WriteWeatherFieldArrays(out, sim.Fields());
    Draoi::HexWorld::WriteBiomeCells(out, registry, terrain.biome.Size() == 0u ? nullptr : &terrain.biome[0u],
                                     terrain.biome.Size());
    return true;
}

bool DecodeRecord(const Json::Value& root, const WorldStep::RecordSpec& spec,
                  const Draoi::HexWorld::BiomeRegistry& registry, WeatherRecord& out,
                  Draoi::HexWorld::SaveError& err)
{
    out = WeatherRecord();
    if (!Draoi::HexWorld::Validate(root, spec.fields, spec.fieldCount, spec.caps, err))
    {
        return false;
    }
    if (!root["ready"].asBool())
    {
        return true;
    }

    WeatherRecord rec;
    const uint32_t cellCount = root["cell_count"].asUInt();
    rec.ready      = true;
    rec.width      = root["width"].asUInt();
    rec.height     = root["height"].asUInt();
    rec.paramsHash = root["params_hash"].asUInt64();
    rec.week       = CoW::WeekOf(root["step_day"].asUInt64());

    if (!ReadWeatherFieldArrays(root, cellCount, rec.fields))
    {
        err.field.SetText("fields");
        err.reason.SetText("array could not be read");
        return false;
    }
    std::vector<Draoi::HexWorld::BiomeIndex> cells(cellCount);
    if (!Draoi::HexWorld::ReadBiomeCells(root, registry, cells.data(), cellCount, err))
    {
        return false;
    }
    rec.biomes.Reserve(cellCount);
    for (uint32_t i = 0; i < cellCount; ++i) { rec.biomes.Add(cells[i]); }

    if (root.isMember("influences") || root.isMember("climate"))
    {
        DIA_LOG_INFO(kLogChannel, "WTH: weather save has influences/climate blocks; ignored (not built)");
    }

    out.ready      = rec.ready;
    out.week       = rec.week;
    out.paramsHash = rec.paramsHash;
    out.width      = rec.width;
    out.height     = rec.height;
    out.fields.Resize(cellCount);
    float* dst[kWeatherSaveFieldCount];
    const float* src[kWeatherSaveFieldCount];
    WeatherFieldPointers(out.fields, dst);
    WeatherFieldPointers(rec.fields, src);
    for (uint32_t f = 0; f < kWeatherSaveFieldCount; ++f)
    {
        for (uint32_t c = 0; c < cellCount; ++c) { dst[f][c] = src[f][c]; }
    }
    out.biomes.Reserve(cellCount);
    for (uint32_t i = 0; i < cellCount; ++i) { out.biomes.Add(rec.biomes[i]); }
    return true;
}

bool EncodeState(const Draoi::HexWorld::Weather::WeatherSimulation& sim, uint64_t paramsHash,
                 const Draoi::HexWorld::BiomeRegistry& registry, Json::Value& out)
{
    if (!sim.IsInitialised())
    {
        DIA_LOG_ERROR(kLogChannel, "WTH: weather state export refused, simulation is not initialised");
        return false;
    }
    Json::Value body(Json::objectValue);
    if (!EncodeRecord(sim, paramsHash, registry, body)) { return false; }
    body["format"]  = World::StateFormat(World::kWeather).c_str();
    body["version"] = kWeatherStateVersion;
    out = body;
    return true;
}

bool DecodeState(const Json::Value& root, const WorldStep::RecordSpec& spec,
                 const Draoi::HexWorld::BiomeRegistry& registry, WeatherRecord& out,
                 Draoi::HexWorld::SaveError& err)
{
    if (!root.isObject())
    {
        err.field.SetText("format");
        err.reason.SetText("state file is not an object");
        return false;
    }
    const Json::Value& fmt = root["format"];
    if (!fmt.isString() || fmt.asString() != World::StateFormat(World::kWeather).c_str())
    {
        err.field.SetText("format");
        err.reason.SetText("expected format cow.weather_state");
        return false;
    }
    const Json::Value& ver = root["version"];
    if (!ver.isIntegral() || ver.asInt64() != static_cast<int64_t>(kWeatherStateVersion))
    {
        err.field.SetText("version");
        err.reason.SetText("missing or unsupported state file version");
        return false;
    }
    WeatherRecord rec;
    if (!DecodeRecord(root, spec, registry, rec, err)) { return false; }
    if (!rec.ready)
    {
        err.field.SetText("ready");
        err.reason.SetText("a state file must hold a ready weather record");
        return false;
    }
    // WeatherFields/DynamicArray are copied element-wise (as DecodeRecord does); a plain assignment is not relied on.
    const uint32_t cellCount = rec.fields.CellCount();
    out = WeatherRecord();
    out.ready      = rec.ready;
    out.week       = rec.week;
    out.paramsHash = rec.paramsHash;
    out.width      = rec.width;
    out.height     = rec.height;
    out.fields.Resize(cellCount);
    float* dst[kWeatherSaveFieldCount];
    const float* src[kWeatherSaveFieldCount];
    WeatherFieldPointers(out.fields, dst);
    WeatherFieldPointers(rec.fields, src);
    for (uint32_t f = 0; f < kWeatherSaveFieldCount; ++f)
    {
        for (uint32_t c = 0; c < cellCount; ++c) { dst[f][c] = src[f][c]; }
    }
    out.biomes.Reserve(cellCount);
    for (uint32_t i = 0; i < cellCount; ++i) { out.biomes.Add(rec.biomes[i]); }
    return true;
}

} // namespace Weather
} // namespace CoW
