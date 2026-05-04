// QuadtreeDrawer.inl — template implementation (included by QuadtreeDrawer.h)

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>

namespace Dia::Geometry2DVisualDebugger
{

template<typename T, unsigned int MaxObjects>
Dia::Core::StringCRC QuadtreeDrawer<T, MaxObjects>::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoQuadtree;
}

template<typename T, unsigned int MaxObjects>
void QuadtreeDrawer<T, MaxObjects>::Draw(Dia::Graphics::FrameData& frameData)
{
    if (!IsEnabled()) return;

    mTree.VisitNodes([&](const Dia::Geometry2D::AARect& bounds, bool isLeaf)
    {
        const Dia::Graphics::RGBA colour = isLeaf
            ? Dia::Debug::DebugColourPalette::kActive
            : Dia::Debug::DebugColourPalette::kInactive;
        frameData.RequestDrawRect(bounds.GetBottomLeft(), bounds.GetTopRight(), colour);
    });
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
