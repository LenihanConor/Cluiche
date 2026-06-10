#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include <stdint.h>

namespace Dia { namespace Mesh3D {

struct Submesh
{
    uint32_t             indexStart;  // first index in the mesh's index buffer
    uint32_t             indexCount;  // number of indices for this submesh
    Dia::Core::StringCRC materialId;  // hash of source material name; resolved by DiaBgfx3D
};

} }
