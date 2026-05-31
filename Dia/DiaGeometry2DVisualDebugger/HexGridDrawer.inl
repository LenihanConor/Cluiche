// HexGridDrawer.inl — template implementation (included by HexGridDrawer.h)

#ifdef DIA_DEBUG

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace Dia::Geometry2DVisualDebugger
{

template<typename T, unsigned int MaxObjects>
Dia::Core::StringCRC HexGridDrawer<T, MaxObjects>::GetLayerName() const
{
    return Dia::Debug::LayerNames::kGeoHexGrid;
}

template<typename T, unsigned int MaxObjects>
void HexGridDrawer<T, MaxObjects>::Draw(Dia::Graphics::FrameData& frameData)
{
    if (!IsEnabled()) return;

    static const Dia::Graphics::RGBA kSelectFill(100, 180, 255, 60);
    static const Dia::Graphics::RGBA kSelectOutline(100, 180, 255, 200);
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
    const int   colCount  = mGrid.GetColCount();
    const int   rowCount  = mGrid.GetRowCount();

    for (int r = 0; r < rowCount; ++r)
    {
        for (int q = 0; q < colCount; ++q)
        {
            const Dia::Geometry2D::HexCoord coord{ q, r };
            if (!mGrid.IsValidHex(coord)) continue;

            const Dia::Maths::Vector2D center = mGrid.HexToWorld(coord);

            Dia::Maths::Vector2D corners[6];
            for (int k = 0; k < 6; ++k)
            {
                corners[k] = Dia::Maths::Vector2D(
                    center.x + hexRadius * std::cos(kAngles[k]),
                    center.y + hexRadius * std::sin(kAngles[k]));
            }

            const bool selected = (mSelected != nullptr) && (coord == *mSelected);
            const Dia::Graphics::RGBA edgeColour = selected ? kSelectOutline : colour;

            for (int k = 0; k < 6; ++k)
                frameData.RequestDraw(corners[k], corners[(k + 1) % 6], edgeColour);

            // Filled triangle fan for selected hex
            if (selected)
            {
                for (int k = 0; k < 6; ++k)
                    frameData.RequestDraw(center, corners[k], corners[(k + 1) % 6],
                        Dia::Graphics::RGBA(0, 0, 0, 0), kSelectFill);
            }

            if (mShowLabels)
            {
                char label[16];
                std::snprintf(label, sizeof(label), "%d,%d", coord.q, coord.r);
                frameData.RequestDrawText(
                    Dia::Maths::Vector2D(center.x - hexRadius * 0.35f, center.y),
                    label, 10.0f, colour);
            }
        }
    }
}

template<typename T, unsigned int MaxObjects>
void HexGridDrawer<T, MaxObjects>::DrawImGui()
{
    ImGui::Checkbox("coord labels", &mShowLabels);

    // Inspector panel — read-only, selection is owned by the stage module
    if (mSelected != nullptr)
    {
        ImGui::Separator();
        ImGui::Text("hex (%d, %d)", mSelected->q, mSelected->r);

        const Dia::Maths::Vector2D center = mGrid.HexToWorld(*mSelected);
        const float r = mGrid.GetHexRadius();
        ImGui::Text("center  (%.1f, %.1f)  r=%.1f", center.x, center.y, r);

        Dia::Core::Containers::DynamicArrayC<
            Dia::Core::Handle<T>,
            Dia::Geometry2D::kMaxQueryResults> results;
        mGrid.QueryHex(*mSelected, results);

        ImGui::Text("objects  %u", results.Size());
        for (unsigned int i = 0; i < results.Size(); ++i)
            ImGui::Text("  #%u: idx=%u gen=%u", i,
                results[i].GetIndex(), results[i].GetGeneration());
    }
}

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
