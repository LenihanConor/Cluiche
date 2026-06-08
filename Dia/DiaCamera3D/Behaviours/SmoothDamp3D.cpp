////////////////////////////////////////////////////////////////////////////////
// Filename: SmoothDamp3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/SmoothDamp3D.h"
#include "DiaCamera3D/Registry/CameraBehaviourRegistry3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <cmath>

namespace Dia
{
	namespace Camera3D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry3D::Get().Register(
				Dia::Core::StringCRC(SmoothDamp3D::kTypeIdStr),
				[](const void*) -> ICameraBehaviour3D* { return new SmoothDamp3D(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		SmoothDamp3D::SmoothDamp3D(float smoothTime)
			: mSmoothTime(smoothTime)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC SmoothDamp3D::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void SmoothDamp3D::Update(Camera3D& camera, float dt)
		{
			if (!mHasTarget || dt <= 0.0f)
				return;

			// Critically-damped spring approximation (Game Programming Gems vol.4)
			const float omega = 2.0f / (mSmoothTime > 0.0f ? mSmoothTime : 0.001f);
			const float x     = omega * dt;
			const float exp   = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

			const Dia::Maths::Vector3D pos  = camera.position;
			const Dia::Maths::Vector3D diff = Dia::Maths::Vector3D(
				pos.x - mTarget.x, pos.y - mTarget.y, pos.z - mTarget.z);

			const Dia::Maths::Vector3D temp = Dia::Maths::Vector3D(
				(mVelocity.x + omega * diff.x) * dt,
				(mVelocity.y + omega * diff.y) * dt,
				(mVelocity.z + omega * diff.z) * dt);

			mVelocity = Dia::Maths::Vector3D(
				(mVelocity.x - omega * temp.x) * exp,
				(mVelocity.y - omega * temp.y) * exp,
				(mVelocity.z - omega * temp.z) * exp);

			camera.position = Dia::Maths::Vector3D(
				mTarget.x + (diff.x + temp.x) * exp,
				mTarget.y + (diff.y + temp.y) * exp,
				mTarget.z + (diff.z + temp.z) * exp);
		}

	} // namespace Camera3D
} // namespace Dia
