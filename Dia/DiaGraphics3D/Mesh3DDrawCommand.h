#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Matrix/Matrix44.h>

namespace Dia { namespace Graphics3D {

struct Mesh3DDrawCommand
{
    Mesh3DDrawCommand();

    Dia::Core::StringCRC meshId;               // asset id -> vertex/index buffers
    Dia::Core::StringCRC materialId;           // resolved by renderer to shader + params
    Dia::Maths::Matrix44 transform;            // model -> world (row-major)
    uint32_t             skinningPaletteIndex; // 0 = static mesh
    int16_t              layer;
};

} }
