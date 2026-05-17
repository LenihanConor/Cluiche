#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Matrix34::Matrix34()
		{
			SetIdentity();
		}

		inline Matrix34::Matrix34(const Matrix34& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] = other.m[i][j];
				}
			}
		}

		inline Matrix34::Matrix34(float m00, float m01, float m02, float m03,
		                           float m10, float m11, float m12, float m13,
		                           float m20, float m21, float m22, float m23)
		{
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		}

		//-----------------------------------------------------------------------------
		// Assignment
		//-----------------------------------------------------------------------------

		inline Matrix34& Matrix34::operator=(const Matrix34& other)
		{
			if (this != &other)
			{
				for (int i = 0; i < 3; ++i)
				{
					for (int j = 0; j < 4; ++j)
					{
						m[i][j] = other.m[i][j];
					}
				}
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		// Element access
		//-----------------------------------------------------------------------------

		inline float& Matrix34::operator()(int row, int col)
		{
			return m[row][col];
		}

		inline float Matrix34::operator()(int row, int col) const
		{
			return m[row][col];
		}

		//-----------------------------------------------------------------------------
		// Matrix operations
		//-----------------------------------------------------------------------------

		inline Matrix34& Matrix34::operator+=(const Matrix34& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] += other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix34& Matrix34::operator-=(const Matrix34& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] -= other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix34& Matrix34::operator*=(const Matrix34& other)
		{
			*this = *this * other;
			return *this;
		}

		inline Matrix34& Matrix34::operator*=(float scalar)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] *= scalar;
				}
			}
			return *this;
		}

		inline Matrix34 Matrix34::operator-() const
		{
			Matrix34 result;
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					result.m[i][j] = -m[i][j];
				}
			}
			return result;
		}

		inline Matrix34 Matrix34::operator+(const Matrix34& other) const
		{
			Matrix34 result = *this;
			result += other;
			return result;
		}

		inline Matrix34 Matrix34::operator-(const Matrix34& other) const
		{
			Matrix34 result = *this;
			result -= other;
			return result;
		}

		inline Matrix34 Matrix34::operator*(float scalar) const
		{
			Matrix34 result = *this;
			result *= scalar;
			return result;
		}

		inline bool Matrix34::operator==(const Matrix34& other) const
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					if (m[i][j] != other.m[i][j])
					{
						return false;
					}
				}
			}
			return true;
		}

		inline bool Matrix34::operator!=(const Matrix34& other) const
		{
			return !(*this == other);
		}

		//-----------------------------------------------------------------------------
		// Matrix properties
		//-----------------------------------------------------------------------------

		inline void Matrix34::SetIdentity()
		{
			m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
			m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
			m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
		}

		inline float Matrix34::Determinant() const
		{
			// Determinant of upper-left 3x3
			return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
			     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
			     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		}

		//-----------------------------------------------------------------------------
		// Transform vectors
		//-----------------------------------------------------------------------------

		inline Vector3D Matrix34::TransformPoint(const Vector3D& point) const
		{
			// Treat point as (x, y, z, 1) with translation applied
			return Vector3D(
				m[0][0] * point.x + m[0][1] * point.y + m[0][2] * point.z + m[0][3],
				m[1][0] * point.x + m[1][1] * point.y + m[1][2] * point.z + m[1][3],
				m[2][0] * point.x + m[2][1] * point.y + m[2][2] * point.z + m[2][3]
			);
		}

		inline Vector3D Matrix34::TransformDirection(const Vector3D& dir) const
		{
			// Treat direction as (x, y, z, 0) without translation
			return Vector3D(
				m[0][0] * dir.x + m[0][1] * dir.y + m[0][2] * dir.z,
				m[1][0] * dir.x + m[1][1] * dir.y + m[1][2] * dir.z,
				m[2][0] * dir.x + m[2][1] * dir.y + m[2][2] * dir.z
			);
		}
	}
}
