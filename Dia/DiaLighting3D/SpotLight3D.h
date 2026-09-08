#pragma once
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaCore/Colour/RGBA.h>

namespace Dia { namespace Lighting3D {

struct SpotLight3D
{
    Dia::Maths::Vector3D position;
    Dia::Maths::Vector3D direction  = { 0.0f, -1.0f, 0.0f };  // unit vector, world space
    float                innerAngle = 15.0f;  // degrees — full-intensity cone
    float                outerAngle = 30.0f;  // degrees — falloff edge
    float                range      = 20.0f;
    Dia::Core::RGBA      colour     = { 255, 255, 255, 255 };
    float                intensity  = 1.0f;
    bool                 enabled    = true;
};

} }
