////////////////////////////////////////////////////////////////////////////////
// Filename: PanBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Moves the camera by a world-space delta each frame.
		/// Call SetDelta() from application input code.
		class PanBehaviour : public ICameraBehaviour
		{
		public:
			PanBehaviour() = default;

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			/// Set the world-space movement to apply this frame.
			void SetDelta(const Dia::Maths::Vector2D& delta) { mDelta = delta; }

			static constexpr const char* kTypeIdStr = "PanBehaviour";

		private:
			Dia::Maths::Vector2D mDelta;
		};

	} // namespace Camera2D
} // namespace Dia
