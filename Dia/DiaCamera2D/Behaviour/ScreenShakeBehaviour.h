////////////////////////////////////////////////////////////////////////////////
// Filename: ScreenShakeBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Additive trauma-based screen shake. Call Trigger(trauma) to add shake.
		/// Trauma decays each frame. Magnitude = trauma^2 * maxOffset.
		/// Attach last so shake is additive on top of all other behaviours.
		class ScreenShakeBehaviour : public ICameraBehaviour
		{
		public:
			explicit ScreenShakeBehaviour(float maxOffset = 20.0f, float traumaDecay = 1.5f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			/// Add trauma in [0,1]. Clamped to 1. Multiple calls accumulate.
			void Trigger(float trauma);

			float GetTrauma() const { return mTrauma; }

			static constexpr const char* kTypeIdStr = "ScreenShakeBehaviour";

		private:
			float mTrauma      = 0.0f;
			float mMaxOffset   = 20.0f;
			float mTraumaDecay = 1.5f;  ///< Trauma units lost per second
			float mTime        = 0.0f;  ///< Used for noise offset
		};

	} // namespace Camera2D
} // namespace Dia
