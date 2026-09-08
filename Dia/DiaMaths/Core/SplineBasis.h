#pragma once

// Scalar basis coefficients for cubic spline evaluation.
// Spline.cpp and Spline3D.cpp use these to avoid duplicating the same
// float arithmetic across 2D and 3D implementations.
//
// Usage:
//   float b0, b1, b2, b3;
//   CubicBSplineBasis(u, b0, b1, b2, b3);
//   result = b0*p0 + b1*p1 + b2*p2 + b3*p3;  // works for any vector type

namespace Dia { namespace Maths {

// Uniform cubic B-Spline basis (tension-free, C2 continuity).
// M_bs = (1/6) * [[-1  3 -3  1]
//                 [ 3 -6  3  0]
//                 [-3  0  3  0]
//                 [ 1  4  1  0]]
inline void CubicBSplineBasis(float u, float& b0, float& b1, float& b2, float& b3)
{
    const float u2 = u * u;
    const float u3 = u2 * u;
    b0 = (1.0f / 6.0f) * (-u3 + 3.0f*u2 - 3.0f*u + 1.0f);
    b1 = (1.0f / 6.0f) * (3.0f*u3 - 6.0f*u2 + 4.0f);
    b2 = (1.0f / 6.0f) * (-3.0f*u3 + 3.0f*u2 + 3.0f*u + 1.0f);
    b3 = (1.0f / 6.0f) * u3;
}

// First derivative of the uniform cubic B-Spline basis.
inline void CubicBSplineBasisTangent(float u, float& b0, float& b1, float& b2, float& b3)
{
    const float u2 = u * u;
    b0 = (1.0f / 6.0f) * (-3.0f*u2 + 6.0f*u - 3.0f);
    b1 = (1.0f / 6.0f) * (9.0f*u2 - 12.0f*u);
    b2 = (1.0f / 6.0f) * (-9.0f*u2 + 6.0f*u + 3.0f);
    b3 = (1.0f / 6.0f) * (3.0f*u2);
}

// Catmull-Rom basis (tension = 0.5, C1 continuity, passes through p1 and p2).
// M_cr = 0.5 * [[-1  3 -3  1]
//               [ 2 -5  4 -1]
//               [-1  0  1  0]
//               [ 0  2  0  0]]
inline void CatmullRomBasis(float u, float& b0, float& b1, float& b2, float& b3)
{
    const float u2 = u * u;
    const float u3 = u2 * u;
    b0 = 0.5f * (-u3 + 2.0f*u2 - u);
    b1 = 0.5f * (3.0f*u3 - 5.0f*u2 + 2.0f);
    b2 = 0.5f * (-3.0f*u3 + 4.0f*u2 + u);
    b3 = 0.5f * (u3 - u2);
}

// First derivative of the Catmull-Rom basis.
inline void CatmullRomBasisTangent(float u, float& b0, float& b1, float& b2, float& b3)
{
    const float u2 = u * u;
    b0 = 0.5f * (-3.0f*u2 + 4.0f*u - 1.0f);
    b1 = 0.5f * (9.0f*u2 - 10.0f*u);
    b2 = 0.5f * (-9.0f*u2 + 8.0f*u + 1.0f);
    b3 = 0.5f * (3.0f*u2 - 2.0f*u);
}

} } // namespace Dia::Maths
