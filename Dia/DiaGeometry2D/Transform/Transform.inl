#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/MathsDefines.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaMaths/Core/FloatMaths.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Transform::Transform()
			: mLocalPosition(Dia::Maths::Vector2D::Zero())
			, mLocalRotation(Dia::Maths::Angle::Deg0)
			, mLocalScale(1.0f, 1.0f)
			, mParent(nullptr)
		{
		}

		inline Transform::Transform(const Dia::Maths::Vector2D& position, const Dia::Maths::Angle& rotation, const Dia::Maths::Vector2D& scale)
			: mLocalPosition(position)
			, mLocalRotation(rotation)
			, mLocalScale(scale)
			, mParent(nullptr)
		{
		}

		inline Transform::Transform(const Transform& other)
			: mLocalPosition(other.mLocalPosition)
			, mLocalRotation(other.mLocalRotation)
			, mLocalScale(other.mLocalScale)
			, mParent(other.mParent)
		{
		}

		inline Transform& Transform::operator=(const Transform& other)
		{
			if (this != &other)
			{
				mLocalPosition = other.mLocalPosition;
				mLocalRotation = other.mLocalRotation;
				mLocalScale    = other.mLocalScale;
				mParent        = other.mParent;
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		// Local space properties
		//-----------------------------------------------------------------------------

		inline const Dia::Maths::Vector2D& Transform::GetLocalPosition() const
		{
			return mLocalPosition;
		}

		inline const Dia::Maths::Angle& Transform::GetLocalRotation() const
		{
			return mLocalRotation;
		}

		inline const Dia::Maths::Vector2D& Transform::GetLocalScale() const
		{
			return mLocalScale;
		}

		inline void Transform::SetLocalPosition(const Dia::Maths::Vector2D& position)
		{
			mLocalPosition = position;
		}

		inline void Transform::SetLocalRotation(const Dia::Maths::Angle& rotation)
		{
			mLocalRotation = rotation;
		}

		inline void Transform::SetLocalScale(const Dia::Maths::Vector2D& scale)
		{
			mLocalScale = scale;
		}

		inline void Transform::SetLocalScale(float uniformScale)
		{
			mLocalScale.Set(uniformScale, uniformScale);
		}

		//-----------------------------------------------------------------------------
		// World space properties
		//-----------------------------------------------------------------------------

		inline Dia::Maths::Vector2D Transform::GetWorldPosition() const
		{
			if (!mParent)
			{
				return mLocalPosition;
			}

			Dia::Maths::Vector2D parentPos;
			Dia::Maths::Angle parentRot;
			Dia::Maths::Vector2D parentScale;
			GetParentWorldTransform(parentPos, parentRot, parentScale);

			Dia::Maths::Vector2D scaled  = mLocalPosition * parentScale;
			Dia::Maths::Vector2D rotated = scaled.AsRotateCounterClockwiseBy(parentRot);
			return parentPos + rotated;
		}

		inline Dia::Maths::Angle Transform::GetWorldRotation() const
		{
			if (!mParent)
			{
				return mLocalRotation;
			}

			return mParent->GetWorldRotation() + mLocalRotation;
		}

		inline Dia::Maths::Vector2D Transform::GetWorldScale() const
		{
			if (!mParent)
			{
				return mLocalScale;
			}

			Dia::Maths::Vector2D parentScale = mParent->GetWorldScale();
			return Dia::Maths::Vector2D(mLocalScale.x * parentScale.x, mLocalScale.y * parentScale.y);
		}

		inline void Transform::SetWorldPosition(const Dia::Maths::Vector2D& position)
		{
			if (!mParent)
			{
				mLocalPosition = position;
				return;
			}

			mLocalPosition = mParent->InverseTransformPoint(position);
		}

		inline void Transform::SetWorldRotation(const Dia::Maths::Angle& rotation)
		{
			if (!mParent)
			{
				mLocalRotation = rotation;
				return;
			}

			mLocalRotation = rotation - mParent->GetWorldRotation();
		}

		inline void Transform::SetWorldScale(const Dia::Maths::Vector2D& scale)
		{
			if (!mParent)
			{
				mLocalScale = scale;
				return;
			}

			Dia::Maths::Vector2D parentScale = mParent->GetWorldScale();
			mLocalScale.x = (Dia::Maths::Float::FAbs(parentScale.x) > Dia::Maths::FLOAT_EPSILON) ? scale.x / parentScale.x : scale.x;
			mLocalScale.y = (Dia::Maths::Float::FAbs(parentScale.y) > Dia::Maths::FLOAT_EPSILON) ? scale.y / parentScale.y : scale.y;
		}

		inline void Transform::GetWorldTransform(Dia::Maths::Vector2D& outPosition, Dia::Maths::Angle& outRotation, Dia::Maths::Vector2D& outScale) const
		{
			if (!mParent)
			{
				outPosition = mLocalPosition;
				outRotation = mLocalRotation;
				outScale    = mLocalScale;
				return;
			}

			Dia::Maths::Vector2D parentPos;
			Dia::Maths::Angle parentRot;
			Dia::Maths::Vector2D parentScale;
			mParent->GetWorldTransform(parentPos, parentRot, parentScale);

			outRotation = parentRot + mLocalRotation;
			outScale    = Dia::Maths::Vector2D(mLocalScale.x * parentScale.x, mLocalScale.y * parentScale.y);

			Dia::Maths::Vector2D scaled  = mLocalPosition * parentScale;
			Dia::Maths::Vector2D rotated = scaled.AsRotateCounterClockwiseBy(parentRot);
			outPosition = parentPos + rotated;
		}

		//-----------------------------------------------------------------------------
		// Hierarchy
		//-----------------------------------------------------------------------------

		inline Transform* Transform::GetParent() const
		{
			return mParent;
		}

		inline void Transform::SetParent(Transform* parent)
		{
			mParent = parent;
		}

		inline bool Transform::HasParent() const
		{
			return mParent != nullptr;
		}

		//-----------------------------------------------------------------------------
		// Transformations
		//-----------------------------------------------------------------------------

		inline void Transform::Translate(const Dia::Maths::Vector2D& delta)
		{
			mLocalPosition += delta;
		}

		inline void Transform::TranslateWorld(const Dia::Maths::Vector2D& delta)
		{
			if (!mParent)
			{
				mLocalPosition += delta;
			}
			else
			{
				Dia::Maths::Vector2D localDelta = mParent->InverseTransformDirection(delta);
				mLocalPosition += localDelta;
			}
		}

		inline void Transform::Rotate(const Dia::Maths::Angle& delta)
		{
			mLocalRotation += delta;
		}

		inline void Transform::Scale(const Dia::Maths::Vector2D& scale)
		{
			mLocalScale.x *= scale.x;
			mLocalScale.y *= scale.y;
		}

		inline void Transform::Scale(float uniformScale)
		{
			mLocalScale.x *= uniformScale;
			mLocalScale.y *= uniformScale;
		}

		//-----------------------------------------------------------------------------
		// Space conversions
		//-----------------------------------------------------------------------------

		inline Dia::Maths::Vector2D Transform::TransformPoint(const Dia::Maths::Vector2D& localPoint) const
		{
			Dia::Maths::Vector2D worldPos   = GetWorldPosition();
			Dia::Maths::Angle worldRot      = GetWorldRotation();
			Dia::Maths::Vector2D worldScale = GetWorldScale();

			Dia::Maths::Vector2D scaled(localPoint.x * worldScale.x, localPoint.y * worldScale.y);
			if (scaled == Dia::Maths::Vector2D::Zero())
				return worldPos;
			Dia::Maths::Vector2D rotated = scaled.AsRotateCounterClockwiseBy(worldRot);
			return worldPos + rotated;
		}

		inline Dia::Maths::Vector2D Transform::TransformDirection(const Dia::Maths::Vector2D& localDirection) const
		{
			Dia::Maths::Angle worldRot      = GetWorldRotation();
			Dia::Maths::Vector2D worldScale = GetWorldScale();

			Dia::Maths::Vector2D scaled(localDirection.x * worldScale.x, localDirection.y * worldScale.y);
			if (scaled == Dia::Maths::Vector2D::Zero())
				return Dia::Maths::Vector2D::Zero();
			return scaled.AsRotateCounterClockwiseBy(worldRot);
		}

		inline Dia::Maths::Vector2D Transform::InverseTransformPoint(const Dia::Maths::Vector2D& worldPoint) const
		{
			Dia::Maths::Vector2D worldPos   = GetWorldPosition();
			Dia::Maths::Angle worldRot      = GetWorldRotation();
			Dia::Maths::Vector2D worldScale = GetWorldScale();

			Dia::Maths::Vector2D translated = worldPoint - worldPos;
			if (translated == Dia::Maths::Vector2D::Zero())
				return Dia::Maths::Vector2D::Zero();
			Dia::Maths::Vector2D rotated    = translated.AsRotateClockwiseBy(worldRot);

			Dia::Maths::Vector2D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > Dia::Maths::FLOAT_EPSILON) ? rotated.x / worldScale.x : rotated.x;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > Dia::Maths::FLOAT_EPSILON) ? rotated.y / worldScale.y : rotated.y;

			return result;
		}

		inline Dia::Maths::Vector2D Transform::InverseTransformDirection(const Dia::Maths::Vector2D& worldDirection) const
		{
			Dia::Maths::Angle worldRot      = GetWorldRotation();
			Dia::Maths::Vector2D worldScale = GetWorldScale();

			Dia::Maths::Vector2D rotated = worldDirection.AsRotateClockwiseBy(worldRot);

			Dia::Maths::Vector2D result;
			result.x = (Dia::Maths::Float::FAbs(worldScale.x) > Dia::Maths::FLOAT_EPSILON) ? rotated.x / worldScale.x : rotated.x;
			result.y = (Dia::Maths::Float::FAbs(worldScale.y) > Dia::Maths::FLOAT_EPSILON) ? rotated.y / worldScale.y : rotated.y;

			return result;
		}

		//-----------------------------------------------------------------------------
		// Utility
		//-----------------------------------------------------------------------------

		inline Dia::Maths::Vector2D Transform::GetForward() const
		{
			return Dia::Maths::Vector2D::XAxis().AsRotateCounterClockwiseBy(GetWorldRotation());
		}

		inline Dia::Maths::Vector2D Transform::GetRight() const
		{
			return Dia::Maths::Vector2D::YAxis().AsRotateCounterClockwiseBy(GetWorldRotation());
		}

		inline void Transform::LookAt(const Dia::Maths::Vector2D& target)
		{
			Dia::Maths::Vector2D worldPos = GetWorldPosition();
			Dia::Maths::Vector2D direction = target - worldPos;

			if (direction.SquareMagnitude() < Dia::Maths::FLOAT_EPSILON)
			{
				return;
			}

			float angleRadians = Dia::Maths::ATan2(direction.y, direction.x);
			Dia::Maths::Angle targetRotation = Dia::Maths::Angle::FromRadians(angleRadians);

			SetWorldRotation(targetRotation);
		}

		//-----------------------------------------------------------------------------
		// Private helpers
		//-----------------------------------------------------------------------------

		inline void Transform::GetParentWorldTransform(Dia::Maths::Vector2D& outPosition, Dia::Maths::Angle& outRotation, Dia::Maths::Vector2D& outScale) const
		{
			if (mParent)
			{
				outPosition = mParent->GetWorldPosition();
				outRotation = mParent->GetWorldRotation();
				outScale    = mParent->GetWorldScale();
			}
			else
			{
				outPosition = Dia::Maths::Vector2D::Zero();
				outRotation = Dia::Maths::Angle::Deg0;
				outScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
			}
		}
	}
}
