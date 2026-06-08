////////////////////////////////////////////////////////////////////////////////
// Filename: ScreenShake3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"

namespace Dia
{
	namespace Camera3D
	{
		// Trauma-based additive screen shake — affects both position and orientation.
		// Call Trigger(trauma) to add shake. Trauma decays each frame.
		// Attach last so shake is additive on top of all other behaviours.
		class ScreenShake3D : public ICameraBehaviour3D
		{
		public:
			explicit ScreenShake3D(float maxPositionOffset = 0.5f,
			                       float maxAngleOffset    = 0.05f,
			                       float traumaDecay       = 1.5f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera3D& camera, float dt) override;

			// Add trauma in [0,1]. Clamped to 1. Multiple calls accumulate.
			void Trigger(float trauma);

			float GetTrauma() const { return mTrauma; }

			static constexpr const char* kTypeIdStr = "ScreenShake3D";

		private:
			float mTrauma            = 0.0f;
			float mMaxPositionOffset = 0.5f;
			float mMaxAngleOffset    = 0.05f;  // radians
			float mTraumaDecay       = 1.5f;
			float mTime              = 0.0f;
		};

	} // namespace Camera3D
} // namespace Dia
