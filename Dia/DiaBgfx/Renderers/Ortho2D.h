////////////////////////////////////////////////////////////////////////////////
// Filename: Ortho2D.h
// Shared 2D camera -> ortho matrix helper (no bgfx types). Used by DebugRenderer and
// CellFieldRenderer so both draw through exactly the same view transform.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace Bgfx
    {
        // Column-major 4x4 ortho matrix identical to the one DebugRenderer::Draw has always built:
        //   bx::mtxOrtho(m, cx-halfW, cx+halfW, cy+halfH, cy-halfH, 0, 1000, 0, homogeneousDepth)
        // with zoom <= 0 treated as 1, halfW = canvas.x*0.5/zoom, halfH = canvas.y*0.5/zoom.
        // World +Y points down the screen. Camera rotation is not applied (as before).
        // Pass bgfx::getCaps()->homogeneousDepth for homogeneousDepth in the real render path.
        void BuildOrtho2D(const Dia::Graphics::Camera2D& camera, const Dia::Maths::Vector2D& canvasSize,
                          float outMatrix[16], bool homogeneousDepth = false);

        // Transforms a world point by `matrix` (from BuildOrtho2D) and maps NDC to pixels:
        // x right, y DOWN, origin top-left of the canvas (the view rect is 0,0,w,h).
        Dia::Maths::Vector2D MatrixWorldToPixel(const float matrix[16], const Dia::Maths::Vector2D& canvasSize,
                                                const Dia::Maths::Vector2D& worldPoint);

        // BuildOrtho2D + MatrixWorldToPixel.
        Dia::Maths::Vector2D WorldToPixel(const Dia::Graphics::Camera2D& camera, const Dia::Maths::Vector2D& canvasSize,
                                          const Dia::Maths::Vector2D& worldPoint);

    } // namespace Bgfx
} // namespace Dia
