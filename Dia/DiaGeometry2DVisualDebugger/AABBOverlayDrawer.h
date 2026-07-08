////////////////////////////////////////////////////////////////////////////////
// Filename: AABBOverlayDrawer.h
// Description: IVisualDebugger that draws the AABB of each submitted geometry
//              shape. Submit shapes each frame via Submit*(); Draw() flushes.
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Geometry2D
{
    class Circle;
    class AARect;
    class OORect;
    class Line;
    class Triangle;
    class ConvexPolygon;
    class Capsule;
}

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// AABBOverlayDrawer
//
// Computes and draws the axis-aligned bounding box of each submitted shape.
// Submit before Draw() each frame; buffer is cleared at the start of Draw().
//
// Layer: LayerNames::kGeoAABB   Priority: 15
////////////////////////////////////////////////////////////////////////////////
class AABBOverlayDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kMaxAABBs = 64;

    explicit AABBOverlayDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    void SubmitCircle    (const Dia::Geometry2D::Circle&       shape);
    void SubmitAARect    (const Dia::Geometry2D::AARect&       shape);
    void SubmitOORect    (const Dia::Geometry2D::OORect&       shape);
    void SubmitLine      (const Dia::Geometry2D::Line&         shape);
    void SubmitTriangle  (const Dia::Geometry2D::Triangle&     shape);
    void SubmitConvexPoly(const Dia::Geometry2D::ConvexPolygon& shape);
    void SubmitCapsule   (const Dia::Geometry2D::Capsule&      shape);

private:
    struct AABBEntry { float minX, minY, maxX, maxY; };

    void Submit(float minX, float minY, float maxX, float maxY);

    Dia::Core::Containers::DynamicArrayC<AABBEntry, kMaxAABBs> mPending;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
