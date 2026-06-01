////////////////////////////////////////////////////////////////////////////////
// Filename: ZoomBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Scales camera zoom by a scroll delta input. Clamps to [minZoom, maxZoom].
		/// Call SetInput() from application scroll input code.
		class ZoomBehaviour : public ICameraBehaviour
		{
		public:
			explicit ZoomBehaviour(float sensitivity = 0.1f,
			                       float minZoom     = 0.25f,
			                       float maxZoom     = 4.0f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			/// Set scroll input. Positive = zoom in, negative = zoom out.
			void SetInput(float scrollDelta) { mScrollDelta = scrollDelta; }
			void SetSensitivity(float s)     { mSensitivity = s; }
			void SetRange(float minZ, float maxZ) { mMinZoom = minZ; mMaxZoom = maxZ; }

			static constexpr const char* kTypeIdStr = "ZoomBehaviour";

		private:
			float mScrollDelta  = 0.0f;
			float mSensitivity  = 0.1f;
			float mMinZoom      = 0.25f;
			float mMaxZoom      = 4.0f;
		};

	} // namespace Camera2D
} // namespace Dia
