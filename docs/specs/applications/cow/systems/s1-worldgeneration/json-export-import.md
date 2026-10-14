# Feature Spec: JSON Export/Import

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

JSON Export/Import enables designers to freeze a procedurally-generated world into an editable text file, hand-edit problematic placements in any text editor, and re-import the result as an exact authored world (not a re-roll). The export format is human-readable JSON capturing full world structure (grid with biomes/terrain/regions per cell, all site arrays), and the round-trip guarantee ensures `Import(Export(world)) == world` byte-for-byte.

This feature resolves GDD decision 4's "you can't hand-edit a seed" limitation — export captures placement outcomes, not generation parameters.

## Problem

Without export/import:
- **Unlucky placement locked in** — If retry mechanism succeeds but produces a world with one badly-placed Hearth Site (e.g., too close to a rival), designers cannot fix it without regenerating from scratch (loses the rest of the good placement)
- **Binary-only iteration** — Without human-readable format, designers cannot inspect world structure outside the game or hand-author test worlds for downstream testbeds (S-2/S-3/S-4)
- **No authored baselines** — Cannot create canonical test worlds (e.g., "symmetric world for balance testing", "minimal world for regression testing") that survive code changes to Feature 1's algorithm

With export/import:
- **Surgical edits** — Generate world → export → tweak one Hearth position in JSON → re-import, keeping the rest unchanged
- **Authored test cases** — Hand-write or export worlds for S-2/S-3 regression tests (e.g., "ecology baseline", "pathfinding stress test")
- **Version control friendly** — JSON diffs show what changed between authored worlds, unlike binary formats

## Goals

1. **Human-readable JSON format** — Designers can open `.wld.json` in any text editor, read biome names / site positions, and edit values without custom tooling
2. **Full structure capture** — Export includes all fields of `WorldStructure` (dimensions, cells array with biome/terrain/region per cell, all site arrays with positions/types)
3. **Round-trip guarantee** — `Import(Export(world)) == world` byte-for-byte identical (not "similar", not "regenerated from same seed")
4. **Validation on import** — Import fails gracefully if JSON is malformed, missing required fields, or violates structural invariants (e.g., cell count != dimensions.width * dimensions.height)
5. **Versioned schema** — JSON includes schema version tag so future format changes can migrate old files (same pattern as E1 save/load versioning)
6. **Fast export/import** — Completes within seconds for typical worlds (Large 104×64 = 6,656 cells); does not block iteration workflow

## Non-Goals

1. **Binary format** — JSON only (human-readable trumps file size / performance for S-1's authoring use case); if S-2/S-3 need faster loading, add binary format later
2. **Compression** — Uncompressed JSON (compression makes hand-editing harder); if file sizes become unwieldy, add gzip wrapper later
3. **Partial export** — Full world only (cannot export "just Hearth Sites" or "just biomes"); if needed for large worlds, add later
4. **Schema migration tool** — Version tag present but migration code deferred until schema actually changes (S-1 ships with v1 only)
5. **Export of live session state** — This exports world structure only (Feature 1's output); E1 (save/load) exports live ecology/dragon/clan state (not this feature's job)

## Architecture

### JSON Schema (v1)

```json
{
  "schema_version": 1,
  "world": {
    "topology": "hex_pointy_odd_r",
    "dimensions": {
      "width": 80,
      "height": 52
    },
    "cells": [
      {
        "biome": "Forest",
        "terrain": "Foliage",
        "region": "VerdantLowlands",
        "elevation": 0.35,
        "latitude": 0.42,
        "water_source": "None"
      },
      // ... 4160 cells total (width * height); cell [y * width + x]
    ],
    "hearths": [
      {
        "position": { "x": 10, "y": 20 },
        "parent_region": "VerdantLowlands"
      },
      // ... N hearths
    ],
    "nexuses": [
      {
        "position": { "x": 50, "y": 60 }
      },
      // ... N nexuses
    ],
    "thrones": [
      {
        "position": { "x": 70, "y": 80 },
        "type": "FireApotheosis"
      },
      // ... exactly 6 thrones
    ],
    "dynasty_starts": [
      {
        "position": { "x": 30, "y": 40 },
        "role": "PLAYER"
      },
      {
        "position": { "x": 100, "y": 110 },
        "role": "RIVAL"
      },
      // ... N dynasty starts (1 PLAYER, N-1 RIVALs)
    ]
  }
}
```

**Rationale for schema choices:**
- **String biome/terrain/region names** (not integer IDs): Human-readable ("Forest" beats `BiomeID: 3`); maps to StringCRC at import time
- **Flat cells array** (not 2D nested arrays): Simpler JSON parse; cell `[y * width + x]` mapping explicit in import code
- **Separate site arrays** (not embedded in cells): Matches `WorldStructure` layout; easier to edit site positions without finding the cell
- **`schema_version` at top level**: Enables future migration (v2 might add new site types, rename fields, etc.)
- **`topology`** *(amended 2026-10-14)*: names the cell layout. Only `"hex_pointy_odd_r"` (Dia's rectangular `HexPathGrid` layout) is valid in v1; import fails on anything else, so a square-grid file can never be silently misread. No world file existed before this amendment, so the schema stays at v1.
- **`elevation`, `latitude`, `water_source`** *(amended 2026-10-14)*: A4 Weather's terrain inputs. `elevation` and `latitude` are floats in [0, 1], written with 9 significant digits (`float` round-trips exactly at that precision); `water_source` is `None` / `Ocean` / `Lake` / `River`. Import fails if a float is outside [0, 1] or `water_source` is unknown.

### Export Implementation

```cpp
Result<void> ExportWorld(const WorldStructure& world, const std::string& filePath) {
    Json::Value root;
    root["schema_version"] = 1;
    
    Json::Value worldJson;
    worldJson["topology"] = "hex_pointy_odd_r";
    worldJson["dimensions"]["width"] = world.dimensions.width;
    worldJson["dimensions"]["height"] = world.dimensions.height;
    
    // Serialize cells array
    for (const auto& cell : world.cells) {
        Json::Value cellJson;
        cellJson["biome"] = BiomeToString(cell.biome);        // StringCRC → string
        cellJson["terrain"] = TerrainToString(cell.terrain);
        cellJson["region"] = RegionToString(cell.region);
        cellJson["elevation"] = cell.elevation;           // written with 9 significant digits
        cellJson["latitude"] = cell.latitude;
        cellJson["water_source"] = WaterSourceToString(cell.waterSource);
        worldJson["cells"].append(cellJson);
    }
    
    // Serialize site arrays (hearths, nexuses, thrones, dynasty_starts)
    // ... similar pattern: append to JSON arrays
    
    root["world"] = worldJson;
    
    // Write to file
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return Result<void>::Failure("Cannot open file for writing: " + filePath);
    }
    file << root.toStyledString();  // jsoncpp pretty-print (human-readable)
    file.close();
    
    DIA_LOG_INFO("WLD", "Exported world to %s", filePath.c_str());
    return Result<void>::Success();
}
```

**String conversion helpers:**

```cpp
std::string BiomeToString(Biome biome) {
    // Map StringCRC back to string (requires biome name registry or reverse lookup table)
    // Placeholder: hardcoded switch/map for Phase 2 biome enum
}

Biome BiomeFromString(const std::string& str) {
    // Map string to StringCRC (compile-time or runtime hash)
    return Biome{Dia::Core::StringCRC::Hash(str.c_str())};
}
```

### Import Implementation

```cpp
Result<WorldStructure> ImportWorld(const std::string& filePath) {
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return Result<WorldStructure>::Failure("Cannot open file for reading: " + filePath);
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        return Result<WorldStructure>::Failure("JSON parse error: " + errors);
    }
    
    // Validate schema version
    int schemaVersion = root["schema_version"].asInt();
    if (schemaVersion != 1) {
        return Result<WorldStructure>::Failure("Unsupported schema version: " + std::to_string(schemaVersion));
    }
    
    // Deserialize world structure
    WorldStructure world;
    const Json::Value& worldJson = root["world"];
    
    world.dimensions.width = worldJson["dimensions"]["width"].asUInt();
    world.dimensions.height = worldJson["dimensions"]["height"].asUInt();
    
    // Validate cell count
    const Json::Value& cellsJson = worldJson["cells"];
    if (cellsJson.size() != world.dimensions.width * world.dimensions.height) {
        return Result<WorldStructure>::Failure("Cell count mismatch: expected " + std::to_string(world.dimensions.width * world.dimensions.height) + ", got " + std::to_string(cellsJson.size()));
    }
    
    // Deserialize cells
    for (const auto& cellJson : cellsJson) {
        Cell cell;
        cell.biome = BiomeFromString(cellJson["biome"].asString());
        cell.terrain = TerrainFromString(cellJson["terrain"].asString());
        cell.region = RegionFromString(cellJson["region"].asString());
        cell.elevation = cellJson["elevation"].asFloat();       // validated into [0, 1]
        cell.latitude = cellJson["latitude"].asFloat();         // validated into [0, 1]
        cell.waterSource = WaterSourceFromString(cellJson["water_source"].asString());
        world.cells.push_back(cell);
    }
    
    // Deserialize site arrays (hearths, nexuses, thrones, dynasty_starts)
    // ... similar pattern: loop over JSON arrays, construct C++ structs
    
    DIA_LOG_INFO("WLD", "Imported world from %s", filePath.c_str());
    return Result<WorldStructure>::Success(world);
}
```

**Validation on import:** Check `topology`, dimensions, cell count, per-cell `elevation`/`latitude` in [0, 1] and known `water_source`, every site position in bounds, throne count == 6, at least one PLAYER dynasty start. A world with no water cell imports with a warning, not an error (A4 refuses to run on it, and authored test worlds may be waterless). Fail gracefully if violations found (don't return half-constructed world).

### Round-Trip Guarantee

**Test implementation:** `world1` → Export → Import → `world2`, then compare every field of `world1` and `world2`:

```cpp
bool WorldStructure::operator==(const WorldStructure& other) const {
    if (dimensions != other.dimensions) return false;
    if (cells.size() != other.cells.size()) return false;
    for (size_t i = 0; i < cells.size(); ++i) {
        if (cells[i] != other.cells[i]) return false;  // Cell::operator== checks biome/terrain/region
    }
    // ... compare hearths, nexuses, thrones, dynasty_starts arrays
    return true;
}
```

**No tolerance for floating-point drift:** Grid positions are integers (no rounding issues). StringCRC comparisons are exact (no hash collisions for Phase 2's small enum set).

## Public Interfaces

### APIs

```cpp
// WorldGenerationModule.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

class WorldGenerationModule : public Dia::Core::IModule {
public:
    // Export world to JSON file at specified path
    // Overwrites existing file if present
    Dia::Core::Result<void> ExportWorld(const WorldStructure& world, const std::string& filePath);
    
    // Import world from JSON file; validates structure on load
    // Returns: success (with WorldStructure), or failure (malformed JSON, validation error)
    Dia::Core::Result<WorldStructure> ImportWorld(const std::string& filePath);
    
    // ... other Feature 3/5/6 APIs (GenerateWorld, GetProgress, Cancel)
};

} // namespace CoW::WorldGeneration
```

**Default file paths:** Export defaults to `Cluiche/out/CoW/worlds/<seed>.wld.json` (PD-009 compliance); Feature 6 (Debug Host Integration) can override path via UI.

### Events

```cpp
// Published on successful export (optional, for F-1 debug host feedback)
struct WorldExportComplete {
    std::string filePath;
};

// Published on successful import (optional, for F-1 debug host feedback).
// A small notification: the world is read via WorldGenerationModule::GetCurrentWorld() (amended 2026-10-14).
struct WorldImportComplete {
    std::string filePath;
    uint32_t worldRevision;
};
```

**Rationale:** Events enable F-1 debug host to display feedback ("Exported to C:/.../.wld.json"). Optional — can log instead if events not needed for S-1 MVP.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Produces `WorldStructure` to export | Draft (being specced) |
| **Phase 2 data design** | Biome/TerrainType/Region/ApotheosisType enum names (for string serialization) | GDD in-progress (names TBD — placeholder uses arbitrary names until Phase 2 finalizes) |

### Dia Engine Dependencies

- **DiaCore** (file I/O: `std::ifstream`/`std::ofstream`, containers: `DynamicArrayC`)
- **jsoncpp** (`Json::Value`, `Json::CharReaderBuilder`, `Json::StyledWriter`) — already in `External/jsoncpp-master`, used by Dia's JSON integration
- **PD-009 compliance:** Default export path under `Cluiche/out/CoW/`

## Acceptance Criteria

### AC1 — Export Produces Valid JSON
Generate world, export to file. Open file in text editor. **Pass:** Valid JSON (no syntax errors), human-readable biome/terrain/region/site names (not integer IDs or hex dumps), pretty-printed (indented, not minified one-liner).

### AC2 — Round-Trip Guarantee (Exact Match)
Generate world, export to file, import from file, compare imported world to original. **Pass:** `imported == original` byte-for-byte (every cell, every site, every field). Test on ≥5 random worlds.

### AC3 — Import Validation (Malformed JSON)
Hand-edit exported JSON to introduce errors: (a) missing "biome" field in cell, (b) cell count != dimensions, (c) throne count != 6, (d) invalid schema version. Attempt import. **Pass:** Import returns Failure with clear error message, does not crash or return half-constructed world.

### AC4 — Hand-Edit and Re-Import
Export world, hand-edit JSON to move one Hearth Site position (change x/y coordinates), re-import. **Pass:** Imported world reflects edited position (Hearth at new location), all other data unchanged. Proves hand-editing workflow.

### AC5 — Schema Version Enforcement
Export world (schema v1). Hand-edit JSON to change `schema_version: 2`. Attempt import. **Pass:** Import returns Failure ("Unsupported schema version: 2"). Proves version gating works for future migration.

### AC6 — Export/Import Speed
Export and import worlds at the Standard (80×52 = 4,160 cells) and Large (104×64 = 6,656 cells) sizes, ~20 sites total. Measure time for each operation. **Target: <3 seconds per operation** (does not block iteration workflow; designers can export/hand-edit/re-import within ~10 seconds total).

### AC8 — Terrain Fields and Topology *(added 2026-10-14)*
Round-trip a world containing non-trivial `elevation`/`latitude` values (including 0, 1, and values like 0.1 that are not exact in binary) and all four `water_source` values. **Pass:** every float and enum compares equal after import. Edit `"topology"` to `"square"`, set one `elevation` to 1.5, and set one `water_source` to `"Swamp"`, in turn. **Pass:** each import fails with a message naming the field. Edit a site position outside the grid. **Pass:** import fails.

### AC7 — Default Export Path (PD-009 Compliance)
Export world without specifying file path (use default). **Pass:** File written to `Cluiche/out/CoW/worlds/<seed>.wld.json` (or `Cluiche/out/CoW/worlds/world_<timestamp>.wld.json` if seed not available). Proves PD-009 compliance (all non-binary output under `out/<AppName>/`).

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Biome/TerrainType/Region/ApotheosisType enums serialized as strings (human-readable names), converted to StringCRC at import time via `BiomeFromString()` helpers |
| PD-004 | No STL containers in public APIs | `WorldStructure` uses `DynamicArrayC<T>` (already compliant per Feature 1); export/import internal code can use `std::ifstream`/`std::ofstream` (not public API) |
| PD-009 | All generated non-binary output lives under `Cluiche/out/<AppName>/` | Default export path is `Cluiche/out/CoW/worlds/`; user can override via Feature 6 debug host, but default must comply |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | Export/Import is **Feature-local** initially (lives in `WorldGenerationModule`); if other CoW systems need JSON world serialization, promote to **CoW-Shared** utility. Not Dia (too CoW-specific: uses CoW's Hearth/Nexus/world-throne types), not Baseline (no other game needs CoW's world format) |
| AD-003 | GDD owns design intent | GDD defines "export uses JSON, captures full structure not seed-only" (decision 4), round-trip guarantee (§6 Test 3), and hand-editing use case (decision 4). This spec defines JSON schema (v1 format, validation rules, string serialization) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | Should export default to `<seed>.wld.json` (deterministic filename) or `world_<timestamp>.wld.json` (avoids overwrite)? | Affects Feature 6 (Debug Host Integration) UX and AC7 test; does not block implementation (can change default easily) | Decide during Feature 6 spec (Debug Host Integration): if designers export multiple worlds per session, timestamp avoids overwrite; if they export once per seed for iteration, seed-based filename is clearer |
| 2 | Should JSON schema include generation metadata (seed, GenParams, retry count) for provenance tracking? | Nice-to-have for authored world documentation ("this world was generated from seed 12345 with Forest weight 0.8"); does not affect round-trip guarantee (metadata not part of `WorldStructure`) | Defer to later (S-1 MVP ships without metadata; add in v2 schema if designers request it) |
| 3 | Should import validate the three hard guarantees (call Feature 2 Reachability Validator)? | Hand-edited worlds might violate guarantees (designer moves Hearth to unreachable spot); import can either fail (strict) or warn (permissive) | Permissive: import succeeds but logs warning ("Imported world may violate guarantees, run validation manually"). Strict validation is Feature 2's job, not import's (designers might intentionally author broken worlds for testing) |

## Status

`Approved` — 2026-10-11. Amended 2026-10-14 (A4 Weather, hex world): `topology`, terrain fields, notification-only import event, map sizes.

---

## Implementation Notes

When implementation begins (after approval), create a plan file `json-export-import.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- JSON schema v1 full definition (all fields, types, required vs. optional)
- String conversion helpers (BiomeToString/FromString, etc.) — hardcoded maps or registry lookup?
- Validation rules on import (which invariants checked, error messages per failure type)
- Default file path logic (seed-based vs. timestamp-based filename, directory creation if missing)
- Round-trip test implementation (`WorldStructure::operator==`, field-by-field comparison)

These patterns are validated against actual implementation before tasks are marked Done.
