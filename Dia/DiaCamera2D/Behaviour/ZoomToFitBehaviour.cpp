////////////////////////////////////////////////////////////////////////////////
// Filename: ZoomToFitBehaviour.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/ZoomToFitBehaviour.h"
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"
#include "DiaCamera2D/Camera2D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Camera2D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry::Get().Register(
				Dia::Core::StringCRC(ZoomToFitBehaviour::kTypeIdStr),
				[](const void*) -> ICameraBehaviour* {
					return new ZoomToFitBehaviour(Dia::Maths::Vector2D(1400.0f, 1000.0f));
				});
			return true;
		}();

		////////////////////////////////////////////////////////////
		ZoomToFitBehaviour::ZoomToFitBehaviour(const Dia::Maths::Vector2D& windowSize,
		                                        float margin, float minZoom, float maxZoom)
			: mTargetCount(0)
			, mWindowSize(windowSize)
			, mMargin(margin)
			, mMinZoom(minZoom)
			, mMaxZoom(maxZoom)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC ZoomToFitBehaviour::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void ZoomToFitBehaviour::SetTargets(const Dia::Maths::Vector2D* targets, unsigned int count)
		{
			DIA_ASSERT(count <= kMaxTargets, "ZoomToFitBehaviour: too many targets");
			mTargetCount = count < kMaxTargets ? count : kMaxTargets;
			for (unsigned int i = 0; i < mTargetCount; ++i)
				mTargets[i] = targets[i];
		}

		////////////////////////////////////////////////////////////
		void ZoomToFitBehaviour::Update(Camera2D& camera, float /*dt*/)
		{
			if (mTargetCount == 0)
				return;

			// Compute centroid and AABB of targets
			float minX = mTargets[0].x, maxX = mTargets[0].x;
			float minY = mTargets[0].y, maxY = mTargets[0].y;
			float centreX = 0.0f, centreY = 0.0f;

			for (unsigned int i = 0; i < mTargetCount; ++i)
			{
				if (mTargets[i].x < minX) minX = mTargets[i].x;
				if (mTargets[i].x > maxX) maxX = mTargets[i].x;
				if (mTargets[i].y < minY) minY = mTargets[i].y;
				if (mTargets[i].y > maxY) maxY = mTargets[i].y;
				centreX += mTargets[i].x;
				centreY += mTargets[i].y;
			}
			centreX /= static_cast<float>(mTargetCount);
			centreY /= static_cast<float>(mTargetCount);

			camera.SetPosition(Dia::Maths::Vector2D(centreX, centreY));

			// Compute zoom to fit
			const float reqW = (maxX - minX) + mMargin * 2.0f;
			const float reqH = (maxY - minY) + mMargin * 2.0f;

			if (reqW <= 0.0f || reqH <= 0.0f || mWindowSize.x <= 0.0f || mWindowSize.y <= 0.0f)
				return;

			float zoom = mWindowSize.x / reqW < mWindowSize.y / reqH
			           ? mWindowSize.x / reqW
			           : mWindowSize.y / reqH;

			if (zoom < mMinZoom) zoom = mMinZoom;
			if (zoom > mMaxZoom) zoom = mMaxZoom;
			camera.SetZoom(zoom);
		}

	} // namespace Camera2D
} // namespace Dia
