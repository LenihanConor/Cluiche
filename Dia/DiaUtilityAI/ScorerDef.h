#pragma once

#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace UtilityAI
    {
        struct ScorerDef
        {
            Dia::Core::StringCRC slot;    // IConditionContext slot to read
            Dia::Core::StringCRC field;   // field within slot
            float inputMin;               // raw value mapped to 0
            float inputMax;               // raw value mapped to 1
            ResponseCurve curve;

            ScorerDef() : inputMin(0.0f), inputMax(1.0f) {}
        };

    } // namespace UtilityAI
} // namespace Dia
