#include "DiaMaths/Matrix/Matrix34.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Matrix/Matrix33.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Quaternion/Quaternion.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Maths
	{
		DIA_TYPE_DEFINITION(Matrix34)
			DIA_TYPE_ADD_VARIABLE("m00", m[0][0])
			DIA_TYPE_ADD_VARIABLE("m01", m[0][1])
			DIA_TYPE_ADD_VARIABLE("m02", m[0][2])
			DIA_TYPE_ADD_VARIABLE("m03", m[0][3])
			DIA_TYPE_ADD_VARIABLE("m10", m[1][0])
			DIA_TYPE_ADD_VARIABLE("m11", m[1][1])
			DIA_TYPE_ADD_VARIABLE("m12", m[1][2])
			DIA_TYPE_ADD_VARIABLE("m13", m[1][3])
			DIA_TYPE_ADD_VARIABLE("m20", m[2][0])
			DIA_TYPE_ADD_VARIABLE("m21", m[2][1])
			DIA_TYPE_ADD_VARIABLE("m22", m[2][2])
			DIA_TYPE_ADD_VARIABLE("m23", m[2][3])
		DIA_TYPE_DEFINITION_END()

		//-----------------------------------------------------------------------------
		// Factory methods
		//-----------------------------------------------------------------------------

		Matrix34 Matrix34::Identity()
		{
			return Matrix34(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			);
		}

		Matrix34 Matrix34::FromTranslation(const Vector3D& translation)
		{
			return Matrix34(
				1.0f, 0.0f, 0.0f, translation.x,
				0.0f, 1.0f, 0.0f, translation.y,
				0.0f, 0.0f, 1.0f, translation.z
			);
		}

		Matrix34 Matrix34::FromRotation(const Quaternion& rotation)
		{
			// Get 3x3 rotation matrix from quaternion
			Matrix33 rot33 = rotation.ToMatrix33();

			// Copy the 3x3 rotation block, leave translation column zero
			return Matrix34(
				rot33.m[0][0], rot33.m[0][1], rot33.m[0][2], 0.0f,
				rot33.m[1][0], rot33.m[1][1], rot33.m[1][2], 0.0f,
				rot33.m[2][0], rot33.m[2][1], rot33.m[2][2], 0.0f
			);
		}

		Matrix34 Matrix34::FromScale(const Vector3D& scale)
		{
			return Matrix34(
				scale.x, 0.0f, 0.0f, 0.0f,
				0.0f, scale.y, 0.0f, 0.0f,
				0.0f, 0.0f, scale.z, 0.0f
			);
		}

		Matrix34 Matrix34::FromScale(float uniformScale)
		{
			return FromScale(Vector3D(uniformScale, uniformScale, uniformScale));
		}

		Matrix34 Matrix34::FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale)
		{
			// TRS composition: Translation * Rotation * Scale
			Matrix34 t = FromTranslation(translation);
			Matrix34 r = FromRotation(rotation);
			Matrix34 s = FromScale(scale);
			return t * r * s;
		}

		Matrix34 Matrix34::FromMatrix44(const Matrix44& transform)
		{
			// Assert in debug that row 3 is approximately (0, 0, 0, 1)
#ifdef _DEBUG
			constexpr float kThreshold = 1e-5f;
			DIA_ASSERT(
				Dia::Maths::Float::FAbs(transform.m[3][0] - 0.0f) < kThreshold &&
				Dia::Maths::Float::FAbs(transform.m[3][1] - 0.0f) < kThreshold &&
				Dia::Maths::Float::FAbs(transform.m[3][2] - 0.0f) < kThreshold &&
				Dia::Maths::Float::FAbs(transform.m[3][3] - 1.0f) < kThreshold,
				"FromMatrix44: input is not affine (row 3 must be (0,0,0,1))"
			);
#endif

			// Copy rows 0-2
			return Matrix34(
				transform.m[0][0], transform.m[0][1], transform.m[0][2], transform.m[0][3],
				transform.m[1][0], transform.m[1][1], transform.m[1][2], transform.m[1][3],
				transform.m[2][0], transform.m[2][1], transform.m[2][2], transform.m[2][3]
			);
		}

		Matrix44 Matrix34::ToMatrix44() const
		{
			// Append implicit (0, 0, 0, 1) row
			return Matrix44(
				m[0][0], m[0][1], m[0][2], m[0][3],
				m[1][0], m[1][1], m[1][2], m[1][3],
				m[2][0], m[2][1], m[2][2], m[2][3],
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		//-----------------------------------------------------------------------------
		// Matrix multiplication (respects implicit (0,0,0,1) bottom row)
		//-----------------------------------------------------------------------------

		Matrix34 Matrix34::operator*(const Matrix34& other) const
		{
			Matrix34 result;

			// For each row and column in the result
			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 4; ++c)
				{
					// Standard 4x4 multiplication but only computing the top 3 rows
					// For affine matrices, the implicit 4th row [0,0,0,1] * [other] = [0,0,0,1]
					result.m[r][c] = 0.0f;

					// Multiply row r of this by column c of other
					for (int k = 0; k < 3; ++k)
					{
						result.m[r][c] += m[r][k] * other.m[k][c];
					}

					// Add the implicit (0,0,0,1) contribution for translation column
					if (c == 3)
					{
						result.m[r][c] += m[r][3];  // This is m[r][3] * 1 from the implicit row
					}
				}
			}

			return result;
		}

		//-----------------------------------------------------------------------------
		// Matrix inverse (affine-specific formula — faster than 4x4)
		//-----------------------------------------------------------------------------

		Matrix34 Matrix34::Inverse() const
		{
			// Affine inverse: Let R = upper-left 3x3, t = translation column
			// R_inv = inverse of R
			// New translation = -(R_inv * t)

			// Calculate determinant of upper-left 3x3
			float det = Determinant();

			// Check for singular matrix
			if (Dia::Maths::Float::FAbs(det) < FLOAT_EPSILON)
			{
				DIA_ASSERT(false, "Matrix34::Inverse: singular matrix (determinant near zero)");
				return Identity();
			}

			float invDet = 1.0f / det;

			// Calculate cofactor matrix for upper-left 3x3 (same as Matrix33::Inverse)
			Matrix34 result;

			result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
			result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
			result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

			result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
			result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
			result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;

			result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
			result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
			result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

			// New translation = -(R_inv * t)
			Vector3D t(m[0][3], m[1][3], m[2][3]);
			Vector3D invT(
				-(result.m[0][0] * t.x + result.m[0][1] * t.y + result.m[0][2] * t.z),
				-(result.m[1][0] * t.x + result.m[1][1] * t.y + result.m[1][2] * t.z),
				-(result.m[2][0] * t.x + result.m[2][1] * t.y + result.m[2][2] * t.z)
			);

			result.m[0][3] = invT.x;
			result.m[1][3] = invT.y;
			result.m[2][3] = invT.z;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Component extraction
		//-----------------------------------------------------------------------------

		Vector3D Matrix34::GetTranslation() const
		{
			return Vector3D(m[0][3], m[1][3], m[2][3]);
		}

		Quaternion Matrix34::GetRotation() const
		{
			// Extract scale first
			Vector3D scale = GetScale();

			// Extract upper-left 3x3 rotation matrix, removing scale
			// Guard against zero-scale by using safe normalization
			float sx = (scale.x > FLOAT_EPSILON) ? (1.0f / scale.x) : 1.0f;
			float sy = (scale.y > FLOAT_EPSILON) ? (1.0f / scale.y) : 1.0f;
			float sz = (scale.z > FLOAT_EPSILON) ? (1.0f / scale.z) : 1.0f;

			Matrix33 rotationMatrix(
				m[0][0] * sx, m[0][1] * sy, m[0][2] * sz,
				m[1][0] * sx, m[1][1] * sy, m[1][2] * sz,
				m[2][0] * sx, m[2][1] * sy, m[2][2] * sz
			);

			// Delegate to Quaternion::FromMatrix33
			return Quaternion::FromMatrix33(rotationMatrix);
		}

		Vector3D Matrix34::GetScale() const
		{
			// Extract transformed basis vectors from upper-left 3x3 columns
			Vector3D xAxis(m[0][0], m[1][0], m[2][0]);
			Vector3D yAxis(m[0][1], m[1][1], m[2][1]);
			Vector3D zAxis(m[0][2], m[1][2], m[2][2]);

			// Scale is the magnitude of these transformed unit vectors
			return Vector3D(xAxis.Magnitude(), yAxis.Magnitude(), zAxis.Magnitude());
		}
	}
}
