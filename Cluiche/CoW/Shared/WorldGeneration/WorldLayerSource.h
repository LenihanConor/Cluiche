// S-1 World Generation map view: descriptor table and readers that feed the DiaCellLayers viewer.
// Pure CoW-Shared code: each reader copies one generated layer from a WorldGenSession into a caller-supplied
// float buffer of width*height (uint layers widened to float, Cell fields strided out of WorldStructure).
// Landmass ids (0..255) exceed the 64-colour category palette, so landmass is a Float layer with a ramp.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/WorldGenSession.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

enum class WorldLayerKind : uint8_t { Float, Category };

// Fills out[0..count) from the session; zero-fills when no world has been generated yet.
using WorldLayerReadFn = void (*)(const WorldGenSession& session, float* out, uint32_t count);

struct WorldLayerDesc
{
    const char*      id;              // layer id and schema entry key, e.g. "terrainType"
    const char*      schemaKey;       // full schema key: "<kWorldLayerSchemaFor>/<id>"
    const char*      group;           // viewer picker group
    WorldLayerKind   kind;
    uint32_t         categoryCount;   // enum value count for Category layers, 0 for Float
    WorldLayerReadFn read;
    bool             defaultBase;     // the viewer's default base layer
};

// Schema file "for" value; the layer schema keys are "<for>/<id>".
inline constexpr const char* kWorldLayerSchemaFor = "cow.worldgen.layers";

class WorldLayerSource
{
public:
    static uint32_t              Count();
    static const WorldLayerDesc& At(uint32_t index);
    // nullptr when the id is unknown.
    static const WorldLayerDesc* Find(const char* id);

    // The version stamp of the data the readers return: changes only when a generation changed the world.
    static uint32_t Version(const WorldGenSession& session) { return session.Revision(); }
};

} // namespace WorldGeneration
} // namespace CoW
