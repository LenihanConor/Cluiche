#pragma once
#include <DiaCore/Colour/RGBA.h>

namespace Dia { namespace Lighting3D {

struct AmbientLight3D
{
    Dia::Core::RGBA colour    = { 30, 30, 50, 255 };  // dim cool default
    float           intensity = 0.1f;
    bool            enabled   = true;
};

} }
