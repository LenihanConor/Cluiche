////////////////////////////////////////////////////////////////////////////////
// Filename: ZoomToFitBehaviour.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Camera2D
	{
		/// Auto-zooms the camera to keep all provided targets in view.
		/// Pads the fit by a configurable margin. Clamps zoom to min/max.
		class ZoomToFitBehaviour : public ICameraBehaviour
		{
		public:
			static const unsigned int kMaxTargets = 8;

			explicit ZoomToFitBehaviour(const Dia::Maths::Vector2D& windowSize,
			                            float margin    = 50.0f,
			                            float minZoom   = 0.5f,
			                            float maxZoom   = 2.0f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera2D& camera, float dt) override;

			void SetTargets(const Dia::Maths::Vector2D* targets, unsigned int count);
			void SetWindowSize(const Dia::Maths::Vector2D& size) { mWindowSize = size; }
			void SetMargin(float margin) { mMargin = margin; }

			static constexpr const char* kTypeIdStr = "ZoomToFitBehaviour";

		private:
			Dia::Maths::Vector2D mTargets[kMaxTargets];
			unsigned int         mTargetCount = 0;
			Dia::Maths::Vector2D mWindowSize;
			float                mMargin  = 50.0f;
			float                mMinZoom = 0.5f;
			float                mMaxZoom = 2.0f;
		};

	} // namespace Camera2D
} // namespace Dia
