#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/Sector.h>

namespace CluicheTest {

class Geometry2DLabelsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    Geometry2DLabelsDrawer(
        const Dia::Geometry2D::Circle&        circle,
        const Dia::Geometry2D::AARect&        aaRect,
        const Dia::Geometry2D::OORect&        ooRect,
        const Dia::Geometry2D::Line&          line,
        const Dia::Geometry2D::Ray&           ray,
        const Dia::Geometry2D::Triangle&      triangle,
        const Dia::Geometry2D::Capsule&       capsule,
        const Dia::Geometry2D::ConvexPolygon& convexPoly,
        const Dia::Geometry2D::Sector&        sector);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    const Dia::Geometry2D::Circle&        mCircle;
    const Dia::Geometry2D::AARect&        mAARect;
    const Dia::Geometry2D::OORect&        mOORect;
    const Dia::Geometry2D::Line&          mLine;
    const Dia::Geometry2D::Ray&           mRay;
    const Dia::Geometry2D::Triangle&      mTriangle;
    const Dia::Geometry2D::Capsule&       mCapsule;
    const Dia::Geometry2D::ConvexPolygon& mConvexPoly;
    const Dia::Geometry2D::Sector&        mSector;
    float mFontScale = 1.0f;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
