////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGraphics/Camera/Camera2D.h>

namespace Dia { namespace Graphics { class EntityFrameData; } }

namespace Dia
{
    namespace Bgfx
    {
        class ShaderProgram;

        // Consumes EntityFrameData and issues bgfx draw calls for sprite batches.
        // Uses TransientVertexBuffer; batches by ITexture* key (one draw call per unique texture).
        // No visitor pattern (RB-005).
        class SpriteRenderer
        {
        public:
            SpriteRenderer(unsigned short viewId, ShaderProgram* spriteProgram);
            ~SpriteRenderer();

            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);
            void SetCamera(const Dia::Graphics::Camera2D& camera);
            void Draw(const Dia::Graphics::EntityFrameData& sprites);

        private:
            unsigned short           mViewId;
            ShaderProgram*           mSpriteProgram;   // not owned
            Dia::Maths::Vector2D     mCanvasSize;
            Dia::Graphics::Camera2D  mCamera;

            SpriteRenderer(const SpriteRenderer&) = delete;
            SpriteRenderer& operator=(const SpriteRenderer&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
