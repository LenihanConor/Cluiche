#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/Trigonometry.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Transform2D::Transform2D()
			: mLocalPosition(Vector2D::Zero())
			, mLocalRotation(Angle::Deg0)
			, mLocalScale(1.0f, 1.0f)
			, mParent(nullptr)
		{
		}

		inline Transform2D::Transform2D(const Vector2D& position, const Angle& rotation, const Vector2D& scale)
			: mLocalPosition(position)
			, mLocalRotation(rotation)
			, mLocalScale(scale)
			, mParent(nullptr)
		{
		}

		inline Transform2D::Transform2D(const Transform2D& other)
			: mLocalPosition(other.mLocalPosition)
			, mLocalRotation(other.mLocalRotation)
			, mLocalScale(other.mLocalScale)
			, mParent(other.mParent)
		{
		}

		inline Transform2D& Transform2D::operator=(const Transform2D& other)
		{
			if (this != &other)
			{
				mLocalPosition = other.mLocalPosition;
				mLocalRotation = other.mLocalRotation;
				mLocalScale = other.mLocalScale;
				mParent = other.mParent;
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		// Local space properties
		//-----------------------------------------------------------------------------

		inline const Vector2D& Transform2D::GetLocalPosition() const
		{
			return mLocalPosition;
		}

		inline const Angle& Transform2D::GetLocalRotation() const
		{
			return mLocalRotation;
		}

		inline const Vector2D& Transform2D::GetLocalScale() const
		{
			return mLocalScale;
		}

		inline void Transform2D::SetLocalPosition(const Vector2D& position)
		{
			mLocalPosition = position;
		}

		inline void Transform2D::SetLocalRotation(const Angle& rotation)
		{
			mLocalRotation = rotation;
		}

		inline void Transform2D::SetLocalScale(const Vector2D& scale)
		{
			mLocalScale = scale;
		}

		inline void Transform2D::SetLocalScale(float uniformScale)
		{
			mLocalScale.Set(uniformScale, uniformScale);
		}

		//-----------------------------------------------------------------------------
		// World space properties
		//
		// World transform = parent world transform × local transform
		// Order of operations (TRS): Translate, Rotate, Scale
		//-----------------------------------------------------------------------------

		inline Vector2D Transform2D::GetWorldPosition() const
		{
			// No parent? Local space IS world space
			if (!mParent)
			{
				return mLocalPosition;
			}

			// Transform local position by parent's world transform
			// This combines the transformations: apply parent transform to local offset
			Vector2D parentPos;
			Angle parentRot;
			Vector2D parentScale;
			GetParentWorldTransform(parentPos, parentRot, parentScale);

			// Apply parent's transformation to local position:
			// 1. Scale by parent's scale (child size affected by parent)
			// 2. Rotate by parent's rotation (child rotates with parent)
			// 3. Translate by parent's position (child moves with parent)
			Vector2D scaled = mLocalPosition * parentScale;
			Vector2D rotated = scaled.AsRotateCounterClockwiseBy(parentRot);
			return parentPos + rotated;
		}

		inline Angle Transform2D::GetWorldRotation() const
		{
			if (!mParent)
			{
				return mLocalRotation;
			}

			return mParent->GetWorldRotation() + mLocalRotation;
		}

		inline Vector2D Transform2D::GetWorldScale() const
		{
			if (!mParent)
			{
				return mLocalScale;
			}

			Vector2D parentScale = mParent->GetWorldScale();
			return Vector2D(mLocalScale.x * parentScale.x, mLocalScale.y * parentScale.y);
		}

		inline void Transform2D::SetWorldPosition(const Vector2D& position)
		{
			// No parent? World position directly sets local position
			if (!mParent)
			{
				mLocalPosition = position;
				return;
			}

			// Convert world position to local space relative to parent
			// This finds: "What local position gives us this world position?"
			// Uses inverse transform to convert from parent's space
			mLocalPosition = mParent->InverseTransformPoint(position);
		}

		inline void Transform2D::SetWorldRotation(const Angle& rotation)
		{
			if (!mParent)
			{
				mLocalRotation = rotation;
				return;
			}

			mLocalRotation = rotation - mParent->GetWorldRotation();
		}

		inline void Transform2D::SetWorldScale(const Vector2D& scale)
		{
			if (!mParent)
			{
				mLocalScale = scale;
				return;
			}

			Vector2D parentScale = mParent->GetWorldScale();
			mLocalScale.x = (Dia::Maths::Float::FAbs(parentScale.x) > FLOAT_EPSILON) ? scale.x / parentScale.x : scale.x;
			mLocalScale.y = (Dia::Maths::Float::FAbs(parentScale.y) > FLOAT_EPSILON) ? scale.y / parentScale.y : scale.y;
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
		//   Vector2D pos; Angle rot; Vector2D scale;
		//   transform.GetWorldTransform(pos, rot, scale);  // Get all at once!
		inline void Transform2D::GetWorldTransform(Vector2D& outPosition, Angle& outRotation, Vector2D& outScale) const
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
			Vector2D parentPos;
			Angle parentRot;
			Vector2D parentScale;
			mParent->GetWorldTransform(parentPos, parentRot, parentScale);

			// Combine with local transform
			outRotation = parentRot + mLocalRotation;
			outScale = Vector2D(mLocalScale.x * parentScale.x, mLocalScale.y * parentScale.y);

			// Transform local position by parent's world transform
			Vector2D scaled = mLocalPosition * parentScale;
			Vector2D rotated = scaled.AsRotateCounterClockwiseBy(parentRot);
			outPosition = parentPos + rotated;
		}

		//-----------------------------------------------------------------------------
		// Hierarchy
		//-----------------------------------------------------------------------------

		inline Transform2D* Transform2D::GetParent() const
		{
			return mParent;
		}

		inline void Transform2D::SetParent(Transform2D* parent)
		{
			mParent = parent;
		}

		inline bool Transform2D::HasParent() const
		{
			return mParent != nullptr;
		}

		//-----------------------------------------------------------------------------
		// Transformations
		//-----------------------------------------------------------------------------

		inline void Transform2D::Translate(const Vector2D& delta)
		{
			mLocalPosition += delta;
		}

		inline void Transform2D::TranslateWorld(const Vector2D& delta)
		{
			if (!mParent)
			{
				mLocalPosition += delta;
			}
			else
			{
				// Convert world delta to local space
				Vector2D localDelta = mParent->InverseTransformDirection(delta);
				mLocalPosition += localDelta;
			}
		}

		inline void Transform2D::Rotate(const Angle& delta)
		{
			mLocalRotation += delta;
		}

		inline void Transform2D::Scale(const Vector2D& scale)
		{
			mLocalScale.x *= scale.x;
			mLocalScale.y *= scale.y;
		}

		inline void Transform2D::Scale(float uniformScale)
		{
			mLocalScale.x *= uniformScale;
			mLocalScale.y *= uniformScale;
		}

		//-----------------------------------------------------------------------------
		// Space conversions
		//
		// Transform order: Scale → Rotate → Translate (SRT)
		// Inverse order:   Translate → Rotate → Scale (reverse of SRT)
		//-----------------------------------------------------------------------------

		inline Vector2D Transform2D::TransformPoint(const Vector2D& localPoint) const
		{
			Vector2D worldPos = GetWorldPosition();
			Angle worldRot = GetWorldRotation();
			Vector2D worldScale = GetWorldScale();

			// Standard transformation order: Scale → Rotate → Translate
			// 1. Scale: Apply object's size to the point
			Vector2D scaled(localPoint.x * worldScale.x, localPoint.y * worldScale.y);

			// 2. Rotate: Apply object's rotation to the scaled point
			Vector2D rotated = scaled.AsRotateCounterClockwiseBy(worldRot);

			// 3. Translate: Add object's position to move point to world space
			return worldPos + rotated;
		}

		inline Vector2D Transform2D::TransformDirection(const Vector2D& localDirection) const
		{
			Angle worldRot = GetWorldRotation();
			Vector2D worldScale = GetWorldScale();

			// Scale and rotate (no translation for directions)
			Vector2D scaled(localDirection.x * worldScale.x, localDirection.y * worldScale.y);
			return scaled.AsRotateCounterClockwiseBy(worldRot);
		}

		inline Vector2D Transform2D::InverseTransformPoint(const Vector2D& worldPoint) const
		{
			Vector2D worldPos = GetWorldPosition();
			Angle worldRot = GetWorldRotation();
			Vector2D worldScale = GetWorldScale();

			// Inverse transformation: reverse order of TransformPoint
			// Original was: Scale → Rotate → Translate
			// Inverse is:   Translate → Rotate → Scale (backwards!)

			// 1. Translate: Remove object's position offset
			Vector2D translated = worldPoint - worldPos;

			// 2. Rotate: Rotate backwards to undo object's rotation
			Vector2D rotated = translated.AsRotateClockwiseBy(worldRot);

			// 3. Scale: Divide by scale to undo object's size transformation
			// Must check for zero/near-zero scale to avoid division by zero
			Vector2D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > FLOAT_EPSILON) ? rotated.x / worldScale.x : rotated.x;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > FLOAT_EPSILON) ? rotated.y / worldScale.y : rotated.y;

			return result;
		}

		inline Vector2D Transform2D::InverseTransformDirection(const Vector2D& worldDirection) const
		{
			Angle worldRot = GetWorldRotation();
			Vector2D worldScale = GetWorldScale();

			// Rotate back and scale back (no translation for directions)
			Vector2D rotated = worldDirection.AsRotateClockwiseBy(worldRot);

			Vector2D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > FLOAT_EPSILON) ? rotated.x / worldScale.x : rotated.x;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > FLOAT_EPSILON) ? rotated.y / worldScale.y : rotated.y;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Utility
		//-----------------------------------------------------------------------------

		inline Vector2D Transform2D::GetForward() const
		{
			// Forward is rotated +X axis
			return Vector2D::XAxis().AsRotateCounterClockwiseBy(GetWorldRotation());
		}

		inline Vector2D Transform2D::GetRight() const
		{
			// Right is rotated +Y axis
			return Vector2D::YAxis().AsRotateCounterClockwiseBy(GetWorldRotation());
		}

		inline void Transform2D::LookAt(const Vector2D& target)
		{
			Vector2D worldPos = GetWorldPosition();
			Vector2D direction = target - worldPos;

			// If target is at same position, looking "at" it is undefined
			if (direction.SquareMagnitude() < FLOAT_EPSILON)
			{
				return; // Do nothing - maintain current rotation
			}

			// Calculate angle to target using atan2
			// atan2(y, x) returns angle in radians from positive X axis
			// This naturally gives us the rotation needed to point at target
			float angleRadians = Dia::Maths::ATan2(direction.y, direction.x);
			Angle targetRotation = Angle::FromRadians(angleRadians);

			// Set the rotation (handles parent transform automatically if needed)
			SetWorldRotation(targetRotation);
		}

		//-----------------------------------------------------------------------------
		// Private helpers
		//-----------------------------------------------------------------------------

		inline void Transform2D::GetParentWorldTransform(Vector2D& outPosition, Angle& outRotation, Vector2D& outScale) const
		{
			if (mParent)
			{
				outPosition = mParent->GetWorldPosition();
				outRotation = mParent->GetWorldRotation();
				outScale = mParent->GetWorldScale();
			}
			else
			{
				outPosition = Vector2D::Zero();
				outRotation = Angle::Deg0;
				outScale = Vector2D(1.0f, 1.0f);
			}
		}
	}
}
