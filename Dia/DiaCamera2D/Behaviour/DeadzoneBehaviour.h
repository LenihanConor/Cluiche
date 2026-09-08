////////////////////////////////////////////////////////////////////////////////
// Filename: DeadzoneBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Camera doesn't move until the target leaves a central deadzone region.
		/// Useful for platformers where minor player movement shouldn't shift the view.
		class DeadzoneBehaviour : public ICameraBehaviour
		{
		public:
			explicit DeadzoneBehaviour(float halfWidth = 50.0f, float halfHeight = 30.0f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			void SetTarget(const Dia::Maths::Vector2D& target) { mTarget = target; mHasTarget = true; }
			void SetDeadzone(float halfWidth, float halfHeight) { mHalfWidth = halfWidth; mHalfHeight = halfHeight; }

			static constexpr const char* kTypeIdStr = "DeadzoneBehaviour";

		private:
			Dia::Maths::Vector2D mTarget;
			float                mHalfWidth  = 50.0f;
			float                mHalfHeight = 30.0f;
			bool                 mHasTarget  = false;
		};

	} // namespace Camera2D
} // namespace Dia
