////////////////////////////////////////////////////////////////////////////////
// Filename: BoundsClampBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Clamps the camera position so it never shows beyond the world bounds.
		/// Takes window size into account (camera can't show void at edges).
		/// No-op if bounds are zero-area (unbounded world).
		class BoundsClampBehaviour : public ICameraBehaviour
		{
		public:
			BoundsClampBehaviour() = default;
			explicit BoundsClampBehaviour(const Dia::Geometry2D::AARect& bounds,
			                              const Dia::Maths::Vector2D& windowSize = Dia::Maths::Vector2D(0.0f, 0.0f));

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			void SetBounds(const Dia::Geometry2D::AARect& bounds) { mBounds = bounds; }
			void SetWindowSize(const Dia::Maths::Vector2D& size)  { mWindowSize = size; }

			static constexpr const char* kTypeIdStr = "BoundsClampBehaviour";

		private:
			Dia::Geometry2D::AARect mBounds;       ///< World bounds (zero-area = unbounded)
			Dia::Maths::Vector2D    mWindowSize;   ///< Needed to compute half-viewport offset
		};

	} // namespace Camera2D
} // namespace Dia
