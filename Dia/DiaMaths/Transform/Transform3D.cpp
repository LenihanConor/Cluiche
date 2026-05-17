#include "DiaMaths/Transform/Transform3D.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Matrix/Matrix34.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// World space properties
		//
		// World transform = parent world transform × local transform
		// Order of operations (TRS): Scale, Rotate, Translate
		//-----------------------------------------------------------------------------

		Vector3D Transform3D::GetWorldPosition() const
		{
			// No parent? Local space IS world space
			if (!mParent)
			{
				return mLocalPosition;
			}

			// Transform local position by parent's world transform
			Vector3D parentPos;
			Quaternion parentRot;
			Vector3D parentScale;
			GetParentWorldTransform(parentPos, parentRot, parentScale);

			// Apply parent's transformation to local position:
			// 1. Scale by parent's scale (child size affected by parent)
			// 2. Rotate by parent's rotation (child rotates with parent)
			// 3. Translate by parent's position (child moves with parent)
			Vector3D scaled(mLocalPosition.x * parentScale.x,
			                mLocalPosition.y * parentScale.y,
			                mLocalPosition.z * parentScale.z);
			Vector3D rotated = parentRot.Rotate(scaled);
			return parentPos + rotated;
		}

		Quaternion Transform3D::GetWorldRotation() const
		{
			if (!mParent)
			{
				return mLocalRotation;
			}

			// World rotation = parent.WorldRot * mLocalRot
			return mParent->GetWorldRotation() * mLocalRotation;
		}

		Vector3D Transform3D::GetWorldScale() const
		{
			if (!mParent)
			{
				return mLocalScale;
			}

			Vector3D parentScale = mParent->GetWorldScale();
			return Vector3D(mLocalScale.x * parentScale.x,
			                mLocalScale.y * parentScale.y,
			                mLocalScale.z * parentScale.z);
		}

		void Transform3D::SetWorldPosition(const Vector3D& position)
		{
			// No parent? World position directly sets local position
			if (!mParent)
			{
				mLocalPosition = position;
				return;
			}

			// Convert world position to local space relative to parent
			// Back-solve: mLocalPosition = parentRot^-1 * (position - parentPos) / parentScale
			Vector3D parentPos;
			Quaternion parentRot;
			Vector3D parentScale;
			GetParentWorldTransform(parentPos, parentRot, parentScale);

			Vector3D translated = position - parentPos;
			Vector3D rotated = parentRot.Inverse().Rotate(translated);

			// Component-wise division with zero guard (epsilon = 1e-6f)
			constexpr float kEpsilon = 1e-6f;
			mLocalPosition.x = (Dia::Maths::Float::FAbs(parentScale.x) > kEpsilon) ? rotated.x / parentScale.x : 0.0f;
			mLocalPosition.y = (Dia::Maths::Float::FAbs(parentScale.y) > kEpsilon) ? rotated.y / parentScale.y : 0.0f;
			mLocalPosition.z = (Dia::Maths::Float::FAbs(parentScale.z) > kEpsilon) ? rotated.z / parentScale.z : 0.0f;
		}

		void Transform3D::SetWorldRotation(const Quaternion& rotation)
		{
			if (!mParent)
			{
				mLocalRotation = rotation;
				return;
			}

			// Back-solve: mLocalRotation = parentRot^-1 * rotation
			mLocalRotation = mParent->GetWorldRotation().Inverse() * rotation;
		}

		void Transform3D::SetWorldScale(const Vector3D& scale)
		{
			if (!mParent)
			{
				mLocalScale = scale;
				return;
			}

			Vector3D parentScale = mParent->GetWorldScale();

			// Component-wise division with zero guard
			constexpr float kEpsilon = 1e-6f;
			mLocalScale.x = (Dia::Maths::Float::FAbs(parentScale.x) > kEpsilon) ? scale.x / parentScale.x : 0.0f;
			mLocalScale.y = (Dia::Maths::Float::FAbs(parentScale.y) > kEpsilon) ? scale.y / parentScale.y : 0.0f;
			mLocalScale.z = (Dia::Maths::Float::FAbs(parentScale.z) > kEpsilon) ? scale.z / parentScale.z : 0.0f;
		}

		//-----------------------------------------------------------------------------
		// OPTIMIZED: Get all world properties at once
		// This is much faster than calling GetWorldPosition/Rotation/Scale separately
		// because it only traverses the parent hierarchy once
		//
		// PERFORMANCE COMPARISON:
		//   Separate calls: O(3 * hierarchy_depth)
		//   This function:  O(hierarchy_depth)
		//
		// Example usage:
		//   Vector3D pos; Quaternion rot; Vector3D scale;
		//   transform.GetWorldTransform(pos, rot, scale);  // Get all at once!
		void Transform3D::GetWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const
		{
			if (!mParent)
			{
				// No parent - local IS world
				outPosition = mLocalPosition;
				outRotation = mLocalRotation;
				outScale = mLocalScale;
				return;
			}

			// Get parent's world transform (recursive call up the hierarchy)
			Vector3D parentPos;
			Quaternion parentRot;
			Vector3D parentScale;
			mParent->GetWorldTransform(parentPos, parentRot, parentScale);

			// Combine with local transform
			outRotation = parentRot * mLocalRotation;
			outScale = Vector3D(mLocalScale.x * parentScale.x,
			                    mLocalScale.y * parentScale.y,
			                    mLocalScale.z * parentScale.z);

			// Transform local position by parent's world transform
			Vector3D scaled(mLocalPosition.x * parentScale.x,
			                mLocalPosition.y * parentScale.y,
			                mLocalPosition.z * parentScale.z);
			Vector3D rotated = parentRot.Rotate(scaled);
			outPosition = parentPos + rotated;
		}

		//-----------------------------------------------------------------------------
		// Hierarchy
		//-----------------------------------------------------------------------------

		void Transform3D::SetParent(Transform3D* parent)
		{
#if !defined(NDEBUG)
			// Debug: Walk ancestor chain looking for cycle
			if (parent != nullptr)
			{
				Transform3D* ancestor = parent;
				while (ancestor != nullptr)
				{
					if (ancestor == this)
					{
						// Cycle detected - reject the change
						DIA_ASSERT(false, "Transform3D parent cycle detected");
						return;
					}
					ancestor = ancestor->mParent;
				}
			}
#endif
			mParent = parent;
		}

		//-----------------------------------------------------------------------------
		// Space conversions
		//
		// Transform order: Scale → Rotate → Translate (SRT)
		// Inverse order:   Translate → Rotate → Scale (reverse of SRT)
		//-----------------------------------------------------------------------------

		Vector3D Transform3D::TransformPoint(const Vector3D& localPoint) const
		{
			Vector3D worldPos = GetWorldPosition();
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			// Standard transformation order: Scale → Rotate → Translate
			// 1. Scale: Apply object's size to the point
			Vector3D scaled(localPoint.x * worldScale.x,
			                localPoint.y * worldScale.y,
			                localPoint.z * worldScale.z);

			// 2. Rotate: Apply object's rotation to the scaled point
			Vector3D rotated = worldRot.Rotate(scaled);

			// 3. Translate: Add object's position to move point to world space
			return worldPos + rotated;
		}

		Vector3D Transform3D::TransformDirection(const Vector3D& localDirection) const
		{
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			// Scale and rotate (no translation for directions)
			Vector3D scaled(localDirection.x * worldScale.x,
			                localDirection.y * worldScale.y,
			                localDirection.z * worldScale.z);
			return worldRot.Rotate(scaled);
		}

		Vector3D Transform3D::InverseTransformPoint(const Vector3D& worldPoint) const
		{
			Vector3D worldPos = GetWorldPosition();
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			// Inverse transformation: reverse order of TransformPoint
			// Original was: Scale → Rotate → Translate
			// Inverse is:   Translate → Rotate → Scale (backwards!)

			// 1. Translate: Remove object's position offset
			Vector3D translated = worldPoint - worldPos;

			// 2. Rotate: Rotate backwards to undo object's rotation
			Vector3D rotated = worldRot.Inverse().Rotate(translated);

			// 3. Scale: Divide by scale to undo object's size transformation
			// Must check for zero/near-zero scale to avoid division by zero
			constexpr float kEpsilon = 1e-6f;
			Vector3D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > kEpsilon) ? rotated.x / worldScale.x : 0.0f;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > kEpsilon) ? rotated.y / worldScale.y : 0.0f;
			result.z = (Dia::Maths::Float::FAbs(worldScale.z) > kEpsilon) ? rotated.z / worldScale.z : 0.0f;

			return result;
		}

		Vector3D Transform3D::InverseTransformDirection(const Vector3D& worldDirection) const
		{
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			// Rotate back and scale back (no translation for directions)
			Vector3D rotated = worldRot.Inverse().Rotate(worldDirection);

			constexpr float kEpsilon = 1e-6f;
			Vector3D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > kEpsilon) ? rotated.x / worldScale.x : 0.0f;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > kEpsilon) ? rotated.y / worldScale.y : 0.0f;
			result.z = (Dia::Maths::Float::FAbs(worldScale.z) > kEpsilon) ? rotated.z / worldScale.z : 0.0f;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Matrix generation
		//-----------------------------------------------------------------------------

		Matrix44 Transform3D::GetLocalMatrix() const
		{
			return Matrix44::FromTRS(mLocalPosition, mLocalRotation, mLocalScale);
		}

		Matrix44 Transform3D::GetWorldMatrix() const
		{
			Vector3D worldPos = GetWorldPosition();
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			return Matrix44::FromTRS(worldPos, worldRot, worldScale);
		}

		Matrix34 Transform3D::GetLocalAffine() const
		{
			return Matrix34::FromTRS(mLocalPosition, mLocalRotation, mLocalScale);
		}

		Matrix34 Transform3D::GetWorldAffine() const
		{
			Vector3D worldPos = GetWorldPosition();
			Quaternion worldRot = GetWorldRotation();
			Vector3D worldScale = GetWorldScale();

			return Matrix34::FromTRS(worldPos, worldRot, worldScale);
		}

		//-----------------------------------------------------------------------------
		// Utility
		//-----------------------------------------------------------------------------

		void Transform3D::LookAt(const Vector3D& target, const Vector3D& up)
		{
			Vector3D worldPos = GetWorldPosition();
			Vector3D direction = target - worldPos;

			// If target is at same position, looking "at" it is undefined
			if (direction.SquareMagnitude() < FLOAT_EPSILON)
			{
#if !defined(NDEBUG)
				DIA_ASSERT(false, "Transform3D::LookAt: target is at same position as transform");
#endif
				return; // Do nothing - maintain current rotation
			}

			Vector3D forward = direction.AsNormal();

			// Check if up is parallel to forward (degenerate case)
#if !defined(NDEBUG)
			float upDotForward = Dia::Maths::Float::FAbs(up.Dot(forward));
			DIA_ASSERT(upDotForward < 0.9999f, "Transform3D::LookAt: up vector is parallel to target direction");
#endif

			// Compute look rotation quaternion
			Quaternion lookRotation = Quaternion::LookRotation(forward, up);

			// Back-solve if we have a parent
			if (mParent)
			{
				Quaternion parentRot = mParent->GetWorldRotation();
				mLocalRotation = parentRot.Inverse() * lookRotation;
			}
			else
			{
				mLocalRotation = lookRotation;
			}
		}

		//-----------------------------------------------------------------------------
		// Private helpers
		//-----------------------------------------------------------------------------

		void Transform3D::GetParentWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const
		{
			if (mParent)
			{
				outPosition = mParent->GetWorldPosition();
				outRotation = mParent->GetWorldRotation();
				outScale = mParent->GetWorldScale();
			}
			else
			{
				outPosition = Vector3D::Zero();
				outRotation = Quaternion::Identity();
				outScale = Vector3D(1.0f, 1.0f, 1.0f);
			}
		}
	}
}
