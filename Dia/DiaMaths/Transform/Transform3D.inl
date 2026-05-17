#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Construction
		//-----------------------------------------------------------------------------

		inline Transform3D::Transform3D()
			: mLocalPosition(0.0f, 0.0f, 0.0f)
			, mLocalRotation(Quaternion::Identity())
			, mLocalScale(1.0f, 1.0f, 1.0f)
			, mParent(nullptr)
		{
		}

		inline Transform3D::Transform3D(const Vector3D& position, const Quaternion& rotation, const Vector3D& scale)
			: mLocalPosition(position)
			, mLocalRotation(rotation)
			, mLocalScale(scale)
			, mParent(nullptr)
		{
		}

		inline Transform3D::Transform3D(const Transform3D& other)
			: mLocalPosition(other.mLocalPosition)
			, mLocalRotation(other.mLocalRotation)
			, mLocalScale(other.mLocalScale)
			, mParent(other.mParent)
		{
		}

		inline Transform3D& Transform3D::operator=(const Transform3D& other)
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

		inline const Vector3D& Transform3D::GetLocalPosition() const
		{
			return mLocalPosition;
		}

		inline const Quaternion& Transform3D::GetLocalRotation() const
		{
			return mLocalRotation;
		}

		inline const Vector3D& Transform3D::GetLocalScale() const
		{
			return mLocalScale;
		}

		inline void Transform3D::SetLocalPosition(const Vector3D& position)
		{
			mLocalPosition = position;
		}

		inline void Transform3D::SetLocalRotation(const Quaternion& rotation)
		{
			mLocalRotation = rotation;
		}

		inline void Transform3D::SetLocalRotation(const Angle& yaw, const Angle& pitch, const Angle& roll)
		{
			mLocalRotation = Quaternion::FromEuler(yaw, pitch, roll);
		}

		inline void Transform3D::SetLocalScale(const Vector3D& scale)
		{
			mLocalScale = scale;
		}

		inline void Transform3D::SetLocalScale(float uniformScale)
		{
			mLocalScale.Set(uniformScale, uniformScale, uniformScale);
		}

		//-----------------------------------------------------------------------------
		// Hierarchy
		//-----------------------------------------------------------------------------

		inline Transform3D* Transform3D::GetParent() const
		{
			return mParent;
		}

		inline bool Transform3D::HasParent() const
		{
			return mParent != nullptr;
		}

		//-----------------------------------------------------------------------------
		// Transformations
		//-----------------------------------------------------------------------------

		inline void Transform3D::Translate(const Vector3D& delta)
		{
			// Rotate delta by local rotation before adding
			mLocalPosition += mLocalRotation.Rotate(delta);
		}

		inline void Transform3D::TranslateWorld(const Vector3D& delta)
		{
			// Add delta directly in world space
			mLocalPosition += delta;
		}

		inline void Transform3D::Rotate(const Quaternion& delta)
		{
			// Apply delta first (delta * mLocalRotation) per AC 11
			mLocalRotation = delta * mLocalRotation;
		}

		inline void Transform3D::Scale(const Vector3D& scale)
		{
			mLocalScale.x *= scale.x;
			mLocalScale.y *= scale.y;
			mLocalScale.z *= scale.z;
		}

		inline void Transform3D::Scale(float uniformScale)
		{
			mLocalScale.x *= uniformScale;
			mLocalScale.y *= uniformScale;
			mLocalScale.z *= uniformScale;
		}

		//-----------------------------------------------------------------------------
		// Utility Functions
		//-----------------------------------------------------------------------------

		inline Vector3D Transform3D::GetForward() const
		{
			// Forward is -Z axis in Y-up RH (AC 18), in world space
			return GetWorldRotation().Rotate(-Vector3D::ZAxis());
		}

		inline Vector3D Transform3D::GetRight() const
		{
			// Right is +X axis in Y-up RH (AC 18), in world space
			return GetWorldRotation().Rotate(Vector3D::XAxis());
		}

		inline Vector3D Transform3D::GetUp() const
		{
			// Up is +Y axis in Y-up RH (AC 18), in world space
			return GetWorldRotation().Rotate(Vector3D::YAxis());
		}
	}
}
