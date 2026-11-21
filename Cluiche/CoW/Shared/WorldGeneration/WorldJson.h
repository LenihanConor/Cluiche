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

namespace CoW { namespace World { class BiomeRegistry; } }

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
    // 1-based position of a JSON syntax error in the text; 0 when the problem is structural (no text position).
    uint32_t    line = 0;
    uint32_t    column = 0;
};

struct WorldJsonContext
{
    // Export: names for registered site kinds. Import: used to warn about kinds this build does not register.
    // nullptr = only the four built-in node kinds are known.
    const SiteRegistry* registry = nullptr;
    // Required by export and import (both fail without it): biome ids, provenance.biome_registry_hash, and the
    // registry the region summaries are recomputed against.
    const World::BiomeRegistry* biomes = nullptr;
    // Export: written as provenance.preset_id when non-empty, else the world's own provenance.presetId.
    std::string presetId;
    // Import: this build's terrain rules version (TerrainRules::rulesVersion); a file recorded with another one gets a warning. 0 = do not compare.
    uint32_t rulesVersion = 0;
};

// Biome ids are written with BiomeRegistry::IdText and read with ResolveOrFallback (an unknown id imports as the
// fallback biome). The other enums use the names below (parse is the exact inverse of the name functions).
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

struct WorldImportResult
{
    bool                     ok = false;
    JsonIssue                error;      // the first problem when !ok
    std::vector<std::string> warnings;   // import succeeded but the file was unusual (at most kMaxImportWarnings + 1)
};

constexpr size_t kMaxImportWarnings = 64;

// Parses and validates `text` into a scratch world; `out` is replaced only when the result is ok (all-or-nothing).
// Derived region fields (cell_count, primary_biome, purity, centre_cell, neighbours) are recomputed from the cells with
// the generator's own arithmetic; where the file disagrees a warning is added and the recomputed value wins.
// Site kinds are kept whether or not they are registered; the original kind text is remembered in out.sites so a
// re-export writes it back unchanged.
WorldImportResult ImportWorldText(const std::string& text, const WorldJsonContext& context, WorldStructure& out);

} // namespace WorldGeneration
} // namespace CoW
