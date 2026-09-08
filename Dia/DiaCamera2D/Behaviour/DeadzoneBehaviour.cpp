////////////////////////////////////////////////////////////////////////////////
// Filename: DeadzoneBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/DeadzoneBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(DeadzoneBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new DeadzoneBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		DeadzoneBehaviour::DeadzoneBehaviour(float halfWidth, float halfHeight)
			: mHalfWidth(halfWidth)
			, mHalfHeight(halfHeight)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC DeadzoneBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void DeadzoneBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			if (!mHasTarget)
				return;

			const Dia::Maths::Vector2D pos = camera.GetPosition();
			const float dx = mTarget.x - pos.x;
			const float dy = mTarget.y - pos.y;

			float newX = pos.x;
			float newY = pos.y;

			if (dx >  mHalfWidth)  newX = mTarget.x - mHalfWidth;
			if (dx < -mHalfWidth)  newX = mTarget.x + mHalfWidth;
			if (dy >  mHalfHeight) newY = mTarget.y - mHalfHeight;
			if (dy < -mHalfHeight) newY = mTarget.y + mHalfHeight;

			camera.SetPosition(Dia::Maths::Vector2D(newX, newY));
		}

	} // namespace Camera2D
} // namespace Dia
