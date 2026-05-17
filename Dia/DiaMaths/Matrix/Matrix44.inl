#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Matrix44::Matrix44()
		{
			SetIdentity();
		}

		inline Matrix44::Matrix44(const Matrix44& other)
		{
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] = other.m[i][j];
				}
			}
		}

		inline Matrix44::Matrix44(float m00, float m01, float m02, float m03,
		                           float m10, float m11, float m12, float m13,
		                           float m20, float m21, float m22, float m23,
		                           float m30, float m31, float m32, float m33)
		{
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
			m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
		}

		//-----------------------------------------------------------------------------
		// Assignment
		//-----------------------------------------------------------------------------

		inline Matrix44& Matrix44::operator=(const Matrix44& other)
		{
			if (this != &other)
			{
				for (int i = 0; i < 4; ++i)
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

		inline float& Matrix44::operator()(int row, int col)
		{
			return m[row][col];
		}

		inline float Matrix44::operator()(int row, int col) const
		{
			return m[row][col];
		}

		//-----------------------------------------------------------------------------
		// Matrix operations
		//-----------------------------------------------------------------------------

		inline Matrix44& Matrix44::operator+=(const Matrix44& other)
		{
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] += other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix44& Matrix44::operator-=(const Matrix44& other)
		{
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] -= other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix44& Matrix44::operator*=(const Matrix44& other)
		{
			*this = *this * other;
			return *this;
		}

		inline Matrix44& Matrix44::operator*=(float scalar)
		{
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					m[i][j] *= scalar;
				}
			}
			return *this;
		}

		inline Matrix44 Matrix44::operator-() const
		{
			Matrix44 result;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					result.m[i][j] = -m[i][j];
				}
			}
			return result;
		}

		inline Matrix44 Matrix44::operator+(const Matrix44& other) const
		{
			Matrix44 result = *this;
			result += other;
			return result;
		}

		inline Matrix44 Matrix44::operator-(const Matrix44& other) const
		{
			Matrix44 result = *this;
			result -= other;
			return result;
		}

		inline Matrix44 Matrix44::operator*(const Matrix44& other) const
		{
			Matrix44 result;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					result.m[i][j] = 0.0f;
					for (int k = 0; k < 4; ++k)
					{
						result.m[i][j] += m[i][k] * other.m[k][j];
					}
				}
			}
			return result;
		}

		inline Matrix44 Matrix44::operator*(float scalar) const
		{
			Matrix44 result = *this;
			result *= scalar;
			return result;
		}

		inline bool Matrix44::operator==(const Matrix44& other) const
		{
			for (int i = 0; i < 4; ++i)
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

		inline bool Matrix44::operator!=(const Matrix44& other) const
		{
			return !(*this == other);
		}

		//-----------------------------------------------------------------------------
		// Matrix properties
		//-----------------------------------------------------------------------------

		inline void Matrix44::SetIdentity()
		{
			m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
			m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
			m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
			m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
		}

		inline Matrix44 Matrix44::Transpose() const
		{
			return Matrix44(
				m[0][0], m[1][0], m[2][0], m[3][0],
				m[0][1], m[1][1], m[2][1], m[3][1],
				m[0][2], m[1][2], m[2][2], m[3][2],
				m[0][3], m[1][3], m[2][3], m[3][3]
			);
		}

		inline float Matrix44::Determinant() const
		{
			// 4x4 determinant via cofactor expansion along first row
			float det = 0.0f;

			// Cofactor for m[0][0]
			det += m[0][0] * (
				m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
				m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])
			);

			// Cofactor for m[0][1]
			det -= m[0][1] * (
				m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
				m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])
			);

			// Cofactor for m[0][2]
			det += m[0][2] * (
				m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
				m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
				m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			// Cofactor for m[0][3]
			det -= m[0][3] * (
				m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
				m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
				m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0])
			);

			return det;
		}

		//-----------------------------------------------------------------------------
		// Transform vectors (inline eligible)
		//-----------------------------------------------------------------------------

		inline Vector4D Matrix44::TransformVector4(const Vector4D& v) const
		{
			return Vector4D(
				m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
				m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
				m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
				m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w
			);
		}
	}
}
