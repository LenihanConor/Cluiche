////////////////////////////////////////////////////////////////////////////////
// Filename: DebugRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGraphics/Camera/Camera2D.h>

namespace Dia { namespace Graphics { class DebugFrameData; } }

namespace Dia
{
    namespace Bgfx
    {
        class ShaderProgram;

        // Consumes DebugFrameData and issues bgfx draw calls for all 8 primitive types.
        // Text2D is rendered via bgfx::dbgTextPrintf (no vertex buffer needed).
        // All other primitives use TransientVertexBuffer; one draw call per primitive.
        class DebugRenderer
        {
        public:
            DebugRenderer(unsigned short viewId, ShaderProgram* debugProgram);
            ~DebugRenderer();

            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);
            void SetCamera(const Dia::Graphics::Camera2D& camera);
            void Draw(const Dia::Graphics::DebugFrameData& debug);

        private:
            unsigned short           mViewId;
            ShaderProgram*           mDebugProgram;  // not owned
            Dia::Maths::Vector2D     mCanvasSize;
            Dia::Graphics::Camera2D  mCamera;

            DebugRenderer(const DebugRenderer&) = delete;
            DebugRenderer& operator=(const DebugRenderer&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
