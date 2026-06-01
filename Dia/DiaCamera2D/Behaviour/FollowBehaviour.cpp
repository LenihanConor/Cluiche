////////////////////////////////////////////////////////////////////////////////
// Filename: FollowBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/FollowBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(FollowBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new FollowBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		FollowBehaviour::FollowBehaviour(Dia::Maths::Vector2D offset)
			: mOffset(offset)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC FollowBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void FollowBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			if (!mHasTarget)
				return;

			camera.SetPosition(Dia::Maths::Vector2D(
				mTarget.x + mOffset.x,
				mTarget.y + mOffset.y));
		}

	} // namespace Camera2D
} // namespace Dia
