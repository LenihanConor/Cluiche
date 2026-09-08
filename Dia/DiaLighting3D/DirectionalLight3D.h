#pragma once
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaCore/Colour/RGBA.h>

namespace Dia { namespace Lighting3D {

struct DirectionalLight3D
{
    Dia::Maths::Vector3D direction  = { 0.0f, -1.0f, 0.0f };  // unit vector, world space, points away from surface
    Dia::Core::RGBA      colour     = { 255, 255, 230, 255 };  // warm white default
    float                intensity  = 1.0f;
    bool                 enabled    = true;
};

} }
