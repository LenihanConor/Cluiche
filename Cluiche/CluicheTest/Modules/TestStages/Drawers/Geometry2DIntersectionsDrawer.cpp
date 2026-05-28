#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DIntersectionsDrawer.h"

#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kHitColour(220, 50, 50, 255);
static const Dia::Graphics::RGBA kMissColour(50, 200, 50, 255);
static constexpr float kRayLen = 60.0f;

Geometry2DIntersectionsDrawer::Geometry2DIntersectionsDrawer(
    const Dia::Core::Containers::DynamicArrayC<IntersectionPair, kMaxPairs>& pairs,
    const Dia::Debug::DebugLayerManager& mgr)
    : mPairs(pairs), mManager(mgr)
{}

Dia::Core::StringCRC Geometry2DIntersectionsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geo2d.intersections");
}

void Geometry2DIntersectionsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    for (unsigned int i = 0; i < mPairs.Size(); ++i)
    {
        const IntersectionPair& pair = mPairs[i];
        const Dia::Graphics::RGBA colour = pair.hit ? kHitColour : kMissColour;

        switch (pair.kind)
        {
        case IntersectionPair::Kind::CircleCircle:
            drawer.SubmitCircle(pair.circleA, colour);
            drawer.SubmitCircle(pair.circleB, colour);
            break;
        case IntersectionPair::Kind::CircleAARect:
            drawer.SubmitCircle(pair.circleA, colour);
            drawer.SubmitAARect(pair.aaRect,  colour);
            break;
        case IntersectionPair::Kind::AARectTriangle:
            drawer.SubmitAARect   (pair.aaRect,   colour);
            drawer.SubmitTriangle (pair.triangle, colour);
            break;
        case IntersectionPair::Kind::RayCircle:
            drawer.SubmitRay   (pair.ray,    kRayLen, colour);
            drawer.SubmitCircle(pair.circleA, colour);
            break;
        case IntersectionPair::Kind::LineAARect:
            drawer.SubmitLine  (pair.line,   colour);
            drawer.SubmitAARect(pair.aaRect, colour);
            break;
        case IntersectionPair::Kind::CircleTriangle:
            drawer.SubmitCircle  (pair.circleA,  colour);
            drawer.SubmitTriangle(pair.triangle, colour);
            break;
        }
    }

    drawer.Draw(frameData);
}

} // namespace CluicheTest

#endif // DIA_DEBUG


