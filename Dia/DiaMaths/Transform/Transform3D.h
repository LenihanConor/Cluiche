#pragma once

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Quaternion/Quaternion.h"

namespace Dia
{
	namespace Maths
	{
		class Matrix44;
		class Matrix34;
		class Angle;

		//==============================================================================
		// CLASS Transform3D
		//==============================================================================
		// Represents a 3D transformation with position, rotation, and scale
		// Supports parent-child hierarchy for scene graphs
		//
		// CONCEPTS:
		//   Local Space  - Position/rotation/scale relative to parent transform
		//   World Space  - Position/rotation/scale in absolute game world coordinates
		//
		// HIERARCHY:
		//   A transform can have a parent. Its world transform is computed by
		//   combining its local transform with its parent's world transform.
		//   Example: Sword attached to player's hand
		//     - Player transform: position, rotation, scale
		//     - Sword local transform: offset from player's hand
		//     - Sword world transform: automatically calculated from both
		//
		// ROTATION: Stored as Quaternion. Euler setters are sugar (YXZ intrinsic).
		// AXES: Y-up right-handed. Forward = -Z, Right = +X, Up = +Y.
		//
		// USAGE:
		//   Transform3D player;
		//   Transform3D sword;
		//   sword.SetParent(&player);
		//   sword.SetLocalPosition(Vector3D(0.5f, 0.0f, 0.0f)); // Sword offset from player
		//   Vector3D swordWorldPos = sword.GetWorldPosition(); // Actual game position
		//==============================================================================
		class Transform3D
		{
		public:
			// Default constructor - identity transform at origin
			Transform3D();

			// Construct with initial values
			Transform3D(const Vector3D& position, const Quaternion& rotation, const Vector3D& scale);

			// Copy constructor
			Transform3D(const Transform3D& other);

			// Assignment operator
			Transform3D& operator=(const Transform3D& other);

			//-----------------------------------------------------------------------------
			// Local Space Properties (relative to parent)
			//
			// Local space is used to position objects relative to their parent.
			// Example: A turret on a spaceship uses local space - when ship moves/rotates,
			//          turret moves with it while maintaining its relative position.
			//-----------------------------------------------------------------------------

			const Vector3D&		GetLocalPosition() const;
			const Quaternion&	GetLocalRotation() const;
			const Vector3D&		GetLocalScale() const;

			void				SetLocalPosition(const Vector3D& position);
			void				SetLocalRotation(const Quaternion& rotation);
			void				SetLocalRotation(const Angle& yaw, const Angle& pitch, const Angle& roll);  // YXZ intrinsic
			void				SetLocalScale(const Vector3D& scale);
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
			Vector3D			GetWorldPosition() const;

			// Get world rotation (absolute rotation in game world)
			Quaternion			GetWorldRotation() const;

			// Get world scale (absolute scale in game world)
			Vector3D			GetWorldScale() const;

			// Set world position (automatically updates local position if parented)
			void				SetWorldPosition(const Vector3D& position);

			// Set world rotation (automatically updates local rotation if parented)
			void				SetWorldRotation(const Quaternion& rotation);

			// Set world scale (automatically updates local scale if parented)
			void				SetWorldScale(const Vector3D& scale);

			// Get all world properties at once (optimized for when you need multiple)
			// This traverses the parent hierarchy only once, much faster than calling
			// GetWorldPosition(), GetWorldRotation(), GetWorldScale() separately
			// Use when: You need 2+ world properties in the same frame
			void				GetWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;

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
			Transform3D*		GetParent() const;

			// Set parent transform (pass nullptr to detach from parent)
			// After calling this, world transform will be different unless you
			// adjust local transform to compensate
			//
			// ⚠️ WARNING: Do not create cycles! Setting parent must maintain tree structure.
			//            A->B->C->A will cause infinite recursion and stack overflow!
			void				SetParent(Transform3D* parent);

			// Check if this transform has a parent
			bool				HasParent() const;

			//-----------------------------------------------------------------------------
			// Transformations
			//-----------------------------------------------------------------------------

			// Translate in local space (delta rotated by local rotation)
			void				Translate(const Vector3D& delta);

			// Translate in world space
			void				TranslateWorld(const Vector3D& delta);

			// Rotate around local origin (applies delta * current rotation)
			void				Rotate(const Quaternion& delta);

			// Scale relative to current scale
			void				Scale(const Vector3D& scale);
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
			Vector3D			TransformPoint(const Vector3D& localPoint) const;

			// Transform direction from local to world space (ignores translation)
			// Applies: Scale → Rotate (no translation for directions)
			// Use for: Converting object-relative directions (like "forward") to world
			Vector3D			TransformDirection(const Vector3D& localDirection) const;

			// Transform point from world to local space (inverse operation)
			// Applies: Translate → Rotate → Scale (inverse order of TransformPoint)
			// Use for: Converting world position to object's coordinate system
			//          Example: Is this point to the left or right of the object?
			Vector3D			InverseTransformPoint(const Vector3D& worldPoint) const;

			// Transform direction from world to local space (ignores translation)
			// Applies: Rotate → Scale (inverse, no translation)
			// Use for: Converting world direction to object's coordinate system
			Vector3D			InverseTransformDirection(const Vector3D& worldDirection) const;

			//-----------------------------------------------------------------------------
			// Matrix generation
			//-----------------------------------------------------------------------------

			// Get local transformation matrix (4x4 for 3D)
			Matrix44			GetLocalMatrix() const;

			// Get world transformation matrix (4x4 for 3D)
			Matrix44			GetWorldMatrix() const;

			// Get local transformation as affine matrix (3x4)
			Matrix34			GetLocalAffine() const;

			// Get world transformation as affine matrix (3x4)
			Matrix34			GetWorldAffine() const;

			//-----------------------------------------------------------------------------
			// Utility Functions
			//-----------------------------------------------------------------------------

			// Get forward direction in world space (rotated -Z axis, Y-up RH)
			// Returns normalized vector pointing in the direction the transform "faces"
			// Use for: Movement direction, projectile direction, line-of-sight
			Vector3D			GetForward() const;

			// Get right direction in world space (rotated +X axis, Y-up RH)
			// Returns normalized vector pointing to the right of the transform
			// Use for: Strafing movement, perpendicular calculations
			Vector3D			GetRight() const;

			// Get up direction in world space (rotated +Y axis, Y-up RH)
			// Returns normalized vector pointing upward from the transform
			// Use for: Jump direction, vertical alignment
			Vector3D			GetUp() const;

			// Rotate transform to look at target position in world space
			// Sets rotation so that forward direction points toward target
			// Use for: Enemies tracking player, turrets aiming, cameras
			// Note: Does nothing if target is at same position as transform
			void				LookAt(const Vector3D& target, const Vector3D& up = Vector3D::YAxis());

		private:
			Vector3D			mLocalPosition;
			Quaternion			mLocalRotation;
			Vector3D			mLocalScale;

			Transform3D*		mParent;

			// Helper to compute world transformation when parent exists
			void				GetParentWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;
		};
	}
}

#include "DiaMaths/Transform/Transform3D.inl"
