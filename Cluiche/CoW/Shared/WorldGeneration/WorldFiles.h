// S-1 World Generation: world file names, locations and safe reading/writing of `<name>.wld.json`.
// Pure logic over std::filesystem. A world is always addressed by a NAME, never a path: the name is validated
// ([A-Za-z0-9_-]{1,64}, no separators, dots or device names), the extension is added here, and the directory comes
// from configuration, so a command argument can never reach outside the worlds directories.
//
// Writable worlds live in the out worlds directory (PD-009: Cluiche/out/CoW/worlds); an optional read-only
// deployed directory is searched second on read.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC7, Default file paths)
#pragma once

#include <Shared/WorldGeneration/WorldJson.h>

#include <cstdint>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr size_t kMaxWorldNameLength = 64;

enum class WorldFileStatus : uint8_t
{
    Ok,
    InvalidName,     // not [A-Za-z0-9_-]{1,64}, or a reserved device name
    NoDirectory,     // no directory configured
    NotFound,        // read: no such world in any configured directory
    AlreadyExists,   // write without overwrite
    TooLarge,        // larger than kMaxWorldJsonBytes (checked before reading, and before writing)
    IoError,
};

const char* WorldFileStatusName(WorldFileStatus status);

struct WorldFileResult
{
    WorldFileStatus status = WorldFileStatus::Ok;
    std::string     message;       // human readable reason when status != Ok
    std::string     path;          // file read or written
    size_t          bytes = 0;     // size of the text written or read

    bool Ok() const { return status == WorldFileStatus::Ok; }
};

// [A-Za-z0-9_-]{1,64}, and not a Windows device name (CON, PRN, AUX, NUL, COM1-9, LPT1-9, any case).
bool IsValidWorldName(const std::string& name);

// "<directory>/<name>.wld.json"
std::string WorldFilePath(const std::string& directory, const std::string& name);

// The default export name: the seed in decimal ("12345").
std::string DefaultWorldName(uint64_t seed);

// Writes `<directory>/<name>.wld.json`, creating the directory. Atomic: the text goes to a temporary file in the
// same directory which is then renamed over the target, so a failed write never leaves a half file or damages an
// existing one. Refuses an existing file unless overwrite.
WorldFileResult WriteWorldFile(const std::string& directory, const std::string& name, const std::string& text, bool overwrite);

// The valid world names (stems of `*.wld.json`) in one directory, sorted. Empty for an empty/missing directory.
std::vector<std::string> ListWorldNames(const std::string& directory);

// Reads `<name>.wld.json` from outDirectory, else from deployedDirectory (either may be empty = not configured).
WorldFileResult ReadWorldFile(const std::string& outDirectory, const std::string& deployedDirectory, const std::string& name,
                              std::string& text);

} // namespace WorldGeneration
} // namespace CoW
