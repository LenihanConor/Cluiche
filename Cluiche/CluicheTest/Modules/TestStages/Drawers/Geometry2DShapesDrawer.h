#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/Sector.h>
#include <DiaGeometry2D/Shapes/Spline.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class Geometry2DShapesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    Geometry2DShapesDrawer(
        const Dia::Geometry2D::Circle&        circle,
        const Dia::Geometry2D::AARect&        aaRect,
        const Dia::Geometry2D::OORect&        ooRect,
        const Dia::Geometry2D::Line&          line,
        const Dia::Geometry2D::Ray&           ray,
        const Dia::Geometry2D::Triangle&      triangle,
        const Dia::Geometry2D::Capsule&       capsule,
        const Dia::Geometry2D::ConvexPolygon& convexPoly,
        const Dia::Geometry2D::Sector&        sector,
        const Dia::Geometry2D::Spline&        spline,
        const Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 24>& spatialScatter,
        const Dia::Debug::DebugLayerManager&  mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    bool mExtendedRay = false;
    const Dia::Geometry2D::Circle&        mCircle;
    const Dia::Geometry2D::AARect&        mAARect;
    const Dia::Geometry2D::OORect&        mOORect;
    const Dia::Geometry2D::Line&          mLine;
    const Dia::Geometry2D::Ray&           mRay;
    const Dia::Geometry2D::Triangle&      mTriangle;
    const Dia::Geometry2D::Capsule&       mCapsule;
    const Dia::Geometry2D::ConvexPolygon& mConvexPoly;
    const Dia::Geometry2D::Sector&        mSector;
    const Dia::Geometry2D::Spline&        mSpline;
    const Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::AARect, 24>& mSpatialScatter;
    const Dia::Debug::DebugLayerManager&  mManager;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
