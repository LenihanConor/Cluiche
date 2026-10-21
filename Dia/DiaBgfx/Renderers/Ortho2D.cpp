////////////////////////////////////////////////////////////////////////////////
// Filename: Ortho2D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/Ortho2D.h"

#include <bx/math.h>

namespace Dia
{
    namespace Bgfx
    {
        void BuildOrtho2D(const Dia::Graphics::Camera2D& camera, const Dia::Maths::Vector2D& canvasSize,
                          float outMatrix[16], bool homogeneousDepth)
        {
            const float cw    = canvasSize.X();
            const float ch    = canvasSize.Y();
            const float zoom  = camera.GetZoom() > 0.0f ? camera.GetZoom() : 1.0f;
            const float halfW = (cw * 0.5f) / zoom;
            const float halfH = (ch * 0.5f) / zoom;
            const float cx    = camera.GetPosition().X();
            const float cy    = camera.GetPosition().Y();

            // Same call DebugRenderer used before the helper was factored out (left-handed, offset 0).
            bx::mtxOrtho(outMatrix, cx - halfW, cx + halfW, cy + halfH, cy - halfH,
                         0.0f, 1000.0f, 0.0f, homogeneousDepth);
        }

        Dia::Maths::Vector2D MatrixWorldToPixel(const float m[16], const Dia::Maths::Vector2D& canvasSize,
                                                const Dia::Maths::Vector2D& worldPoint)
        {
            const float ndcX = m[0] * worldPoint.X() + m[12];
            const float ndcY = m[5] * worldPoint.Y() + m[13];
            return Dia::Maths::Vector2D((ndcX * 0.5f + 0.5f) * canvasSize.X(),
                                        (1.0f - (ndcY * 0.5f + 0.5f)) * canvasSize.Y());
        }

        Dia::Maths::Vector2D WorldToPixel(const Dia::Graphics::Camera2D& camera, const Dia::Maths::Vector2D& canvasSize,
                                          const Dia::Maths::Vector2D& worldPoint)
        {
            float m[16];
            BuildOrtho2D(camera, canvasSize, m);
            return MatrixWorldToPixel(m, canvasSize, worldPoint);
        }

    } // namespace Bgfx
} // namespace Dia
