// A4 weather-save: the ten saved field arrays. The table is the single list; ranges come from the
// `cow.weather.layers/<field>` sidecar entries, never from code. The array writer, Validate and the
// header are the Draoi kit's (HexWorldSaveCodec.h) and are called, not copied.
#pragma once

#include <stdint.h>

#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/HexWorldSaveCodec.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>
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

// The weather record spec the kit validates against: the ten resolved fields, Draoi caps (weather saves no
// basins, so maxBasins is 0; the cell cap is the kit's kMaxSaveCells) and hasBiome true. `fields` must outlive
// the returned spec.
WorldStep::RecordSpec WeatherSaveRecordSpec(const Draoi::HexWorld::SavedField (&fields)[kWeatherSaveFieldCount]);

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

// A decoded weather record (what DecodeRecord produces; Task 8 hands it to WeatherSimulation::RestoreCommitted).
struct WeatherRecord
{
    bool                                                    ready = false;   // false for a ready:false record
    uint64_t                                                week = 0;        // WeekOf(step_day)
    uint64_t                                                paramsHash = 0;
    uint32_t                                                width = 0;
    uint32_t                                                height = 0;
    Draoi::HexWorld::Weather::WeatherFields                 fields;
    Dia::Core::Containers::DynamicArray<Draoi::HexWorld::BiomeIndex> biomes;   // remapped into the supplied registry
};

// Writes the whole weather record into `out`: ready:false only for an uninitialised sim; otherwise the kit header
// (step_day = committed week * kDaysPerWeek, basin_count 0, world_hash 0), the ten arrays of the committed fields
// and the biome palette + cells (sim.Terrain().biome under `registry`). Writes no influences/climate. Returns false
// with `out` untouched when step_day would not fit the int32 the kit writes.
bool EncodeRecord(const Draoi::HexWorld::Weather::WeatherSimulation& sim, uint64_t paramsHash,
                  const Draoi::HexWorld::BiomeRegistry& registry, Json::Value& out);

// Validates `root` against `spec` (kit Validate), then reads the fields and remaps the biome palette into
// `registry` (unknown id -> fallback biome with one kit Warning per id; hash mismatch with a clean remap -> one kit
// Info). A ready:false record succeeds with out.ready false. Incoming influences/climate blocks are ignored with
// one Info on the WTH channel. On failure returns false with `err` naming the field and `out` reset to empty.
bool DecodeRecord(const Json::Value& root, const WorldStep::RecordSpec& spec,
                  const Draoi::HexWorld::BiomeRegistry& registry, WeatherRecord& out,
                  Draoi::HexWorld::SaveError& err);

constexpr uint32_t kWeatherStateVersion = 1;

// The `cow.weather_state` export file: {"format":"cow.weather_state","version":1} plus the committed record's fields
// (ready always true; no influences/climate). Returns false with `out` untouched for an uninitialised sim or a
// week that overflows step_day.
bool EncodeState(const Draoi::HexWorld::Weather::WeatherSimulation& sim, uint64_t paramsHash,
                 const Draoi::HexWorld::BiomeRegistry& registry, Json::Value& out);

// Parses a state file: rejects (err.field names the key) a non-object, wrong or missing `format`, missing or
// non-1 `version`, `ready` false, and any record defect (DecodeRecord). `out` is assigned only on success.
// Dimension match against the live world is the caller's (Feature 5).
bool DecodeState(const Json::Value& root, const WorldStep::RecordSpec& spec,
                 const Draoi::HexWorld::BiomeRegistry& registry, WeatherRecord& out,
                 Draoi::HexWorld::SaveError& err);

} // namespace Weather
} // namespace CoW
