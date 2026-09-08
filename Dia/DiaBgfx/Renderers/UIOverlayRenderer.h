////////////////////////////////////////////////////////////////////////////////
// Filename: UIOverlayRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUIRenderOverlay.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace Bgfx
    {
        class ShaderProgram;

        // Implements IUIRenderOverlay using a bgfx 2D texture and a fullscreen quad.
        // Receives RGBA pixel data from UIDataBuffer and composites it as an overlay.
        class UIOverlayRenderer : public Dia::UI::IUIRenderOverlay
        {
        public:
            UIOverlayRenderer(unsigned short viewId, ShaderProgram* uiProgram);
            ~UIOverlayRenderer() override;

            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) override;
            void Composite(const Dia::UI::UIDataBuffer& buffer) override;

        private:
            unsigned short       mViewId;
            ShaderProgram*       mUiProgram;     // not owned
            unsigned short       mTextureHandle; // bgfx::TextureHandle::idx; kInvalidHandle until first Composite
            Dia::Maths::Vector2D mCanvasSize;

            UIOverlayRenderer(const UIOverlayRenderer&) = delete;
            UIOverlayRenderer& operator=(const UIOverlayRenderer&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
