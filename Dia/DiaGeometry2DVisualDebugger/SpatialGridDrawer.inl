// SpatialGridDrawer.inl — template implementation (included by SpatialGridDrawer.h)

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia::Geometry2DVisualDebugger
{

template<typename T, unsigned int MaxObjects>
Dia::Core::StringCRC SpatialGridDrawer<T, MaxObjects>::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoSpatialGrid;
}

template<typename T, unsigned int MaxObjects>
void SpatialGridDrawer<T, MaxObjects>::Draw(Dia::Graphics::FrameData& frameData)
{
    if (!IsEnabled()) return;

    const int   countX   = mGrid.GetCellCountX();
    const int   countY   = mGrid.GetCellCountY();
    const float cellSize = mGrid.GetCellSize();
    const Dia::Geometry2D::AARect& worldBounds = mGrid.GetWorldBounds();
    const float blX = worldBounds.GetBottomLeft().x;
    const float blY = worldBounds.GetBottomLeft().y;

    const Dia::Graphics::RGBA colour = Dia::Debug::DebugColourPalette::kInactive;

    for (int cy = 0; cy < countY; ++cy)
    {
        for (int cx = 0; cx < countX; ++cx)
        {
            const float minX = blX + static_cast<float>(cx)     * cellSize;
            const float minY = blY + static_cast<float>(cy)     * cellSize;
            const float maxX = blX + static_cast<float>(cx + 1) * cellSize;
            const float maxY = blY + static_cast<float>(cy + 1) * cellSize;
            frameData.RequestDrawRect(
                Dia::Maths::Vector2D(minX, minY),
                Dia::Maths::Vector2D(maxX, maxY),
                colour);
        }
    }
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
