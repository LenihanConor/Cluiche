#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Matrix33::Matrix33()
		{
			SetIdentity();
		}

		inline Matrix33::Matrix33(const Matrix33& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					m[i][j] = other.m[i][j];
				}
			}
		}

		inline Matrix33::Matrix33(float m00, float m01, float m02,
		                           float m10, float m11, float m12,
		                           float m20, float m21, float m22)
		{
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
		}

		//-----------------------------------------------------------------------------
		// Factory methods
		//-----------------------------------------------------------------------------

		inline Matrix33 Matrix33::Identity()
		{
			return Matrix33(
				1.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 1.0f
			);
		}

		inline Matrix33 Matrix33::FromTranslation(const Vector2D& translation)
		{
			return Matrix33(
				1.0f, 0.0f, translation.x,
				0.0f, 1.0f, translation.y,
				0.0f, 0.0f, 1.0f
			);
		}

		inline Matrix33 Matrix33::FromRotation(const Angle& angle)
		{
			float rad = angle.AsRadians();
			float c = Dia::Maths::Cos(rad);
			float s = Dia::Maths::Sin(rad);

			return Matrix33(
				 c, -s, 0.0f,
				 s,  c, 0.0f,
				0.0f, 0.0f, 1.0f
			);
		}

		inline Matrix33 Matrix33::FromScale(const Vector2D& scale)
		{
			return Matrix33(
				scale.x, 0.0f, 0.0f,
				0.0f, scale.y, 0.0f,
				0.0f, 0.0f, 1.0f
			);
		}

		inline Matrix33 Matrix33::FromScale(float uniformScale)
		{
			return FromScale(Vector2D(uniformScale, uniformScale));
		}

		inline Matrix33 Matrix33::FromTRS(const Vector2D& translation, const Angle& rotation, const Vector2D& scale)
		{
			float rad = rotation.AsRadians();
			float c = Dia::Maths::Cos(rad);
			float s = Dia::Maths::Sin(rad);

			return Matrix33(
				scale.x * c, -scale.y * s, translation.x,
				scale.x * s,  scale.y * c, translation.y,
				0.0f, 0.0f, 1.0f
			);
		}

		//-----------------------------------------------------------------------------
		// Assignment
		//-----------------------------------------------------------------------------

		inline Matrix33& Matrix33::operator=(const Matrix33& other)
		{
			if (this != &other)
			{
				for (int i = 0; i < 3; ++i)
				{
					for (int j = 0; j < 3; ++j)
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

		inline float& Matrix33::operator()(int row, int col)
		{
			return m[row][col];
		}

		inline float Matrix33::operator()(int row, int col) const
		{
			return m[row][col];
		}

		//-----------------------------------------------------------------------------
		// Matrix operations
		//-----------------------------------------------------------------------------

		inline Matrix33& Matrix33::operator+=(const Matrix33& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					m[i][j] += other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix33& Matrix33::operator-=(const Matrix33& other)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					m[i][j] -= other.m[i][j];
				}
			}
			return *this;
		}

		inline Matrix33& Matrix33::operator*=(const Matrix33& other)
		{
			*this = *this * other;
			return *this;
		}

		inline Matrix33& Matrix33::operator*=(float scalar)
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					m[i][j] *= scalar;
				}
			}
			return *this;
		}

		inline Matrix33 Matrix33::operator-() const
		{
			Matrix33 result;
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					result.m[i][j] = -m[i][j];
				}
			}
			return result;
		}

		inline Matrix33 Matrix33::operator+(const Matrix33& other) const
		{
			Matrix33 result = *this;
			result += other;
			return result;
		}

		inline Matrix33 Matrix33::operator-(const Matrix33& other) const
		{
			Matrix33 result = *this;
			result -= other;
			return result;
		}

		inline Matrix33 Matrix33::operator*(const Matrix33& other) const
		{
			Matrix33 result;
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					result.m[i][j] = 0.0f;
					for (int k = 0; k < 3; ++k)
					{
						result.m[i][j] += m[i][k] * other.m[k][j];
					}
				}
			}
			return result;
		}

		inline Matrix33 Matrix33::operator*(float scalar) const
		{
			Matrix33 result = *this;
			result *= scalar;
			return result;
		}

		inline bool Matrix33::operator==(const Matrix33& other) const
		{
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					if (m[i][j] != other.m[i][j])
					{
						return false;
					}
				}
			}
			return true;
		}

		inline bool Matrix33::operator!=(const Matrix33& other) const
		{
			return !(*this == other);
		}

		//-----------------------------------------------------------------------------
		// Transform vectors
		//-----------------------------------------------------------------------------

		inline Vector2D Matrix33::TransformPoint(const Vector2D& point) const
		{
			// Treat point as (x, y, 1) for homogeneous coordinates
			return Vector2D(
				m[0][0] * point.x + m[0][1] * point.y + m[0][2],
				m[1][0] * point.x + m[1][1] * point.y + m[1][2]
			);
		}

		inline Vector2D Matrix33::TransformDirection(const Vector2D& direction) const
		{
			// Treat direction as (x, y, 0) - no translation
			return Vector2D(
				m[0][0] * direction.x + m[0][1] * direction.y,
				m[1][0] * direction.x + m[1][1] * direction.y
			);
		}

		//-----------------------------------------------------------------------------
		// Matrix properties
		//-----------------------------------------------------------------------------

		inline void Matrix33::SetIdentity()
		{
			m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;
			m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
			m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f;
		}

		inline Matrix33 Matrix33::Transpose() const
		{
			return Matrix33(
				m[0][0], m[1][0], m[2][0],
				m[0][1], m[1][1], m[2][1],
				m[0][2], m[1][2], m[2][2]
			);
		}

		inline float Matrix33::Determinant() const
		{
			return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
			     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
			     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		}

		inline Vector2D Matrix33::GetTranslation() const
		{
			return Vector2D(m[0][2], m[1][2]);
		}
	}
}
