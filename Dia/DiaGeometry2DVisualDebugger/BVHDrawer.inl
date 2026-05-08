// BVHDrawer.inl — template implementation (included by BVHDrawer.h)

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>

namespace Dia::Geometry2DVisualDebugger
{

template<typename T, unsigned int MaxObjects>
Dia::Core::StringCRC BVHDrawer<T, MaxObjects>::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoBVH;
}

template<typename T, unsigned int MaxObjects>
void BVHDrawer<T, MaxObjects>::Draw(Dia::Graphics::FrameData& frameData)
{
    if (!IsEnabled()) return;
    if (!mBVH.IsBuilt()) return;

    // Depth colour table: cycles through 4 semantic colours
    static const Dia::Graphics::RGBA kDepthColours[4] =
    {
        Dia::Debug::DebugColourPalette::kActive,   // depth 0
        Dia::Debug::DebugColourPalette::kGoal,     // depth 1
        Dia::Debug::DebugColourPalette::kHealthy,  // depth 2
        Dia::Debug::DebugColourPalette::kWarning,  // depth 3
    };

    mBVH.VisitNodes([&](const Dia::Geometry2D::AARect& bounds, int depth)
    {
        const Dia::Graphics::RGBA colour = kDepthColours[depth % 4];
        frameData.RequestDrawRect(bounds.GetBottomLeft(), bounds.GetTopRight(), colour);
    });
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
