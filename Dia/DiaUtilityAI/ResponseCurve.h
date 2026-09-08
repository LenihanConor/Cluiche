#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace UtilityAI
    {
        enum class CurveShape
        {
            kLinear,
            kQuadratic,
            kExponential,
            kLogistic,
            kSine,
            kStep
        };

        // Maps a normalised float input [0,1] to a score [0,1].
        // Additional parameters (exponent, k, steepness, threshold) are curve-specific.
        // When invert is true, output = 1.0f - computed_output.
        class ResponseCurve
        {
        public:
            ResponseCurve();

            // Evaluate the curve at normalised input [0,1]. Output clamped to [0,1].
            float Evaluate(float normalisedInput) const;

            // JSON format: { "shape": "quadratic", "exponent": 2.0, "invert": false }
            static ResponseCurve LoadFromJson(const Json::Value& node);

            CurveShape GetShape() const;

        private:
            CurveShape mShape;
            float mParam;    // exponent / k / steepness / threshold (curve-specific)
            bool  mInvert;
        };

    } // namespace UtilityAI
} // namespace Dia
