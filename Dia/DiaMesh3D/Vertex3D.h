#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector4D.h>

#include <stdint.h>

namespace Dia { namespace Mesh3D {

// Pure geometry — no skinning attributes. Skinning lives in DiaRig3D.
struct Vertex3D
{
    Dia::Maths::Vector3D position;  // 12 bytes
    Dia::Maths::Vector3D normal;    // 12 bytes
    Dia::Maths::Vector4D tangent;   // 16 bytes — xyz=tangent, w=bitangent sign
    Dia::Maths::Vector2D uv0;       //  8 bytes — primary UV channel
    uint32_t             colour;    //  4 bytes — packed RGBA8 (0xFFFFFFFF = opaque white)
};

static_assert(sizeof(Vertex3D) == 52, "Vertex3D size mismatch");

} }
