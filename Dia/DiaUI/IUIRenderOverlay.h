////////////////////////////////////////////////////////////////////////////////
// Filename: IUIRenderOverlay.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace UI
	{
		class UIDataBuffer;

		/// Renderer-agnostic UI overlay surface.
		/// A renderer module implements this to composite a per-frame UIDataBuffer
		/// over its backbuffer. Called once per frame from EndFrame after entity
		/// and debug passes.
		/// Thread model: created and called on the render thread only.
		class IUIRenderOverlay
		{
		public:
			virtual ~IUIRenderOverlay() = default;

			/// Resize internal overlay surface to match the canvas size.
			/// Called on initial setup and on window resize.
			virtual void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) = 0;

			/// Composite the UI buffer over the current backbuffer.
			/// If buffer.GetBufferSize() == 0, composites the previously-uploaded
			/// overlay (preserves last-drawn UI until next non-empty frame).
			virtual void Composite(const UIDataBuffer& buffer) = 0;

		protected:
			IUIRenderOverlay() = default;

		private:
			IUIRenderOverlay(const IUIRenderOverlay&) = delete;
			IUIRenderOverlay& operator=(const IUIRenderOverlay&) = delete;
		};
	}
}
