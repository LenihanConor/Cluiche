#pragma once

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Maths
	{
		class Vector3D;
		class Quaternion;
		class Matrix44;

		//==============================================================================
		// CLASS Matrix34
		//==============================================================================
		// 3x4 affine matrix — rotation + translation, no projection row.
		//
		// STORAGE: Row-major float m[3][4] — matches Matrix22/Matrix33/Matrix44.
		//   The implicit 4th row is (0, 0, 0, 1).
		//
		// USE: Cheaper than Matrix44 for transform hierarchies. Skeletal poses,
		// scene-graph world matrices, and anywhere "no projection ever" is a
		// load-bearing invariant.
		//==============================================================================
		class Matrix34
		{
		public:
			DIA_TYPE_DECLARATION;

			// Construction
			Matrix34();                                                          // identity
			Matrix34(const Matrix34& other);
			Matrix34(float m00, float m01, float m02, float m03,
			         float m10, float m11, float m12, float m13,
			         float m20, float m21, float m22, float m23);

			// Factories
			static Matrix34 Identity();
			static Matrix34 FromTranslation(const Vector3D& translation);
			static Matrix34 FromRotation(const Quaternion& rotation);
			static Matrix34 FromScale(const Vector3D& scale);
			static Matrix34 FromScale(float uniformScale);
			static Matrix34 FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale);
			static Matrix34 FromMatrix44(const Matrix44& transform);             // asserts row 3 == (0,0,0,1)

			// Promotion
			Matrix44 ToMatrix44() const;                                          // appends (0,0,0,1) row

			// Assignment
			Matrix34& operator=(const Matrix34& other);

			// Arithmetic
			Matrix34& operator+=(const Matrix34& other);
			Matrix34& operator-=(const Matrix34& other);
			Matrix34& operator*=(const Matrix34& other);
			Matrix34& operator*=(float scalar);

			Matrix34 operator-() const;
			Matrix34 operator+(const Matrix34& other) const;
			Matrix34 operator-(const Matrix34& other) const;
			Matrix34 operator*(const Matrix34& other) const;                     // implicit (0,0,0,1) bottom row
			Matrix34 operator*(float scalar) const;

			bool operator==(const Matrix34& other) const;
			bool operator!=(const Matrix34& other) const;

			// Element access — m(row, col)
			float& operator()(int row, int col);
			float  operator()(int row, int col) const;

			// Transform
			Vector3D TransformPoint(const Vector3D& point) const;
			Vector3D TransformDirection(const Vector3D& dir) const;

			// Properties
			void     SetIdentity();
			Matrix34 Inverse() const;                                             // affine-specific (faster than 4x4)
			float    Determinant() const;                                         // determinant of upper-left 3x3

			// Component extraction
			Vector3D    GetTranslation() const;
			Quaternion  GetRotation() const;
			Vector3D    GetScale() const;

			// Direct access — row-major m[row][col]
			float m[3][4];
		};

	}  // namespace Maths
}  // namespace Dia

#include "DiaMaths/Matrix/Matrix34.inl"
