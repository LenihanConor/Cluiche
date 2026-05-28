#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Line.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

struct IntersectionPair
{
    enum class Kind { CircleCircle, CircleAARect, AARectTriangle, RayCircle, LineAARect, CircleTriangle };

    Kind kind;
    bool hit;

    Dia::Geometry2D::Circle   circleA;
    Dia::Geometry2D::Circle   circleB;
    Dia::Geometry2D::AARect   aaRect;
    Dia::Geometry2D::Triangle triangle;
    Dia::Geometry2D::Ray      ray;
    Dia::Geometry2D::Line     line;
};

class Geometry2DIntersectionsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr int kMaxPairs = 6;

    Geometry2DIntersectionsDrawer(
        const Dia::Core::Containers::DynamicArrayC<IntersectionPair, kMaxPairs>& pairs,
        const Dia::Debug::DebugLayerManager& mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Core::Containers::DynamicArrayC<IntersectionPair, kMaxPairs>& mPairs;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
