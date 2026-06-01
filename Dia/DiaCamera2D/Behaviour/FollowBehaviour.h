////////////////////////////////////////////////////////////////////////////////
// Filename: FollowBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Moves the camera to track a target position with a configurable offset.
		/// Call SetTarget() each frame from application input/gameplay code.
		class FollowBehaviour : public ICameraBehaviour
		{
		public:
			explicit FollowBehaviour(Dia::Maths::Vector2D offset = Dia::Maths::Vector2D(0.0f, 0.0f));

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			void SetTarget(const Dia::Maths::Vector2D& target) { mTarget = target; mHasTarget = true; }
			void SetOffset(const Dia::Maths::Vector2D& offset) { mOffset = offset; }

			static constexpr const char* kTypeIdStr = "FollowBehaviour";

		private:
			Dia::Maths::Vector2D mTarget;
			Dia::Maths::Vector2D mOffset;
			bool                 mHasTarget = false;
		};

	} // namespace Camera2D
} // namespace Dia
