// HexGridDrawer.inl — template implementation (included by HexGridDrawer.h)

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

namespace Dia::Geometry2DVisualDebugger
{

template<typename T, unsigned int MaxObjects>
Dia::Core::StringCRC HexGridDrawer<T, MaxObjects>::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoSpatialGrid;
}

template<typename T, unsigned int MaxObjects>
void HexGridDrawer<T, MaxObjects>::Draw(Dia::Graphics::FrameData& frameData)
{
    if (!IsEnabled()) return;

    const Dia::Graphics::RGBA colour = Dia::Debug::DebugColourPalette::kInactive;

    // Pointy-top hexagon corner angles (radians): 30, 90, 150, 210, 270, 330 degrees
    static constexpr float kAngles[6] =
    {
        Dia::Maths::PI / 6.0f,           // 30 deg
        Dia::Maths::PI / 2.0f,           // 90 deg
        5.0f * Dia::Maths::PI / 6.0f,    // 150 deg
        7.0f * Dia::Maths::PI / 6.0f,    // 210 deg
        3.0f * Dia::Maths::PI / 2.0f,    // 270 deg
        11.0f * Dia::Maths::PI / 6.0f    // 330 deg
    };

    const float hexRadius = mGrid.GetHexRadius();
    const int   minQ      = mGrid.GetMinQ();
    const int   minR      = mGrid.GetMinR();
    const int   colCount  = mGrid.GetColCount();
    const int   rowCount  = mGrid.GetRowCount();

    for (int r = 0; r < rowCount; ++r)
    {
        for (int q = 0; q < colCount; ++q)
        {
            const Dia::Geometry2D::HexCoord coord{ minQ + q, minR + r };
            if (!mGrid.IsValidHex(coord)) continue;

            const Dia::Maths::Vector2D center = mGrid.HexToWorld(coord);

            // Compute 6 corners
            Dia::Maths::Vector2D corners[6];
            for (int k = 0; k < 6; ++k)
            {
                corners[k] = Dia::Maths::Vector2D(
                    center.x + hexRadius * std::cos(kAngles[k]),
                    center.y + hexRadius * std::sin(kAngles[k]));
            }

            // Draw 6 edge lines
            for (int k = 0; k < 6; ++k)
            {
                frameData.RequestDraw(corners[k], corners[(k + 1) % 6], colour);
            }
        }
    }
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
