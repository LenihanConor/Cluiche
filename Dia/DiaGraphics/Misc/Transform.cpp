////////////////////////////////////////////////////////////////////////////////
// Filename: Transform.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Misc/Transform.h"

#include <cmath>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		const Transform Transform::Identity;

		////////////////////////////////////////////////////////////
		Transform::Transform()
		{
			// Identity matrix
			mMatrix[0] = 1.f; mMatrix[4] = 0.f; mMatrix[8]  = 0.f; mMatrix[12] = 0.f;
			mMatrix[1] = 0.f; mMatrix[5] = 1.f; mMatrix[9]  = 0.f; mMatrix[13] = 0.f;
			mMatrix[2] = 0.f; mMatrix[6] = 0.f; mMatrix[10] = 1.f; mMatrix[14] = 0.f;
			mMatrix[3] = 0.f; mMatrix[7] = 0.f; mMatrix[11] = 0.f; mMatrix[15] = 1.f;
		}

		////////////////////////////////////////////////////////////
		Transform::Transform(float a00, float a01, float a02,
							 float a10, float a11, float a12,
							 float a20, float a21, float a22)
		{
			mMatrix[0] = a00; mMatrix[4] = a01; mMatrix[8]  = 0.f; mMatrix[12] = a02;
			mMatrix[1] = a10; mMatrix[5] = a11; mMatrix[9]  = 0.f; mMatrix[13] = a12;
			mMatrix[2] = 0.f; mMatrix[6] = 0.f; mMatrix[10] = 1.f; mMatrix[14] = 0.f;
			mMatrix[3] = a20; mMatrix[7] = a21; mMatrix[11] = 0.f; mMatrix[15] = a22;
		}

		////////////////////////////////////////////////////////////
		const float* Transform::GetMatrix() const
		{
			return mMatrix;
		}

		////////////////////////////////////////////////////////////
		Transform Transform::GetInverse() const
		{
			// Compute the determinant
			float det = mMatrix[0] * (mMatrix[15] * mMatrix[5] - mMatrix[7] * mMatrix[13]) -
						mMatrix[1] * (mMatrix[15] * mMatrix[4] - mMatrix[7] * mMatrix[12]) +
						mMatrix[3] * (mMatrix[13] * mMatrix[4] - mMatrix[5] * mMatrix[12]);

			// Compute the inverse if the determinant is not zero
			// (don't use an epsilon because the determinant may *really* be tiny)
			if (det != 0.f)
			{
				return Transform((mMatrix[15] * mMatrix[5] - mMatrix[7] * mMatrix[13]) / det,
								 -(mMatrix[15] * mMatrix[4] - mMatrix[7] * mMatrix[12]) / det,
								 (mMatrix[13] * mMatrix[4] - mMatrix[5] * mMatrix[12]) / det,
								 -(mMatrix[15] * mMatrix[1] - mMatrix[3] * mMatrix[13]) / det,
								 (mMatrix[15] * mMatrix[0] - mMatrix[3] * mMatrix[12]) / det,
								 -(mMatrix[13] * mMatrix[0] - mMatrix[1] * mMatrix[12]) / det,
								 (mMatrix[7] * mMatrix[1] - mMatrix[3] * mMatrix[5]) / det,
								 -(mMatrix[7] * mMatrix[0] - mMatrix[3] * mMatrix[4]) / det,
								 (mMatrix[5] * mMatrix[0] - mMatrix[1] * mMatrix[4]) / det);
			}
			else
			{
				return Identity;
			}
		}

		////////////////////////////////////////////////////////////
		Maths::Vector2D Transform::TransformPoint(float x, float y) const
		{
			return Maths::Vector2D(mMatrix[0] * x + mMatrix[4] * y + mMatrix[12],
								   mMatrix[1] * x + mMatrix[5] * y + mMatrix[13]);
		}

		////////////////////////////////////////////////////////////
		Maths::Vector2D Transform::TransformPoint(const Maths::Vector2D& point) const
		{
			return TransformPoint(point.x, point.y);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Combine(const Transform& transform)
		{
			const float* a = mMatrix;
			const float* b = transform.mMatrix;

			*this = Transform(a[0] * b[0] + a[4] * b[1] + a[12] * b[3],
							  a[0] * b[4] + a[4] * b[5] + a[12] * b[7],
							  a[0] * b[12] + a[4] * b[13] + a[12] * b[15],
							  a[1] * b[0] + a[5] * b[1] + a[13] * b[3],
							  a[1] * b[4] + a[5] * b[5] + a[13] * b[7],
							  a[1] * b[12] + a[5] * b[13] + a[13] * b[15],
							  a[3] * b[0] + a[7] * b[1] + a[15] * b[3],
							  a[3] * b[4] + a[7] * b[5] + a[15] * b[7],
							  a[3] * b[12] + a[7] * b[13] + a[15] * b[15]);

			return *this;
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Translate(float x, float y)
		{
			Transform translation(1, 0, x,
								  0, 1, y,
								  0, 0, 1);

			return Combine(translation);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Translate(const Maths::Vector2D& offset)
		{
			return Translate(offset.x, offset.y);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Rotate(float angle)
		{
			float rad = angle * 3.141592654f / 180.f;
			float cos = std::cos(rad);
			float sin = std::sin(rad);

			Transform rotation(cos, -sin, 0,
							   sin, cos, 0,
							   0, 0, 1);

			return Combine(rotation);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Rotate(float angle, float centerX, float centerY)
		{
			float rad = angle * 3.141592654f / 180.f;
			float cos = std::cos(rad);
			float sin = std::sin(rad);

			Transform rotation(cos, -sin, centerX * (1 - cos) + centerY * sin,
							   sin, cos, centerY * (1 - cos) - centerX * sin,
							   0, 0, 1);

			return Combine(rotation);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Rotate(float angle, const Maths::Vector2D& center)
		{
			return Rotate(angle, center.x, center.y);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Scale(float scaleX, float scaleY)
		{
			Transform scaling(scaleX, 0, 0,
							  0, scaleY, 0,
							  0, 0, 1);

			return Combine(scaling);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Scale(float scaleX, float scaleY, float centerX, float centerY)
		{
			Transform scaling(scaleX, 0, centerX * (1 - scaleX),
							  0, scaleY, centerY * (1 - scaleY),
							  0, 0, 1);

			return Combine(scaling);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Scale(const Maths::Vector2D& factors)
		{
			return Scale(factors.x, factors.y);
		}

		////////////////////////////////////////////////////////////
		Transform& Transform::Scale(const Maths::Vector2D& factors, const Maths::Vector2D& center)
		{
			return Scale(factors.x, factors.y, center.x, center.y);
		}

		////////////////////////////////////////////////////////////
		Transform operator *(const Transform& left, const Transform& right)
		{
			return Transform(left).Combine(right);
		}

		////////////////////////////////////////////////////////////
		Transform& operator *=(Transform& left, const Transform& right)
		{
			return left.Combine(right);
		}

		////////////////////////////////////////////////////////////
		Maths::Vector2D operator *(const Transform& left, const Maths::Vector2D& right)
		{
			return left.TransformPoint(right);
		}
	}
}
