#pragma once
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaCore/Colour/RGBA.h>

namespace Dia { namespace Lighting3D {

struct PointLight3D
{
    Dia::Maths::Vector3D position;
    float                radius    = 10.0f;
    Dia::Core::RGBA      colour    = { 255, 255, 255, 255 };
    float                intensity = 1.0f;
    bool                 enabled   = true;
};

} }
