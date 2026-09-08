////////////////////////////////////////////////////////////////////////////////
// Filename: ZoomBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/ZoomBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(ZoomBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new ZoomBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		ZoomBehaviour::ZoomBehaviour(float sensitivity, float minZoom, float maxZoom)
			: mSensitivity(sensitivity)
			, mMinZoom(minZoom)
			, mMaxZoom(maxZoom)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC ZoomBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void ZoomBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			if (mScrollDelta == 0.0f)
				return;

			float zoom = camera.GetZoom() * (1.0f + mScrollDelta * mSensitivity);
			if (zoom < mMinZoom) zoom = mMinZoom;
			if (zoom > mMaxZoom) zoom = mMaxZoom;
			camera.SetZoom(zoom);

			// Input consumed each frame — caller must re-set
			mScrollDelta = 0.0f;
		}

	} // namespace Camera2D
} // namespace Dia
