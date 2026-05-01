#pragma once

#ifndef DIA_GEOMETRY2D_TRANSFORM_H
#define DIA_GEOMETRY2D_TRANSFORM_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia
{
	namespace Maths
	{
		class Matrix33;
	}

	namespace Geometry2D
	{
		//==============================================================================
		// CLASS Transform
		//==============================================================================
		// Represents a 2D transformation with position, rotation, and scale.
		// Supports parent-child hierarchy for scene graphs.
		//
		// CONCEPTS:
		//   Local Space  - Position/rotation/scale relative to parent transform
		//   World Space  - Position/rotation/scale in absolute game world coordinates
		//
		// HIERARCHY:
		//   A transform can have a parent. Its world transform is computed by
		//   combining its local transform with its parent's world transform.
		//
		// USAGE:
		//   Transform player;
		//   Transform sword;
		//   sword.SetParent(&player);
		//   sword.SetLocalPosition(Vector2D(0.5f, 0.0f));
		//   Vector2D swordWorldPos = sword.GetWorldPosition();
		//==============================================================================
		class Transform
		{
		public:
			// Default constructor - identity transform at origin
			Transform();

			// Construct with initial values
			Transform(const Dia::Maths::Vector2D& position, const Dia::Maths::Angle& rotation, const Dia::Maths::Vector2D& scale);

			// Copy constructor
			Transform(const Transform& other);

			// Assignment operator
			Transform& operator=(const Transform& other);

			//-----------------------------------------------------------------------------
			// Local Space Properties (relative to parent)
			//-----------------------------------------------------------------------------

			const Dia::Maths::Vector2D&		GetLocalPosition() const;
			const Dia::Maths::Angle&		GetLocalRotation() const;
			const Dia::Maths::Vector2D&		GetLocalScale() const;

			void SetLocalPosition(const Dia::Maths::Vector2D& position);
			void SetLocalRotation(const Dia::Maths::Angle& rotation);
			void SetLocalScale(const Dia::Maths::Vector2D& scale);
			void SetLocalScale(float uniformScale);

			//-----------------------------------------------------------------------------
			// World Space Properties (absolute game world coordinates)
			//-----------------------------------------------------------------------------

			Dia::Maths::Vector2D	GetWorldPosition() const;
			Dia::Maths::Angle		GetWorldRotation() const;
			Dia::Maths::Vector2D	GetWorldScale() const;

			void SetWorldPosition(const Dia::Maths::Vector2D& position);
			void SetWorldRotation(const Dia::Maths::Angle& rotation);
			void SetWorldScale(const Dia::Maths::Vector2D& scale);

			// Get all world properties at once (optimized: traverses parent hierarchy only once)
			void GetWorldTransform(Dia::Maths::Vector2D& outPosition, Dia::Maths::Angle& outRotation, Dia::Maths::Vector2D& outScale) const;

			//-----------------------------------------------------------------------------
			// Hierarchy
			//
			// WARNING: Raw pointer — caller must ensure parent outlives child, or call
			//          SetParent(nullptr) before parent is destroyed.
			// WARNING: Do NOT create circular parent chains — will cause stack overflow!
			//-----------------------------------------------------------------------------

			Transform*	GetParent() const;
			void		SetParent(Transform* parent);
			bool		HasParent() const;

			//-----------------------------------------------------------------------------
			// Transformations
			//-----------------------------------------------------------------------------

			void Translate(const Dia::Maths::Vector2D& delta);
			void TranslateWorld(const Dia::Maths::Vector2D& delta);
			void Rotate(const Dia::Maths::Angle& delta);
			void Scale(const Dia::Maths::Vector2D& scale);
			void Scale(float uniformScale);

			//-----------------------------------------------------------------------------
			// Space Conversions
			//-----------------------------------------------------------------------------

			// Transform point from local to world space (Scale -> Rotate -> Translate)
			Dia::Maths::Vector2D TransformPoint(const Dia::Maths::Vector2D& localPoint) const;

			// Transform direction from local to world space (ignores translation)
			Dia::Maths::Vector2D TransformDirection(const Dia::Maths::Vector2D& localDirection) const;

			// Transform point from world to local space (inverse of TransformPoint)
			Dia::Maths::Vector2D InverseTransformPoint(const Dia::Maths::Vector2D& worldPoint) const;

			// Transform direction from world to local space (ignores translation)
			Dia::Maths::Vector2D InverseTransformDirection(const Dia::Maths::Vector2D& worldDirection) const;

			//-----------------------------------------------------------------------------
			// Matrix generation
			//-----------------------------------------------------------------------------

			Dia::Maths::Matrix33 GetLocalMatrix() const;
			Dia::Maths::Matrix33 GetWorldMatrix() const;

			//-----------------------------------------------------------------------------
			// Utility
			//-----------------------------------------------------------------------------

			// Get forward direction in world space (rotated +X axis)
			Dia::Maths::Vector2D GetForward() const;

			// Get right direction in world space (rotated +Y axis)
			Dia::Maths::Vector2D GetRight() const;

			// Rotate to look at target position
			void LookAt(const Dia::Maths::Vector2D& target);

		private:
			Dia::Maths::Vector2D	mLocalPosition;
			Dia::Maths::Angle		mLocalRotation;
			Dia::Maths::Vector2D	mLocalScale;

			Transform*	mParent;

			// Helper to compute world transformation when parent exists
			void GetParentWorldTransform(Dia::Maths::Vector2D& outPosition, Dia::Maths::Angle& outRotation, Dia::Maths::Vector2D& outScale) const;
		};
	}
}

#include "DiaGeometry2D/Transform/Transform.inl"

#endif // DIA_GEOMETRY2D_TRANSFORM_H
