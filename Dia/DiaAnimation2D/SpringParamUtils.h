#pragma once
#include "SpringNodeDef.h"

namespace Dia { namespace Animation2D {
    // Returns SpringParams (alias for SpringNodeDef) computed from physics parameters.
    SpringParams SpringParamsFromFrequency(float frequency, float dampingRatio, float mass = 1.0f);
} }
