#pragma once

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Vector/Vector4D.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Maths
	{
		class Quaternion;
		class Angle;

		//==============================================================================
		// CLASS Matrix44
		//==============================================================================
		// 4x4 matrix for 3D transformations (rotation, scale, translation, projection)
		//
		// STORAGE: Row-major float m[4][4] — matches Matrix22 and Matrix33.
		//   m[row][col] gives element at (row, col).
		//   To upload to OpenGL/glTF use GetColumnMajor(float[16]) which transposes once.
		//
		// HANDEDNESS: Right-handed (per DiaMaths SD-007).
		// PROJECTION DEPTH: OpenGL [-1, 1] (per diamaths.md AI Review Q10).
		//
		// TRANSFORMATION ORDER (TRS):
		//   Combined matrix = Translation * Rotation * Scale
		//   Applied to point: result = matrix * point
		//   This gives: point → scaled → rotated → translated
		//==============================================================================
		class Matrix44
		{
		public:
			DIA_TYPE_DECLARATION;

			// Construction
			Matrix44();
			Matrix44(const Matrix44& other);
			Matrix44(float m00, float m01, float m02, float m03,
			         float m10, float m11, float m12, float m13,
			         float m20, float m21, float m22, float m23,
			         float m30, float m31, float m32, float m33);

			// Factory methods
			static Matrix44 Identity();
			static Matrix44 FromTranslation(const Vector3D& translation);
			static Matrix44 FromRotation(const Quaternion& rotation);
			static Matrix44 FromScale(const Vector3D& scale);
			static Matrix44 FromScale(float uniformScale);
			static Matrix44 FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale);

			// Y-up right-handed projection / view builders. fovY uses Angle for type safety.
			static Matrix44 Perspective(const Angle& fovY, float aspect, float nearZ, float farZ);
			static Matrix44 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
			static Matrix44 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

			// Assignment
			Matrix44& operator=(const Matrix44& other);

			// Matrix operations
			Matrix44& operator+=(const Matrix44& other);
			Matrix44& operator-=(const Matrix44& other);
			Matrix44& operator*=(const Matrix44& other);
			Matrix44& operator*=(float scalar);

			Matrix44 operator-() const;
			Matrix44 operator+(const Matrix44& other) const;
			Matrix44 operator-(const Matrix44& other) const;
			Matrix44 operator*(const Matrix44& other) const;
			Matrix44 operator*(float scalar) const;

			bool operator==(const Matrix44& other) const;
			bool operator!=(const Matrix44& other) const;

			// Element access
			float& operator()(int row, int col);
			float operator()(int row, int col) const;

			// Transform vectors
			Vector3D TransformPoint(const Vector3D& point) const;
			Vector3D TransformDirection(const Vector3D& direction) const;
			Vector4D TransformVector4(const Vector4D& v) const;

			// Matrix properties
			void SetIdentity();
			Matrix44 Transpose() const;
			float Determinant() const;
			Matrix44 Inverse() const;

			// OpenGL / glTF interop — fills 16-float column-major buffer.
			void GetColumnMajor(float outData[16]) const;

			// Component extraction (assumes affine input — undefined for projection matrices)
			Vector3D GetTranslation() const;
			Quaternion GetRotation() const;
			Vector3D GetScale() const;

			// Direct access to elements
			float m[4][4];
		};
	}
}

#include "DiaMaths/Matrix/Matrix44.inl"
