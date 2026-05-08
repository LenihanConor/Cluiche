////////////////////////////////////////////////////////////////////////////////
// Filename: ShapeDrawer.h
// Description: IVisualDebugger that draws individual Geometry2D shapes submitted
//              per-frame by the caller. Clears its pending buffer at the start
//              of each Draw() call — callers must re-submit each frame.
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGraphics/Misc/RGBA.h>

// Geometry2D shape forward declarations
namespace Dia::Geometry2D
{
    class Circle;
    class AARect;
    class Line;
    class Ray;
    class Triangle;
    class ConvexPolygon;
}

namespace Dia::Debug  { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// ShapeDrawer
//
// Accepts geometry shapes submitted one-at-a-time via Submit*() before each
// Draw() call. Draw() flushes the buffer and then clears it.
//
// Layer: LayerNames::kGeoShapes   Priority: 10
////////////////////////////////////////////////////////////////////////////////
class ShapeDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kMaxShapes = 128; // ~16 KB stack budget (ConvexPolygon is largest)

    explicit ShapeDrawer(const Dia::Debug::DebugLayerManager& manager);

    // IVisualDebugger
    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

    // Submit shapes before Draw() each frame. Drops silently if buffer full.
    void SubmitCircle    (const Dia::Geometry2D::Circle&       shape, Dia::Graphics::RGBA colour);
    void SubmitAARect    (const Dia::Geometry2D::AARect&        shape, Dia::Graphics::RGBA colour);
    void SubmitLine      (const Dia::Geometry2D::Line&          shape, Dia::Graphics::RGBA colour);
    void SubmitRay       (const Dia::Geometry2D::Ray&           shape, float displayLength, Dia::Graphics::RGBA colour);
    void SubmitTriangle  (const Dia::Geometry2D::Triangle&      shape, Dia::Graphics::RGBA colour);
    void SubmitConvexPoly(const Dia::Geometry2D::ConvexPolygon& shape, Dia::Graphics::RGBA colour);

private:
    enum class ShapeType
    {
        Circle,
        AARect,
        Line,
        Ray,
        Triangle,
        ConvexPoly
    };

    struct CircleData
    {
        float cx, cy, radius;
    };
    struct AARectData
    {
        float minX, minY, maxX, maxY;
    };
    struct LineData
    {
        float x1, y1, x2, y2;
    };
    struct RayData
    {
        float ox, oy, dx, dy, displayLength;
    };
    struct TriangleData
    {
        float x0, y0, x1, y1, x2, y2;
    };
    struct ConvexPolyData
    {
        static constexpr int kMaxVerts = 16; // matches ConvexPolygon::kMaxVertices
        float vx[kMaxVerts];
        float vy[kMaxVerts];
        int   count;
    };

    struct ShapeEntry
    {
        ShapeType          type;
        Dia::Graphics::RGBA colour;
        union
        {
            CircleData     circle;
            AARectData     aarect;
            LineData       line;
            RayData        ray;
            TriangleData   triangle;
            ConvexPolyData poly;
        };
    };

    Dia::Core::Containers::DynamicArrayC<ShapeEntry, kMaxShapes> mPending;
    const Dia::Debug::DebugLayerManager&                         mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
