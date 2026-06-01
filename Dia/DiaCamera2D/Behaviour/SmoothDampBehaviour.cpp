////////////////////////////////////////////////////////////////////////////////
// Filename: SmoothDampBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/SmoothDampBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <cmath>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(SmoothDampBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new SmoothDampBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		SmoothDampBehaviour::SmoothDampBehaviour(float smoothTime)
			: mSmoothTime(smoothTime)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC SmoothDampBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void SmoothDampBehaviour::Update(Camera2D& camera, float dt)
		{
			if (!mHasTarget || dt <= 0.0f)
				return;

			// Critically-damped spring approximation (Game Programming Gems vol.4)
			const float omega = 2.0f / (mSmoothTime > 0.0f ? mSmoothTime : 0.001f);
			const float x     = omega * dt;
			const float exp   = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

			const Dia::Maths::Vector2D pos  = camera.GetPosition();
			const Dia::Maths::Vector2D diff = Dia::Maths::Vector2D(pos.x - mTarget.x, pos.y - mTarget.y);

			const Dia::Maths::Vector2D temp = Dia::Maths::Vector2D(
				(mVelocity.x + omega * diff.x) * dt,
				(mVelocity.y + omega * diff.y) * dt);

			mVelocity = Dia::Maths::Vector2D(
				(mVelocity.x - omega * temp.x) * exp,
				(mVelocity.y - omega * temp.y) * exp);

			camera.SetPosition(Dia::Maths::Vector2D(
				mTarget.x + (diff.x + temp.x) * exp,
				mTarget.y + (diff.y + temp.y) * exp));
		}

	} // namespace Camera2D
} // namespace Dia
