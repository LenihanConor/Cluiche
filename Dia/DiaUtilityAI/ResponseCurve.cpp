#include <DiaUtilityAI/ResponseCurve.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    float Clamp01(float v)
    {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    float Sigmoid(float x)
    {
        return 1.0f / (1.0f + std::expf(-x));
    }

    // Default param values per shape
    constexpr float kDefaultQuadraticExponent  = 2.0f;
    constexpr float kDefaultExponentialK       = 4.0f;
    constexpr float kDefaultLogisticSteepness  = 10.0f;
    constexpr float kDefaultStepThreshold      = 0.5f;
}

namespace Dia
{
    namespace UtilityAI
    {
        ResponseCurve::ResponseCurve()
            : mShape(CurveShape::kLinear)
            , mParam(kDefaultQuadraticExponent)
            , mInvert(false)
        {
        }

        float ResponseCurve::Evaluate(float normalisedInput) const
        {
            const float x = Clamp01(normalisedInput);

            float result = 0.0f;

            switch (mShape)
            {
            case CurveShape::kLinear:
                result = x;
                break;

            case CurveShape::kQuadratic:
                result = std::powf(x, mParam);
                break;

            case CurveShape::kExponential:
            {
                const float k = mParam;
                const float denom = std::expf(k) - 1.0f;
                if (denom < 1e-7f)
                {
                    // k ≈ 0 → degenerate to linear
                    result = x;
                }
                else
                {
                    result = (std::expf(k * x) - 1.0f) / denom;
                }
                break;
            }

            case CurveShape::kLogistic:
            {
                const float s = mParam;
                const float lo  = Sigmoid(-s * 0.5f);
                const float hi  = Sigmoid( s * 0.5f);
                const float raw = Sigmoid(s * (x - 0.5f));
                const float range = hi - lo;
                if (range < 1e-7f)
                {
                    result = 0.5f;
                }
                else
                {
                    result = (raw - lo) / range;
                }
                break;
            }

            case CurveShape::kSine:
                result = std::sinf(x * kPi * 0.5f);
                break;

            case CurveShape::kStep:
                result = (x >= mParam) ? 1.0f : 0.0f;
                break;
            }

            if (mInvert)
            {
                result = 1.0f - result;
            }

            return Clamp01(result);
        }

        ResponseCurve ResponseCurve::LoadFromJson(const Json::Value& node)
        {
            ResponseCurve curve;

            if (node.isMember("shape") && node["shape"].isString())
            {
                const char* shapeStr = node["shape"].asCString();

                if (std::strcmp(shapeStr, "linear") == 0)
                {
                    curve.mShape = CurveShape::kLinear;
                    curve.mParam = 0.0f;
                }
                else if (std::strcmp(shapeStr, "quadratic") == 0)
                {
                    curve.mShape = CurveShape::kQuadratic;
                    curve.mParam = node.isMember("exponent") && node["exponent"].isNumeric()
                        ? node["exponent"].asFloat()
                        : kDefaultQuadraticExponent;
                }
                else if (std::strcmp(shapeStr, "exponential") == 0)
                {
                    curve.mShape = CurveShape::kExponential;
                    curve.mParam = node.isMember("k") && node["k"].isNumeric()
                        ? node["k"].asFloat()
                        : kDefaultExponentialK;
                }
                else if (std::strcmp(shapeStr, "logistic") == 0)
                {
                    curve.mShape = CurveShape::kLogistic;
                    curve.mParam = node.isMember("steepness") && node["steepness"].isNumeric()
                        ? node["steepness"].asFloat()
                        : kDefaultLogisticSteepness;
                }
                else if (std::strcmp(shapeStr, "sine") == 0)
                {
                    curve.mShape = CurveShape::kSine;
                    curve.mParam = 0.0f;
                }
                else if (std::strcmp(shapeStr, "step") == 0)
                {
                    curve.mShape = CurveShape::kStep;
                    curve.mParam = node.isMember("threshold") && node["threshold"].isNumeric()
                        ? node["threshold"].asFloat()
                        : kDefaultStepThreshold;
                }
                // else: unknown shape → defaults already set to kLinear
            }

            if (node.isMember("invert") && node["invert"].isBool())
            {
                curve.mInvert = node["invert"].asBool();
            }

            return curve;
        }

        CurveShape ResponseCurve::GetShape() const
        {
            return mShape;
        }

    } // namespace UtilityAI
} // namespace Dia
