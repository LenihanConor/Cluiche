////////////////////////////////////////////////////////////////////////////////
// Filename: DebugGeometry3DRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Graphics { class DebugFrameData; } }
namespace Dia { namespace Graphics3D { struct Camera3D; } }
namespace Dia { namespace Bgfx { class ShaderProgram; } }

namespace Dia
{
    namespace Bgfx3D
    {
        class DebugGeometry3DRenderer
        {
        public:
            DebugGeometry3DRenderer(unsigned short viewId, Dia::Bgfx::ShaderProgram* debugProgram);
            ~DebugGeometry3DRenderer();

            void Draw(const Dia::Graphics::DebugFrameData& debugData,
                      const Dia::Graphics3D::Camera3D& camera,
                      const Dia::Maths::Vector2D& canvasSize);

        private:
            unsigned short            mViewId;
            Dia::Bgfx::ShaderProgram* mDebugProgram;  // not owned

            DebugGeometry3DRenderer(const DebugGeometry3DRenderer&) = delete;
            DebugGeometry3DRenderer& operator=(const DebugGeometry3DRenderer&) = delete;
        };
    }
}
