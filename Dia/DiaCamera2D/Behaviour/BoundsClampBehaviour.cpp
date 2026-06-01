////////////////////////////////////////////////////////////////////////////////
// Filename: BoundsClampBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/BoundsClampBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(BoundsClampBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new BoundsClampBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		BoundsClampBehaviour::BoundsClampBehaviour(const Dia::Geometry2D::AARect& bounds,
		                                            const Dia::Maths::Vector2D& windowSize)
			: mBounds(bounds)
			, mWindowSize(windowSize)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC BoundsClampBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void BoundsClampBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			// No-op if bounds are zero-area (unbounded world)
			const Dia::Maths::Vector2D bl = mBounds.GetBottomLeft();
			const Dia::Maths::Vector2D tr = mBounds.GetTopRight();
			const float worldW = tr.x - bl.x;
			const float worldH = tr.y - bl.y;

			if (worldW <= 0.0f || worldH <= 0.0f)
				return;

			const float zoom    = camera.GetZoom() > 0.0f ? camera.GetZoom() : 1.0f;
			const float halfW   = (mWindowSize.x / zoom) * 0.5f;
			const float halfH   = (mWindowSize.y / zoom) * 0.5f;

			const float minX = bl.x + halfW;
			const float maxX = tr.x - halfW;
			const float minY = bl.y + halfH;
			const float maxY = tr.y - halfH;

			Dia::Maths::Vector2D pos = camera.GetPosition();

			// Only clamp if the world is larger than the viewport
			if (maxX > minX) pos.x = pos.x < minX ? minX : (pos.x > maxX ? maxX : pos.x);
			if (maxY > minY) pos.y = pos.y < minY ? minY : (pos.y > maxY ? maxY : pos.y);

			camera.SetPosition(pos);
		}

	} // namespace Camera2D
} // namespace Dia
