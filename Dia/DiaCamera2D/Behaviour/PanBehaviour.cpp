////////////////////////////////////////////////////////////////////////////////
// Filename: PanBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/PanBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(PanBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new PanBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC PanBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void PanBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			const Dia::Maths::Vector2D pos = camera.GetPosition();
			camera.SetPosition(Dia::Maths::Vector2D(pos.x + mDelta.x, pos.y + mDelta.y));
			// Delta is consumed each frame — caller must re-set it next frame
			mDelta = Dia::Maths::Vector2D(0.0f, 0.0f);
		}

	} // namespace Camera2D
} // namespace Dia
