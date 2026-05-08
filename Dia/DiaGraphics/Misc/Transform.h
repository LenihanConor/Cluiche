////////////////////////////////////////////////////////////////////////////////
// Filename: Transform.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Define a 3x3 transform matrix for 2D transformations
		///
		/// A Transform specifies how to translate, rotate, scale,
		/// shear, project, whatever things. In mathematical terms, it defines
		/// how to transform a point from one coordinate system to another.
		///
		/// For example, if you apply a rotation transform to a sprite, the
		/// result will be a rotated sprite. And anything that is transformed
		/// by this rotation transform will be rotated the same way, according
		/// to its initial position.
		///
		/// Transforms are typically used for drawing. SFML provides a simple
		/// interface for applying transforms to drawable objects through the
		/// RenderStates structure.
		///
		/// \note Transform uses a 3x3 matrix internally to represent the
		/// transformation in homogeneous coordinates. This allows combining
		/// multiple transforms (translation, rotation, scale) into a single
		/// matrix multiplication.
		////////////////////////////////////////////////////////////
		class Transform
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Default constructor
			///
			/// Creates an identity transform (no transformation).
			////////////////////////////////////////////////////////////
			Transform();

			////////////////////////////////////////////////////////////
			/// \brief Construct a transform from a 3x3 matrix
			///
			/// \param a00 Element (0, 0) of the matrix
			/// \param a01 Element (0, 1) of the matrix
			/// \param a02 Element (0, 2) of the matrix
			/// \param a10 Element (1, 0) of the matrix
			/// \param a11 Element (1, 1) of the matrix
			/// \param a12 Element (1, 2) of the matrix
			/// \param a20 Element (2, 0) of the matrix
			/// \param a21 Element (2, 1) of the matrix
			/// \param a22 Element (2, 2) of the matrix
			////////////////////////////////////////////////////////////
			Transform(float a00, float a01, float a02,
					  float a10, float a11, float a12,
					  float a20, float a21, float a22);

			////////////////////////////////////////////////////////////
			/// \brief Return the transform as a 4x4 matrix
			///
			/// This function returns a pointer to an array of 16 floats
			/// containing the transform elements as a 4x4 matrix, which
			/// is directly compatible with OpenGL functions.
			///
			/// \code
			/// Transform transform = ...;
			/// glLoadMatrixf(transform.GetMatrix());
			/// \endcode
			///
			/// \return Pointer to a 16 floats array
			////////////////////////////////////////////////////////////
			const float* GetMatrix() const;

			////////////////////////////////////////////////////////////
			/// \brief Return the inverse of the transform
			///
			/// If the inverse cannot be computed, an identity transform
			/// is returned.
			///
			/// \return A new transform which is the inverse of self
			////////////////////////////////////////////////////////////
			Transform GetInverse() const;

			////////////////////////////////////////////////////////////
			/// \brief Transform a 2D point
			///
			/// \param x X coordinate of the point to transform
			/// \param y Y coordinate of the point to transform
			///
			/// \return Transformed point
			////////////////////////////////////////////////////////////
			Maths::Vector2D TransformPoint(float x, float y) const;

			////////////////////////////////////////////////////////////
			/// \brief Transform a 2D point
			///
			/// \param point Point to transform
			///
			/// \return Transformed point
			////////////////////////////////////////////////////////////
			Maths::Vector2D TransformPoint(const Maths::Vector2D& point) const;

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with another one
			///
			/// The result is a transform that is equivalent to applying
			/// *this followed by \a transform. Mathematically, it is
			/// equivalent to a matrix multiplication.
			///
			/// \param transform Transform to combine with this transform
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Combine(const Transform& transform);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a translation
			///
			/// \param x Offset to apply on X axis
			/// \param y Offset to apply on Y axis
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Translate(float x, float y);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a translation
			///
			/// \param offset Translation offset to apply
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Translate(const Maths::Vector2D& offset);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a rotation
			///
			/// \param angle Rotation angle, in degrees
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Rotate(float angle);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a rotation
			///
			/// The center of rotation is provided for convenience as a second
			/// argument, so that you can build rotations around arbitrary points
			/// more easily (and efficiently) than the usual
			/// translate(-center).rotate(angle).translate(center).
			///
			/// \param angle  Rotation angle, in degrees
			/// \param centerX X coordinate of the center of rotation
			/// \param centerY Y coordinate of the center of rotation
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Rotate(float angle, float centerX, float centerY);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a rotation
			///
			/// The center of rotation is provided for convenience as a second
			/// argument, so that you can build rotations around arbitrary points
			/// more easily (and efficiently) than the usual
			/// translate(-center).rotate(angle).translate(center).
			///
			/// \param angle  Rotation angle, in degrees
			/// \param center Center of rotation
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Rotate(float angle, const Maths::Vector2D& center);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a scaling
			///
			/// \param scaleX Scaling factor on the X axis
			/// \param scaleY Scaling factor on the Y axis
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Scale(float scaleX, float scaleY);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a scaling
			///
			/// The center of scaling is provided for convenience as a second
			/// argument, so that you can build scaling around arbitrary points
			/// more easily (and efficiently) than the usual
			/// translate(-center).scale(factors).translate(center).
			///
			/// \param scaleX  Scaling factor on X axis
			/// \param scaleY  Scaling factor on Y axis
			/// \param centerX X coordinate of the center of scaling
			/// \param centerY Y coordinate of the center of scaling
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Scale(float scaleX, float scaleY, float centerX, float centerY);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a scaling
			///
			/// \param factors Scaling factors
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Scale(const Maths::Vector2D& factors);

			////////////////////////////////////////////////////////////
			/// \brief Combine the current transform with a scaling
			///
			/// The center of scaling is provided for convenience as a second
			/// argument, so that you can build scaling around arbitrary points
			/// more easily (and efficiently) than the usual
			/// translate(-center).scale(factors).translate(center).
			///
			/// \param factors Scaling factors
			/// \param center  Center of scaling
			///
			/// \return Reference to *this
			////////////////////////////////////////////////////////////
			Transform& Scale(const Maths::Vector2D& factors, const Maths::Vector2D& center);

			////////////////////////////////////////////////////////////
			// Static member data
			////////////////////////////////////////////////////////////
			static const Transform Identity; ///< The identity transform (does nothing)

		private:
			////////////////////////////////////////////////////////////
			// Member data
			////////////////////////////////////////////////////////////
			float mMatrix[16]; ///< 4x4 matrix defining the transformation (stored in column-major order for OpenGL)
		};

		////////////////////////////////////////////////////////////
		/// \relates Transform
		/// \brief Overload of binary operator * to combine two transforms
		///
		/// This call is equivalent to calling Transform(left).combine(right).
		///
		/// \param left  Left operand (the first transform)
		/// \param right Right operand (the second transform)
		///
		/// \return New combined transform
		////////////////////////////////////////////////////////////
		Transform operator *(const Transform& left, const Transform& right);

		////////////////////////////////////////////////////////////
		/// \relates Transform
		/// \brief Overload of binary operator *= to combine two transforms
		///
		/// This call is equivalent to calling left.combine(right).
		///
		/// \param left  Left operand (the first transform)
		/// \param right Right operand (the second transform)
		///
		/// \return The combined transform
		////////////////////////////////////////////////////////////
		Transform& operator *=(Transform& left, const Transform& right);

		////////////////////////////////////////////////////////////
		/// \relates Transform
		/// \brief Overload of binary operator * to transform a point
		///
		/// This call is equivalent to calling left.transformPoint(right).
		///
		/// \param left  Left operand (the transform)
		/// \param right Right operand (the point to transform)
		///
		/// \return New transformed point
		////////////////////////////////////////////////////////////
		Maths::Vector2D operator *(const Transform& left, const Maths::Vector2D& right);
	}
}
