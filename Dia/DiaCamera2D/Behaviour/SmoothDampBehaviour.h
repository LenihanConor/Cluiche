////////////////////////////////////////////////////////////////////////////////
// Filename: SmoothDampBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Applies exponential smoothing to the camera position.
		/// Compose after FollowBehaviour to smooth jerky movement.
		class SmoothDampBehaviour : public ICameraBehaviour
		{
		public:
			explicit SmoothDampBehaviour(float smoothTime = 0.15f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			void SetTarget(const Dia::Maths::Vector2D& target) { mTarget = target; mHasTarget = true; }
			void SetSmoothTime(float t) { mSmoothTime = t; }

			static constexpr const char* kTypeIdStr = "SmoothDampBehaviour";

		private:
			Dia::Maths::Vector2D mTarget;
			Dia::Maths::Vector2D mVelocity;
			float                mSmoothTime = 0.15f;
			bool                 mHasTarget  = false;
		};

	} // namespace Camera2D
} // namespace Dia
