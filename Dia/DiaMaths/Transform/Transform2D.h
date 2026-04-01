#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia
{
	namespace Maths
	{
		class Matrix33;

		//==============================================================================
		// CLASS Transform2D
		//==============================================================================
		// Represents a 2D transformation with position, rotation, and scale
		// Supports parent-child hierarchy for scene graphs
		//
		// CONCEPTS:
		//   Local Space  - Position/rotation/scale relative to parent transform
		//   World Space  - Position/rotation/scale in absolute game world coordinates
		//
		// HIERARCHY:
		//   A transform can have a parent. Its world transform is computed by
		//   combining its local transform with its parent's world transform.
		//   Example: Player holding a sword
		//     - Player transform: position, rotation, scale
		//     - Sword local transform: offset from player's hand
		//     - Sword world transform: automatically calculated from both
		//
		// USAGE:
		//   Transform2D player;
		//   Transform2D sword;
		//   sword.SetParent(&player);
		//   sword.SetLocalPosition(Vector2D(0.5f, 0.0f)); // Sword offset from player
		//   Vector2D swordWorldPos = sword.GetWorldPosition(); // Actual game position
		//==============================================================================
		class Transform2D
		{
		public:
			// Default constructor - identity transform at origin
			Transform2D();

			// Construct with initial values
			Transform2D(const Vector2D& position, const Angle& rotation, const Vector2D& scale);

			// Copy constructor
			Transform2D(const Transform2D& other);

			// Assignment operator
			Transform2D& operator=(const Transform2D& other);

			//-----------------------------------------------------------------------------
			// Local Space Properties (relative to parent)
			//
			// Local space is used to position objects relative to their parent.
			// Example: A turret on a tank uses local space - when tank moves/rotates,
			//          turret moves with it while maintaining its relative position.
			//-----------------------------------------------------------------------------

			const Vector2D&		GetLocalPosition() const;
			const Angle&		GetLocalRotation() const;
			const Vector2D&		GetLocalScale() const;

			void				SetLocalPosition(const Vector2D& position);
			void				SetLocalRotation(const Angle& rotation);
			void				SetLocalScale(const Vector2D& scale);
			void				SetLocalScale(float uniformScale);

			//-----------------------------------------------------------------------------
			// World Space Properties (absolute game world coordinates)
			//
			// World space is the final position/rotation/scale in the game world.
			// If this transform has a parent, world values are computed by combining
			// local transform with parent's world transform.
			//
			// IMPORTANT: Getting world properties is cheap if no parent.
			//            If parent exists, requires calculation up the hierarchy.
			//            Setting world properties is more expensive as it must convert
			//            from world space back to local space.
			//-----------------------------------------------------------------------------

			// Get world position (absolute position in game world)
			Vector2D			GetWorldPosition() const;

			// Get world rotation (absolute rotation in game world)
			Angle				GetWorldRotation() const;

			// Get world scale (absolute scale in game world)
			Vector2D			GetWorldScale() const;

			// Set world position (automatically updates local position if parented)
			void				SetWorldPosition(const Vector2D& position);

			// Set world rotation (automatically updates local rotation if parented)
			void				SetWorldRotation(const Angle& rotation);

			// Set world scale (automatically updates local scale if parented)
			void				SetWorldScale(const Vector2D& scale);

			// Get all world properties at once (optimized for when you need multiple)
			// This traverses the parent hierarchy only once, much faster than calling
			// GetWorldPosition(), GetWorldRotation(), GetWorldScale() separately
			// Use when: You need 2+ world properties in the same frame
			void				GetWorldTransform(Vector2D& outPosition, Angle& outRotation, Vector2D& outScale) const;

			//-----------------------------------------------------------------------------
			// Hierarchy
			//
			// Transform hierarchy allows objects to be "attached" to other objects.
			// Child transforms inherit and combine their parent's transformation.
			//
			// ⚠️ CRITICAL WARNINGS:
			//
			// 1. LIFETIME: This class stores a raw pointer to parent.
			//              Caller must ensure parent outlives child, or call
			//              SetParent(nullptr) before parent is destroyed.
			//
			// 2. NO CYCLES: You MUST NOT create circular parent chains!
			//               Example: A->B->C->A will cause STACK OVERFLOW
			//               Always ensure hierarchy is a tree (no cycles)
			//               Debug builds may assert on cycles, but release builds will crash
			//
			// VALID:   A -> B -> C -> D (tree)
			// INVALID: A -> B -> C -> A (cycle) ❌ WILL CRASH!
			//-----------------------------------------------------------------------------

			// Get parent transform (nullptr if no parent)
			Transform2D*		GetParent() const;

			// Set parent transform (pass nullptr to detach from parent)
			// After calling this, world transform will be different unless you
			// adjust local transform to compensate
			//
			// ⚠️ WARNING: Do not create cycles! Setting parent must maintain tree structure.
			//            A->B->C->A will cause infinite recursion and stack overflow!
			void				SetParent(Transform2D* parent);

			// Check if this transform has a parent
			bool				HasParent() const;

			//-----------------------------------------------------------------------------
			// Transformations
			//-----------------------------------------------------------------------------

			// Translate in local space
			void				Translate(const Vector2D& delta);

			// Translate in world space
			void				TranslateWorld(const Vector2D& delta);

			// Rotate around local origin
			void				Rotate(const Angle& delta);

			// Scale relative to current scale
			void				Scale(const Vector2D& scale);
			void				Scale(float uniformScale);

			//-----------------------------------------------------------------------------
			// Space Conversions
			//
			// These functions convert coordinates between local and world space.
			// Useful for:
			//   - Converting mouse position to object's local coordinate system
			//   - Determining if a point is "in front of" or "behind" an object
			//   - Positioning child objects relative to parent
			//-----------------------------------------------------------------------------

			// Transform point from local to world space
			// Applies: Scale → Rotate → Translate (in that order)
			// Use for: Converting object-relative positions to world positions
			Vector2D			TransformPoint(const Vector2D& localPoint) const;

			// Transform direction from local to world space (ignores translation)
			// Applies: Scale → Rotate (no translation for directions)
			// Use for: Converting object-relative directions (like "forward") to world
			Vector2D			TransformDirection(const Vector2D& localDirection) const;

			// Transform point from world to local space (inverse operation)
			// Applies: Translate → Rotate → Scale (inverse order of TransformPoint)
			// Use for: Converting world position to object's coordinate system
			//          Example: Is this point to the left or right of the object?
			Vector2D			InverseTransformPoint(const Vector2D& worldPoint) const;

			// Transform direction from world to local space (ignores translation)
			// Applies: Rotate → Scale (inverse, no translation)
			// Use for: Converting world direction to object's coordinate system
			Vector2D			InverseTransformDirection(const Vector2D& worldDirection) const;

			//-----------------------------------------------------------------------------
			// Matrix generation
			//-----------------------------------------------------------------------------

			// Get local transformation matrix (3x3 for 2D)
			Matrix33			GetLocalMatrix() const;

			// Get world transformation matrix (3x3 for 2D)
			Matrix33			GetWorldMatrix() const;

			//-----------------------------------------------------------------------------
			// Utility Functions
			//-----------------------------------------------------------------------------

			// Get forward direction in world space (rotated +X axis)
			// Returns normalized vector pointing in the direction the transform "faces"
			// Use for: Movement direction, projectile direction, line-of-sight
			Vector2D			GetForward() const;

			// Get right direction in world space (rotated +Y axis)
			// Returns normalized vector pointing to the right of the transform
			// Use for: Strafing movement, perpendicular calculations
			Vector2D			GetRight() const;

			// Rotate transform to look at target position
			// Sets rotation so that forward direction points toward target
			// Use for: Enemies tracking player, turrets aiming, billboards
			// Note: Does nothing if target is at same position as transform
			void				LookAt(const Vector2D& target);

		private:
			Vector2D			mLocalPosition;
			Angle				mLocalRotation;
			Vector2D			mLocalScale;

			Transform2D*		mParent;

			// Helper to compute world transformation when parent exists
			void				GetParentWorldTransform(Vector2D& outPosition, Angle& outRotation, Vector2D& outScale) const;
		};
	}
}

#include "DiaMaths/Transform/Transform2D.inl"
