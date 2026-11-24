// A4 weather-save: the ten saved field arrays. The table is the single list; ranges come from the
// `cow.weather.layers/<field>` sidecar entries, never from code. The array writer, Validate and the
// header are the Draoi kit's (HexWorldSaveCodec.h) and are called, not copied.
#pragma once

#include <stdint.h>

#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/HexWorldSaveCodec.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>
#include <Shared/World/WorldSimNames.h>
#include <Shared/WorldStep/WorldSimSaveCodec.h>

namespace CoW {
namespace Weather {

constexpr uint32_t kWeatherSaveFieldCount = 10;

// The ten arrays in saved order: moisture, temperature, pressure, wind_direction, wind_speed, precipitation,
// cloud_cover, water_to_ground, snow_pack, pressure_anomaly. Index i here matches index i of
// ResolveWeatherSaveFields output and of the pointer tables below.
const WorldStep::FieldDecl* WeatherSaveFieldDecls();

// Fills out[0..10) from the sidecar schema (ValueSchemaRegistry or ValueSchemaService). false + err naming the
// first field with no layer or state entry (or no min/max).
template <class TSchema>
bool ResolveWeatherSaveFields(const TSchema& schema, Draoi::HexWorld::SavedField (&out)[kWeatherSaveFieldCount],
                              Draoi::HexWorld::SaveError& err)
{
    return WorldStep::ResolveSavedFields(schema, World::kWeather, WeatherSaveFieldDecls(), kWeatherSaveFieldCount,
                                         out, err);
}

// Pointers to the ten arrays in saved order (an empty array yields nullptr).
void WeatherFieldPointers(const Draoi::HexWorld::Weather::WeatherFields& fields, const float* (&out)[kWeatherSaveFieldCount]);
void WeatherFieldPointers(Draoi::HexWorld::Weather::WeatherFields& fields, float* (&out)[kWeatherSaveFieldCount]);

// root[name] = array for each of the ten fields, through Draoi WriteArray (-0.0 is written as +0.0).
// Writes fields.CellCount() entries per array.
void WriteWeatherFieldArrays(Json::Value& root, const Draoi::HexWorld::Weather::WeatherFields& fields);

// Resizes `fields` to cellCount and reads the ten arrays from `root`. Call only on a record that passed
// Draoi Validate (lengths, finiteness and ranges are not re-checked); false when an array is missing or has
// the wrong length or an element is not a number, leaving `fields` unspecified.
bool ReadWeatherFieldArrays(const Json::Value& root, uint32_t cellCount, Draoi::HexWorld::Weather::WeatherFields& fields);

} // namespace Weather
} // namespace CoW
