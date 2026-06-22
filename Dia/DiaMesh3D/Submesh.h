#pragma once

#include <DiaCore/CRC/CRC.h>

#include <stdint.h>

namespace Dia { namespace Mesh3D {

// Wire-compatible with the .mesh3d flat binary: 3 × uint32 = 12 bytes.
// materialId is stored as a plain CRC hash (not StringCRC) so sizeof(Submesh)
// matches the cook tool's 12-byte submesh record exactly.
struct Submesh
{
    uint32_t       indexStart;  // first index in the mesh's index buffer
    uint32_t       indexCount;  // number of indices for this submesh
    Dia::Core::CRC materialId;  // CRC of source material name; resolved by DiaBgfx3D
};

static_assert(sizeof(Submesh) == 12, "Submesh size must match .mesh3d wire format");

} }
