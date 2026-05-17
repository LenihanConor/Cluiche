#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Vector/Vector4D.h"
#include "DiaMaths/Matrix/Matrix33.h"
#include "DiaMaths/Quaternion/Quaternion.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Maths
	{
		DIA_TYPE_DEFINITION(Matrix44)
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
			DIA_TYPE_ADD_VARIABLE("m30", m[3][0])
			DIA_TYPE_ADD_VARIABLE("m31", m[3][1])
			DIA_TYPE_ADD_VARIABLE("m32", m[3][2])
			DIA_TYPE_ADD_VARIABLE("m33", m[3][3])
		DIA_TYPE_DEFINITION_END()

		//-----------------------------------------------------------------------------
		// Factory methods
		//-----------------------------------------------------------------------------

		Matrix44 Matrix44::Identity()
		{
			return Matrix44(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		Matrix44 Matrix44::FromTranslation(const Vector3D& translation)
		{
			return Matrix44(
				1.0f, 0.0f, 0.0f, translation.x,
				0.0f, 1.0f, 0.0f, translation.y,
				0.0f, 0.0f, 1.0f, translation.z,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		Matrix44 Matrix44::FromRotation(const Quaternion& rotation)
		{
			// Delegate to Quaternion::ToMatrix44
			return rotation.ToMatrix44();
		}

		Matrix44 Matrix44::FromScale(const Vector3D& scale)
		{
			return Matrix44(
				scale.x, 0.0f, 0.0f, 0.0f,
				0.0f, scale.y, 0.0f, 0.0f,
				0.0f, 0.0f, scale.z, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		Matrix44 Matrix44::FromScale(float uniformScale)
		{
			return FromScale(Vector3D(uniformScale, uniformScale, uniformScale));
		}

		Matrix44 Matrix44::FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale)
		{
			// TRS composition: Translation * Rotation * Scale
			// Applied to a point, scales then rotates then translates
			Matrix44 t = FromTranslation(translation);
			Matrix44 r = FromRotation(rotation);
			Matrix44 s = FromScale(scale);
			return t * r * s;
		}

		//-----------------------------------------------------------------------------
		// Projection / View builders
		//-----------------------------------------------------------------------------

		Matrix44 Matrix44::Perspective(const Angle& fovY, float aspect, float nearZ, float farZ)
		{
			// Y-up right-handed perspective matrix targeting OpenGL [-1, 1] clip-space depth
			float f = 1.0f / Dia::Maths::Tan(fovY.AsRadians() / 2.0f);

			Matrix44 result;
			result.m[0][0] = f / aspect;
			result.m[0][1] = 0.0f;
			result.m[0][2] = 0.0f;
			result.m[0][3] = 0.0f;

			result.m[1][0] = 0.0f;
			result.m[1][1] = f;
			result.m[1][2] = 0.0f;
			result.m[1][3] = 0.0f;

			result.m[2][0] = 0.0f;
			result.m[2][1] = 0.0f;
			result.m[2][2] = (farZ + nearZ) / (nearZ - farZ);
			result.m[2][3] = (2.0f * farZ * nearZ) / (nearZ - farZ);

			result.m[3][0] = 0.0f;
			result.m[3][1] = 0.0f;
			result.m[3][2] = -1.0f;
			result.m[3][3] = 0.0f;

			return result;
		}

		Matrix44 Matrix44::Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ)
		{
			// Y-up right-handed orthographic projection, OpenGL [-1, 1] depth
			Matrix44 result;
			result.m[0][0] = 2.0f / (right - left);
			result.m[0][1] = 0.0f;
			result.m[0][2] = 0.0f;
			result.m[0][3] = -(right + left) / (right - left);

			result.m[1][0] = 0.0f;
			result.m[1][1] = 2.0f / (top - bottom);
			result.m[1][2] = 0.0f;
			result.m[1][3] = -(top + bottom) / (top - bottom);

			result.m[2][0] = 0.0f;
			result.m[2][1] = 0.0f;
			result.m[2][2] = -2.0f / (farZ - nearZ);
			result.m[2][3] = -(farZ + nearZ) / (farZ - nearZ);

			result.m[3][0] = 0.0f;
			result.m[3][1] = 0.0f;
			result.m[3][2] = 0.0f;
			result.m[3][3] = 1.0f;

			return result;
		}

		Matrix44 Matrix44::LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up)
		{
			// Y-up right-handed view matrix
			Vector3D forward = (target - eye).AsNormal();
			Vector3D right = forward.Cross(up).AsNormal();
			Vector3D trueUp = right.Cross(forward);

			// Check for degenerate case (forward parallel to up)
			if (right.SquareMagnitude() < FLOAT_EPSILON)
			{
				DIA_ASSERT(false, "LookAt: forward and up vectors are parallel");
				return Matrix44::Identity();
			}

			// Build view matrix: rows = (right, trueUp, -forward)
			// Translation = -dot(axis, eye)
			Matrix44 result;
			result.m[0][0] = right.x;
			result.m[0][1] = right.y;
			result.m[0][2] = right.z;
			result.m[0][3] = -right.Dot(eye);

			result.m[1][0] = trueUp.x;
			result.m[1][1] = trueUp.y;
			result.m[1][2] = trueUp.z;
			result.m[1][3] = -trueUp.Dot(eye);

			result.m[2][0] = -forward.x;
			result.m[2][1] = -forward.y;
			result.m[2][2] = -forward.z;
			result.m[2][3] = forward.Dot(eye);

			result.m[3][0] = 0.0f;
			result.m[3][1] = 0.0f;
			result.m[3][2] = 0.0f;
			result.m[3][3] = 1.0f;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Transform vectors
		//-----------------------------------------------------------------------------

		Vector3D Matrix44::TransformPoint(const Vector3D& point) const
		{
			// Treat point as (x, y, z, 1) for homogeneous coordinates
			float x = m[0][0] * point.x + m[0][1] * point.y + m[0][2] * point.z + m[0][3];
			float y = m[1][0] * point.x + m[1][1] * point.y + m[1][2] * point.z + m[1][3];
			float z = m[2][0] * point.x + m[2][1] * point.y + m[2][2] * point.z + m[2][3];
			float w = m[3][0] * point.x + m[3][1] * point.y + m[3][2] * point.z + m[3][3];

			// Homogeneous divide if w != 1
			if (Dia::Maths::Float::FAbs(w - 1.0f) > FLOAT_EPSILON)
			{
				float invW = 1.0f / w;
				return Vector3D(x * invW, y * invW, z * invW);
			}

			return Vector3D(x, y, z);
		}

		Vector3D Matrix44::TransformDirection(const Vector3D& direction) const
		{
			// Treat direction as (x, y, z, 0) - no translation
			return Vector3D(
				m[0][0] * direction.x + m[0][1] * direction.y + m[0][2] * direction.z,
				m[1][0] * direction.x + m[1][1] * direction.y + m[1][2] * direction.z,
				m[2][0] * direction.x + m[2][1] * direction.y + m[2][2] * direction.z
			);
		}

		//-----------------------------------------------------------------------------
		// Matrix inverse (expensive operation!)
		//-----------------------------------------------------------------------------

		Matrix44 Matrix44::Inverse() const
		{
			float det = Determinant();

			// Check for singular matrix (determinant near zero)
			// A singular matrix cannot be inverted (like dividing by zero)
			if (Dia::Maths::Float::FAbs(det) < FLOAT_EPSILON)
			{
				// Return identity as safe fallback for non-invertible matrix
				return Matrix44::Identity();
			}

			float invDet = 1.0f / det;

			Matrix44 result;

			// Calculate cofactor matrix and transpose (adjugate method)
			// Row 0
			result.m[0][0] = invDet * (
				m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
				m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])
			);

			result.m[0][1] = -invDet * (
				m[0][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[0][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
				m[0][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])
			);

			result.m[0][2] = invDet * (
				m[0][1] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) -
				m[0][2] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) +
				m[0][3] * (m[1][1] * m[3][2] - m[1][2] * m[3][1])
			);

			result.m[0][3] = -invDet * (
				m[0][1] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) -
				m[0][2] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) +
				m[0][3] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
			);

			// Row 1
			result.m[1][0] = -invDet * (
				m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])
			);

			result.m[1][1] = invDet * (
				m[0][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[0][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[0][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])
			);

			result.m[1][2] = -invDet * (
				m[0][0] * (m[1][2] * m[3][3] - m[1][3] * m[3][2]) -
				m[0][2] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) +
				m[0][3] * (m[1][0] * m[3][2] - m[1][2] * m[3][0])
			);

			result.m[1][3] = invDet * (
				m[0][0] * (m[1][2] * m[2][3] - m[1][3] * m[2][2]) -
				m[0][2] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) +
				m[0][3] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
			);

			// Row 2
			result.m[2][0] = invDet * (
				m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
				m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			result.m[2][1] = -invDet * (
				m[0][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
				m[0][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[0][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			result.m[2][2] = invDet * (
				m[0][0] * (m[1][1] * m[3][3] - m[1][3] * m[3][1]) -
				m[0][1] * (m[1][0] * m[3][3] - m[1][3] * m[3][0]) +
				m[0][3] * (m[1][0] * m[3][1] - m[1][1] * m[3][0])
			);

			result.m[2][3] = -invDet * (
				m[0][0] * (m[1][1] * m[2][3] - m[1][3] * m[2][1]) -
				m[0][1] * (m[1][0] * m[2][3] - m[1][3] * m[2][0]) +
				m[0][3] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])
			);

			// Row 3
			result.m[3][0] = -invDet * (
				m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
				m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
				m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			result.m[3][1] = invDet * (
				m[0][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
				m[0][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
				m[0][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			result.m[3][2] = -invDet * (
				m[0][0] * (m[1][1] * m[3][2] - m[1][2] * m[3][1]) -
				m[0][1] * (m[1][0] * m[3][2] - m[1][2] * m[3][0]) +
				m[0][2] * (m[1][0] * m[3][1] - m[1][1] * m[3][0])
			);

			result.m[3][3] = invDet * (
				m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
				m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
				m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])
			);

			return result;
		}

		//-----------------------------------------------------------------------------
		// OpenGL / glTF interop
		//-----------------------------------------------------------------------------

		void Matrix44::GetColumnMajor(float outData[16]) const
		{
			// outData[col * 4 + row] = m[row][col]
			for (int col = 0; col < 4; ++col)
			{
				for (int row = 0; row < 4; ++row)
				{
					outData[col * 4 + row] = m[row][col];
				}
			}
		}

		//-----------------------------------------------------------------------------
		// Component extraction (assumes affine input)
		//-----------------------------------------------------------------------------

		Vector3D Matrix44::GetTranslation() const
		{
			return Vector3D(m[0][3], m[1][3], m[2][3]);
		}

		Quaternion Matrix44::GetRotation() const
		{
			// Extract scale first
			Vector3D scale = GetScale();

			// Extract upper-left 3x3 rotation matrix, removing scale
			Matrix33 rotationMatrix(
				m[0][0] / scale.x, m[0][1] / scale.y, m[0][2] / scale.z,
				m[1][0] / scale.x, m[1][1] / scale.y, m[1][2] / scale.z,
				m[2][0] / scale.x, m[2][1] / scale.y, m[2][2] / scale.z
			);

			// Delegate to Quaternion::FromMatrix33
			return Quaternion::FromMatrix33(rotationMatrix);
		}

		Vector3D Matrix44::GetScale() const
		{
			// Extract transformed basis vectors from matrix columns (upper-left 3x3)
			Vector3D xAxis(m[0][0], m[1][0], m[2][0]);
			Vector3D yAxis(m[0][1], m[1][1], m[2][1]);
			Vector3D zAxis(m[0][2], m[1][2], m[2][2]);

			// Scale is the magnitude of these transformed unit vectors
			return Vector3D(xAxis.Magnitude(), yAxis.Magnitude(), zAxis.Magnitude());
		}
	}
}
