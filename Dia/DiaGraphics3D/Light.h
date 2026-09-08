#pragma once
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia { namespace Graphics3D {

struct AmbientLight
{
    Dia::Graphics::RGBA colour;
    float               intensity;  // HDR-range multiplier

    AmbientLight() : colour(230, 230, 217, 255), intensity(0.2f) {}
};

struct DirectionalLight
{
    Dia::Maths::Vector3D direction;             // unit vector, points away from surface
    Dia::Graphics::RGBA  colour;
    float                intensity = 1.0f;  // HDR-range multiplier
};

struct PointLight
{
    Dia::Maths::Vector3D position;
    Dia::Graphics::RGBA  colour;
    float                intensity;
    float                range;      // contribution zero beyond this
};

} }
