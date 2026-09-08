////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DebugFrameData.h"
#include "UIFrameData.h"
#include "EntityFrameData.h"
#include <DiaCamera2D/Camera2D.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// Frame - A single frame that stores all the info that the render
		///
		class FrameData: public DebugFrameData, public UIFrameData, public EntityFrameData
		{
		public:
			// TODO - UI SHOULD BE SAVE AT A DIFFERENT FRAME RATE AS I DONT WANT IT BEING PLAYED BACK
			FrameData();

			FrameData& operator=(const FrameData& rhs);

			void Clear();
			void Copy(const FrameData& rhs);

			const Dia::Camera2D::Camera2D& GetCamera() const { return mCamera; }
			void SetCamera(const Dia::Camera2D::Camera2D& camera) { mCamera = camera; }

			const Dia::Maths::Vector2D& GetWindowSize() const { return mWindowSize; }
			void SetWindowSize(const Dia::Maths::Vector2D& size) { mWindowSize = size; }

			const Dia::Maths::Vector2D& GetMousePixel() const override { return mMousePixel; }
			void SetMousePixel(const Dia::Maths::Vector2D& pixel) { mMousePixel = pixel; }

		private:
			Dia::Camera2D::Camera2D mCamera;
			Dia::Maths::Vector2D mWindowSize;
			Dia::Maths::Vector2D mMousePixel;
		};
	}
}