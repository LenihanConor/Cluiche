// S-1 World Generation: JSON export and import of a WorldStructure (schema "cow.world" v1).
// Pure logic (no files, no Dia module state): text in, text out. File handling is WorldFiles.h, the session
// lock is WorldGenSession.
//
// Export writes deterministic, human-readable text (fixed key order, one cell per line, floats as %.9g which
// reads back to the exact same float). Import is all-or-nothing: it parses and validates into a scratch world and
// touches the caller's world only on success; errors name the JSON path of the first problem.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md
#pragma once

#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr const char* kWorldJsonFormat   = "cow.world";
constexpr uint32_t    kWorldJsonVersion  = 1;
constexpr const char* kWorldJsonTopology = "hex_flat_odd_q";
constexpr const char* kWorldFileExtension = ".wld.json";

// Largest text accepted by import (checked before parsing) and the grid limits import enforces.
constexpr size_t kMaxWorldJsonBytes = 16u * 1024u * 1024u;
constexpr int    kMaxWorldJsonDimension = 256;
constexpr int    kMaxWorldJsonCells = 65536;

// One problem: the JSON path ("world.cells[17].elevation", "version", ...) and a message.
struct JsonIssue
{
    std::string field;
    std::string message;
};

struct WorldJsonContext
{
    // Export: names for registered site kinds. Import: used to warn about kinds this build does not register.
    // nullptr = only the four built-in node kinds are known.
    const SiteRegistry* registry = nullptr;
    // Export: written as provenance.preset_id when non-empty, else the world's own provenance.presetId.
    std::string presetId;
};

// "Biome.TemperateForest", "Lowland", ... (the names the file uses; parse is the exact inverse).
const char* WorldJsonBiomeName(Biome b);
bool        WorldJsonParseBiome(const std::string& text, Biome& out);
bool        WorldJsonParseTerrain(const std::string& text, TerrainType& out);
bool        WorldJsonParseWaterSource(const std::string& text, WaterSource& out);
bool        WorldJsonParseDepthZone(const std::string& text, DepthZone& out);

// The key a site kind is written under: a built-in node kind, a registered kind, a name remembered by the
// world's SiteSet (an unknown kind that came from an import), else "crc:xxxxxxxx" (lower-case hex), which
// import turns back into exactly that kind id.
std::string WorldJsonSiteKindKey(uint32_t kindCrc, const SiteRegistry* registry, const SiteSet& sites);

// Writes the world. False (text cleared, error set) for a world that cannot be written faithfully: a
// non-finite elevation/latitude/purity, an enum out of range, a site outside the grid.
bool ExportWorldText(const WorldStructure& world, const WorldJsonContext& context, std::string& text, JsonIssue& error);

} // namespace WorldGeneration
} // namespace CoW
