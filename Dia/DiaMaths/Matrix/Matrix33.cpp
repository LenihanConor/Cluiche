#include "DiaMaths/Matrix/Matrix33.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Calculate matrix inverse (expensive operation!)
		//
		// MATH: Inverse is calculated using adjugate matrix method
		//   M^-1 = adj(M) / det(M)
		//
		// USE CASES:
		//   - Converting from world space to local space
		//   - Undoing transformations
		//   - Camera view matrices
		//
		// PERFORMANCE: This is expensive (multiple multiplications/divisions)
		//              Cache the result if you need to use it multiple times!
		//
		// SPECIAL CASE: If determinant is near zero, matrix is singular
		//               (non-invertible), so we return identity as safe fallback
		//-----------------------------------------------------------------------------
		Matrix33 Matrix33::Inverse() const
		{
			float det = Determinant();

			// Check for singular matrix (determinant near zero)
			// A singular matrix cannot be inverted (like dividing by zero)
			if (Dia::Maths::Float::FAbs(det) < FLOAT_EPSILON)
			{
				// Return identity as safe fallback for non-invertible matrix
				return Matrix33::Identity();
			}

			float invDet = 1.0f / det;

			Matrix33 result;

			// Calculate cofactor matrix and transpose
			result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
			result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
			result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

			result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
			result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
			result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;

			result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
			result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
			result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Extract rotation angle from transformation matrix
		//
		// EXPLANATION:
		//   A transformation matrix encodes scale, rotation, and translation
		//   To get just rotation, we need to:
		//     1. Extract the rotational part (upper-left 2x2)
		//     2. Remove scale by normalizing
		//     3. Use atan2 to get angle from the normalized rotation matrix
		//
		// MATH: For rotation matrix R:
		//   R = [ cos(θ)  -sin(θ) ]
		//       [ sin(θ)   cos(θ) ]
		//   Therefore: θ = atan2(R[1,0], R[0,0])
		//
		// With scale: First column is X-axis * scaleX, so normalize first
		//-----------------------------------------------------------------------------
		Angle Matrix33::GetRotation() const
		{
			// Extract X-axis from matrix (first column)
			// This is the transformed unit X vector
			Vector2D xAxis(m[0][0], m[1][0]);
			float xLength = xAxis.Magnitude();

			// Check for degenerate case (zero scale)
			if (xLength < FLOAT_EPSILON)
			{
				return Angle::Deg0; // No valid rotation if scale is zero
			}

			// Normalize to remove scale component
			// Now we have pure rotation applied to unit X vector
			xAxis = xAxis / xLength;

			// Calculate rotation angle using atan2
			// atan2(y, x) gives us angle from positive X-axis
			float angleRadians = Dia::Maths::ATan2(xAxis.y, xAxis.x);
			return Angle::FromRadians(angleRadians);
		}

		//-----------------------------------------------------------------------------
		// Extract scale from transformation matrix
		//
		// EXPLANATION:
		//   Scale is encoded as the length of the basis vectors
		//   When we transform unit vectors [1,0] and [0,1], they become
		//   the matrix's columns, stretched by the scale factors
		//
		// MATH:
		//   scaleX = length of first column (transformed X-axis)
		//   scaleY = length of second column (transformed Y-axis)
		//
		// NOTE: This assumes uniform scale along each axis
		//       Non-uniform or sheared transforms work correctly
		//-----------------------------------------------------------------------------
		Vector2D Matrix33::GetScale() const
		{
			// Extract transformed basis vectors from matrix columns
			Vector2D xAxis(m[0][0], m[1][0]); // First column = transformed X unit vector
			Vector2D yAxis(m[0][1], m[1][1]); // Second column = transformed Y unit vector

			// Scale is the magnitude of these transformed unit vectors
			return Vector2D(xAxis.Magnitude(), yAxis.Magnitude());
		}
	}
}
