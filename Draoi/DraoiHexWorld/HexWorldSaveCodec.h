// Save codec, data half: serialization and validation of hex-world state records.
// No game content and no save-system wiring; callers declare fields and supply ranges and caps.
#pragma once

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String256.h>

#include <cstdint>
#include <string>

namespace Draoi {
namespace HexWorld {

enum class SavedKind  : uint8_t { F32, U8, U16 };
enum class SavedScope : uint8_t { PerCell, PerBasin };

// One saved array. The array key is `name`; [min, max] is the caller-supplied inclusive range (no sidecar is read).
// Angular fields declare [0, 2pi]; the caller normalises.
struct SavedField
{
    const char* name;
    SavedKind   kind;
    SavedScope  scope;
    double      min;
    double      max;
};

struct SaveHeader
{
    uint32_t width       = 0;
    uint32_t height      = 0;
    uint32_t cellCount   = 0;
    int32_t  stepDay     = 0;
    uint64_t paramsHash  = 0;   // supplied by the caller
    uint32_t basinCount  = 0;
    uint64_t worldHash   = 0;
};

// Count caps declared by the system.
struct Caps
{
    uint32_t maxBasins = 0;
};

struct SaveError
{
    Dia::Core::Containers::String32  field;
    Dia::Core::Containers::String256 reason;
};

constexpr uint32_t kMaxSaveCells = 65536;

// Writes ready:true, width, height, cell_count, step_day, params_hash, basin_count, world_hash.
void WriteHeader(Json::Value& root, const SaveHeader& header);
// Writes ready:false only.
void WriteNotReady(Json::Value& root);
// Writes root[name] as an array of doubles; -0.0 is written as +0.0.
void WriteArray(Json::Value& root, const char* name, const float* values, uint32_t count);
// Compact text with 17 significant digits for doubles.
std::string Serialize(const Json::Value& root);

// All-or-nothing; on failure err names the first failing field. A record with only ready:false is accepted.
bool Validate(const Json::Value& root, const SavedField* fields, uint32_t fieldCount, const Caps& caps, SaveError& err);
// Same checks over a pending-step blob (the pending format itself belongs to the game).
bool ValidatePending(const Json::Value& blob, const SavedField* fields, uint32_t fieldCount, const Caps& caps, SaveError& err);

} // namespace HexWorld
} // namespace Draoi
