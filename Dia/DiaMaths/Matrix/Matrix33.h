#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Maths
	{
		class Angle;
		class Matrix22;

		//==============================================================================
		// CLASS Matrix33
		//==============================================================================
		// 3x3 matrix for 2D transformations (rotation, scale, translation)
		//
		// WHY 3x3 FOR 2D?
		//   2D vectors are (x, y), but we use homogeneous coordinates (x, y, 1)
		//   This allows translation to be represented as a matrix operation!
		//   Without this, translation would need special handling (matrix + vector)
		//
		// MATRIX LAYOUT (column-major interpretation):
		// | m00  m01  m02 |   | scaleX*cos  -scaleY*sin  translateX |
		// | m10  m11  m12 | = | scaleX*sin   scaleY*cos  translateY |
		// | m20  m21  m22 |   |     0            0            1      |
		//
		// TRANSFORMATION ORDER (TRS):
		//   Combined matrix = Translation * Rotation * Scale
		//   Applied to point: result = matrix * point
		//   This gives: point → scaled → rotated → translated
		//
		// COMMON USES:
		//   - Transforming game objects (position, rotation, size)
		//   - Camera transformations (world to screen)
		//   - UI layout (scaling, positioning)
		//   - Combining multiple transformations efficiently
		//
		// PERFORMANCE:
		//   Multiplying matrices is faster than applying transformations individually
		//   when transforming many points by the same transformation.
		//==============================================================================
		class Matrix33
		{
		public:
			DIA_TYPE_DECLARATION;

			// Construction
			Matrix33();
			Matrix33(const Matrix33& other);
			Matrix33(float m00, float m01, float m02,
			         float m10, float m11, float m12,
			         float m20, float m21, float m22);

			// Factory methods
			static Matrix33 Identity();
			static Matrix33 FromTranslation(const Vector2D& translation);
			static Matrix33 FromRotation(const Angle& angle);
			static Matrix33 FromScale(const Vector2D& scale);
			static Matrix33 FromScale(float uniformScale);
			static Matrix33 FromTRS(const Vector2D& translation, const Angle& rotation, const Vector2D& scale);

			// Assignment
			Matrix33& operator=(const Matrix33& other);

			// Matrix operations
			Matrix33& operator+=(const Matrix33& other);
			Matrix33& operator-=(const Matrix33& other);
			Matrix33& operator*=(const Matrix33& other);
			Matrix33& operator*=(float scalar);

			Matrix33 operator-() const;
			Matrix33 operator+(const Matrix33& other) const;
			Matrix33 operator-(const Matrix33& other) const;
			Matrix33 operator*(const Matrix33& other) const;
			Matrix33 operator*(float scalar) const;

			bool operator==(const Matrix33& other) const;
			bool operator!=(const Matrix33& other) const;

			// Element access
			float& operator()(int row, int col);
			float operator()(int row, int col) const;

			// Transform vectors
			Vector2D TransformPoint(const Vector2D& point) const;
			Vector2D TransformDirection(const Vector2D& direction) const;

			// Matrix properties
			void SetIdentity();
			Matrix33 Transpose() const;
			float Determinant() const;
			Matrix33 Inverse() const;

			// Component extraction
			Vector2D GetTranslation() const;
			Angle GetRotation() const;
			Vector2D GetScale() const;

			// Direct access to elements
			float m[3][3];
		};
	}
}

#include "DiaMaths/Matrix/Matrix33.inl"
