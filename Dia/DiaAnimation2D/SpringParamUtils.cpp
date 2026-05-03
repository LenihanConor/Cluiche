#include "SpringParamUtils.h"
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Animation2D {

SpringParams SpringParamsFromFrequency(float frequency, float dampingRatio, float mass) {
    DIA_ASSERT(frequency > 0.0f, "frequency must be positive");
    DIA_ASSERT(dampingRatio >= 0.0f, "dampingRatio must be non-negative");
    DIA_ASSERT(mass > 0.0f, "mass must be positive");

    const float twoPiF = 2.0f * 3.14159265358979323846f * frequency;
    SpringParams result;
    result.stiffness = twoPiF * twoPiF * mass;
    result.damping   = 2.0f * dampingRatio * twoPiF * mass;
    result.maxAngularVelocity = 20.0f;
    return result;
}

} }
