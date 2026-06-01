////////////////////////////////////////////////////////////////////////////////
// Filename: ScreenShakeBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/ScreenShakeBehaviour.h"
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
				Dia::Core::StringCRC(ScreenShakeBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* { return new ScreenShakeBehaviour(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		ScreenShakeBehaviour::ScreenShakeBehaviour(float maxOffset, float traumaDecay)
			: mMaxOffset(maxOffset)
			, mTraumaDecay(traumaDecay)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC ScreenShakeBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void ScreenShakeBehaviour::Trigger(float trauma)
		{
			mTrauma += trauma;
			if (mTrauma > 1.0f) mTrauma = 1.0f;
		}

		////////////////////////////////////////////////////////////
		void ScreenShakeBehaviour::Update(Camera2D& camera, float dt)
		{
			if (mTrauma <= 0.0f)
				return;

			mTime += dt;

			// Magnitude = trauma^2 for smoother feel
			const float magnitude = mTrauma * mTrauma * mMaxOffset;

			// Pseudo-random offset using sin with incommensurate frequencies
			const float offsetX = magnitude * sinf(mTime * 37.0f);
			const float offsetY = magnitude * sinf(mTime * 53.0f);

			const Dia::Maths::Vector2D pos = camera.GetPosition();
			camera.SetPosition(Dia::Maths::Vector2D(pos.x + offsetX, pos.y + offsetY));

			mTrauma -= mTraumaDecay * dt;
			if (mTrauma < 0.0f) { mTrauma = 0.0f; mTime = 0.0f; }
		}

	} // namespace Camera2D
} // namespace Dia
