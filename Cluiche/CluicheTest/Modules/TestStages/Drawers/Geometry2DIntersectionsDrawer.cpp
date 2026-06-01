#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Geometry2DIntersectionsDrawer.h"

#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kHitColour(220, 50, 50, 255);
static const Dia::Graphics::RGBA kMissColour(50, 200, 50, 255);
static const Dia::Graphics::RGBA kLabelColour(180, 180, 200, 255);
static constexpr float kRayLen = 1.2f;  // *debugScale(50) = 60 world units
static constexpr float kLabelFontSize = 12.0f;
static constexpr float kLabelOffsetY = 35.0f;

static const char* KindLabel(IntersectionPair::Kind kind)
{
    switch (kind)
    {
    case IntersectionPair::Kind::CircleCircle:   return "Circle/Circle";
    case IntersectionPair::Kind::CircleAARect:   return "Circle/AARect";
    case IntersectionPair::Kind::AARectTriangle: return "AARect/Triangle";
    case IntersectionPair::Kind::RayCircle:      return "Ray/Circle";
    case IntersectionPair::Kind::LineAARect:     return "Line/AARect";
    case IntersectionPair::Kind::CircleTriangle: return "Circle/Triangle";
    }
    return "";
}

Geometry2DIntersectionsDrawer::Geometry2DIntersectionsDrawer(
    const Dia::Core::Containers::DynamicArrayC<IntersectionPair, kMaxPairs>& pairs,
    const Dia::Debug::DebugLayerManager& mgr)
    : mPairs(pairs), mManager(mgr)
{}

Dia::Core::StringCRC Geometry2DIntersectionsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("geometry2d.intersections");
}

#pragma warning(push)
#pragma warning(disable: 6262)  // ShapeDrawer::mPending is intentionally stack-allocated
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

    // Draw labels below each pair
    // Use the intersection band Y from the module setup (kY = 0.0f, kStartX = -300, kSpacing = 120)
    constexpr float kBandY = 0.0f;
    constexpr float kStartX = -300.0f;
    constexpr float kSpacing = 120.0f;

    for (unsigned int i = 0; i < mPairs.Size(); ++i)
    {
        const IntersectionPair& pair = mPairs[i];
        const float px = kStartX + kSpacing * static_cast<float>(i);
        const Dia::Graphics::RGBA hitMissColour = pair.hit ? kHitColour : kMissColour;

        frameData.RequestDrawText(
            Dia::Maths::Vector2D(px, kBandY + kLabelOffsetY),
            pair.hit ? "HIT" : "MISS", kLabelFontSize, hitMissColour);

        frameData.RequestDrawText(
            Dia::Maths::Vector2D(px, kBandY + kLabelOffsetY + 16.0f),
            KindLabel(pair.kind), kLabelFontSize - 2.0f, kLabelColour);
    }
}
#pragma warning(pop)

} // namespace CluicheTest

#endif // DIA_DEBUG


