// WorldShapePreset - load, save and list world-shape presets (versioned JSON assets).
// Pure logic (std::filesystem + jsoncpp, no Dia module state).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md (Presets, AC19)
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaCore/Json/external/json/json.h>

#include <cstdint>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr uint32_t kWorldShapePresetVersion = 1;
constexpr const char* kWorldShapePresetFormat = "cow.worldshape";
constexpr const char* kWorldShapePresetExtension = ".worldshape.json";
constexpr size_t kMaxPresetIdLength = 32;

enum class PresetStatus : uint8_t
{
    Ok,
    FileNotFound,
    ParseError,
    InvalidFormat,        // not an object, wrong "format", missing/invalid "version" or "params"
    UnsupportedVersion,   // version greater than kWorldShapePresetVersion (or below 1)
    InvalidId,            // id does not match [a-z0-9_]{1,32}
    IdMismatch,           // id does not match the file name / requested id
    UnknownBarrierForm,
    AlreadyExists,        // SavePreset without overwrite
    IoError,
};

inline const char* PresetStatusName(PresetStatus s)
{
    switch (s)
    {
    case PresetStatus::Ok:                 return "Ok";
    case PresetStatus::FileNotFound:       return "FileNotFound";
    case PresetStatus::ParseError:         return "ParseError";
    case PresetStatus::InvalidFormat:      return "InvalidFormat";
    case PresetStatus::UnsupportedVersion: return "UnsupportedVersion";
    case PresetStatus::InvalidId:          return "InvalidId";
    case PresetStatus::IdMismatch:         return "IdMismatch";
    case PresetStatus::UnknownBarrierForm: return "UnknownBarrierForm";
    case PresetStatus::AlreadyExists:      return "AlreadyExists";
    case PresetStatus::IoError:            return "IoError";
    }
    return "Unknown";
}

struct WorldShapePreset
{
    std::string      id;
    std::string      displayName;
    std::string      description;
    uint32_t         version = kWorldShapePresetVersion;
    WorldShapeParams params;
};

struct PresetResult
{
    PresetStatus status = PresetStatus::Ok;
    std::string  message;   // human-readable reason when status != Ok
    std::string  path;      // file read or written, when applicable

    bool Ok() const { return status == PresetStatus::Ok; }
};

struct PresetInfo
{
    std::string id;
    std::string displayName;
    std::string description;
    uint32_t    version = 0;
    std::string sourcePath;
};

// True when id matches [a-z0-9_]{1,32}.
bool IsValidPresetId(const std::string& id);

// <directory>/<id>.worldshape.json
std::string PresetFilePath(const std::string& directory, const std::string& id);

// Parses a preset document. expectedId is the file-name id; pass "" to skip the match check.
// Warnings ("missing:<key>", "unknown:<key>", "range:<key>", "invalid:<key>") are appended:
// a missing params key takes its default, an unknown key is ignored, an out-of-range value is clamped.
// On failure `out` is left default-constructed.
PresetResult ParsePreset(const Json::Value& root, const std::string& expectedId, WorldShapePreset& out, RuleWarnings& warnings);

// Params are written clamped; floats use the shortest text that reads back to the same float.
Json::Value PresetToJson(const WorldShapePreset& preset);

PresetResult LoadPreset(const std::string& directory, const std::string& id, WorldShapePreset& out, RuleWarnings& warnings);

// Writes <directory>/<id>.worldshape.json, creating the directory. Refuses an existing file unless overwrite.
PresetResult SavePreset(const std::string& directory, const WorldShapePreset& preset, bool overwrite);

// Every valid *.worldshape.json in the directory, sorted by id. Invalid files are skipped.
std::vector<PresetInfo> ListPresets(const std::string& directory);

} // namespace WorldGeneration
} // namespace CoW
