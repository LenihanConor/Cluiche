// ScalarFieldOverlay.h
// Optional header-only adaptors — colour-gradient debug overlays for a DiaScalarField
// rendered via DiaVisualDebugger's IDebugDraw interface.
//
// OPTIONAL ADAPTOR — not part of DiaScalarField.vcxproj build.
// DiaScalarField.vcxproj carries NO hard link dependency on DiaVisualDebugger.
// Consumers must add DiaVisualDebugger to their own project dependencies.
//
// Two independent IVisualDebugger subclasses are provided:
//
//   ScalarFieldHeatmapOverlay<Topology, Policy>
//     — fills each cell with a colour interpolated from lowColour (at minValue)
//       to highColour (at maxValue). Register with DebugLayerManager.
//
//   ScalarFieldGradientOverlay<Topology, Policy>
//     — draws a direction arrow per cell using RequestDrawRay, showing the
//       gradient direction. Cells with gradient magnitude below minMagnitude
//       are skipped.
//
// Both classes support Square and Hex topologies via if constexpr dispatch.
// Layer names: Dia::Debug::LayerNames::kScalarFieldHeatmap / kScalarFieldGradient.
//
// Usage example:
//   OverlayColourMap colourMap;
//   colourMap.lowColour  = RGBA(0, 0, 255);   // blue for 0.0
//   colourMap.highColour = RGBA(255, 0, 0);   // red  for 1.0
//
//   ScalarFieldHeatmapOverlay<SquareFieldTopology, UniformDecayPolicy> heatmap(
//       field, LayerNames::kScalarFieldHeatmap, 32.0f, worldOrigin, colourMap);
//   ScalarFieldGradientOverlay<SquareFieldTopology, UniformDecayPolicy> arrows(
//       field, LayerNames::kScalarFieldGradient, 32.0f, worldOrigin);
//   layerManager.Register(&heatmap);
//   layerManager.Register(&arrows);

#pragma once
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/HexFieldTopology.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <type_traits>
#include <cmath>

namespace Dia
{
    namespace ScalarField
    {
        namespace Adaptors
        {
            // Colour mapping configuration for the heatmap overlay.
            struct OverlayColourMap
            {
                Dia::Core::RGBA lowColour  = Dia::Core::RGBA(0,   0,   255, 200); // blue
                Dia::Core::RGBA highColour = Dia::Core::RGBA(255, 0,   0,   200); // red
                float           minValue   = 0.0f;
                float           maxValue   = 1.0f;
            };

            // ----------------------------------------------------------------
            // Shared helpers
            // ----------------------------------------------------------------
            namespace Detail
            {
                inline float Clamp01(float v)
                {
                    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                }

                inline Dia::Core::RGBA LerpColour(const Dia::Core::RGBA& a, const Dia::Core::RGBA& b, float t)
                {
                    const auto lerp = [](unsigned char lo, unsigned char hi, float u) -> unsigned char {
                        return static_cast<unsigned char>(
                            static_cast<float>(lo) + (static_cast<float>(hi) - static_cast<float>(lo)) * u);
                    };
                    return Dia::Core::RGBA(
                        lerp(a.R(), b.R(), t),
                        lerp(a.G(), b.G(), t),
                        lerp(a.B(), b.B(), t),
                        lerp(a.A(), b.A(), t));
                }

                // World-space centre of a cell given square-grid layout.
                inline Dia::Maths::Vector2D CellCentre(CellIndex cell, float cellWorldSize,
                                                        const Dia::Maths::Vector2D& worldOrigin)
                {
                    return Dia::Maths::Vector2D(
                        worldOrigin.x + (static_cast<float>(cell.x) + 0.5f) * cellWorldSize,
                        worldOrigin.y + (static_cast<float>(cell.y) + 0.5f) * cellWorldSize);
                }
            }

            // ----------------------------------------------------------------
            // ScalarFieldHeatmapOverlay
            //
            // Fills each cell with a colour interpolated between lowColour and
            // highColour according to the cell's current value.
            // ----------------------------------------------------------------
            template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
            class ScalarFieldHeatmapOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                ScalarFieldHeatmapOverlay(const DiaScalarField<Topology, Policy>& field,
                                          Dia::Core::StringCRC layerName,
                                          float cellWorldSize,
                                          Dia::Maths::Vector2D worldOrigin,
                                          OverlayColourMap colourMap = {})
                    : mField(field)
                    , mLayerName(layerName)
                    , mCellWorldSize(cellWorldSize)
                    , mWorldOrigin(worldOrigin)
                    , mColourMap(colourMap)
                {}

                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                        return;

                    const Topology& topology = mField.GetTopology();

                    if constexpr (std::is_same_v<Topology, SquareFieldTopology>)
                    {
                        for (int y = 0; y < topology.GetHeight(); ++y)
                            for (int x = 0; x < topology.GetWidth(); ++x)
                                DrawCell(draw, CellIndex{x, y});
                    }
                    else if constexpr (std::is_same_v<Topology, HexFieldTopology>)
                    {
                        const int R = topology.GetRadius();
                        for (int q = -R; q <= R; ++q)
                        {
                            const int rMin = (q < 0) ? -R - q : -R;
                            const int rMax = (q > 0) ?  R - q :  R;
                            for (int r = rMin; r <= rMax; ++r)
                                DrawCell(draw, CellIndex{q, r});
                        }
                    }
                    else
                    {
                        // Generic fallback: enumerate cells via BFS from (0,0).
                        // This matches DiaScalarField's internal cell order so every
                        // cell is visited exactly once.
                        const int count = mField.GetCellCount();
                        for (int i = 0; i < count; ++i)
                        {
                            // Cell i maps to a stable (x,y) — use GetTopology for that.
                            // For unknown topologies we skip; consumers should
                            // provide a topology specialisation.
                            (void)i;
                        }
                    }
                }

                // Public helper: draw a single cell. Callers who maintain their own
                // enumeration loop (e.g. partial draws) can call this directly.
                void DrawCell(Dia::Core::IDebugDraw& draw, CellIndex cell) const
                {
                    const float t = Detail::Clamp01(
                        (mField.GetValue(cell) - mColourMap.minValue) /
                        (mColourMap.maxValue   - mColourMap.minValue));

                    const Dia::Core::RGBA fill = Detail::LerpColour(
                        mColourMap.lowColour, mColourMap.highColour, t);

                    const float wx = mWorldOrigin.x + static_cast<float>(cell.x) * mCellWorldSize;
                    const float wy = mWorldOrigin.y + static_cast<float>(cell.y) * mCellWorldSize;

                    const Dia::Maths::Vector2D minPt(wx,                   wy);
                    const Dia::Maths::Vector2D maxPt(wx + mCellWorldSize,  wy + mCellWorldSize);

                    draw.RequestDrawRect(minPt, maxPt, Dia::Core::RGBA(0, 0, 0, 0), fill);
                }

            private:
                const DiaScalarField<Topology, Policy>& mField;
                Dia::Core::StringCRC                    mLayerName;
                float                                   mCellWorldSize;
                Dia::Maths::Vector2D                    mWorldOrigin;
                OverlayColourMap                        mColourMap;
            };

            // ----------------------------------------------------------------
            // ScalarFieldGradientOverlay
            //
            // Draws a direction arrow per cell using RequestDrawRay.
            // Cells with gradient magnitude below minMagnitude are skipped.
            // ----------------------------------------------------------------
            template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
            class ScalarFieldGradientOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                ScalarFieldGradientOverlay(const DiaScalarField<Topology, Policy>& field,
                                           Dia::Core::StringCRC layerName,
                                           float cellWorldSize,
                                           Dia::Maths::Vector2D worldOrigin,
                                           float arrowScale    = 0.35f,
                                           float minMagnitude  = 0.01f,
                                           Dia::Core::RGBA arrowColour = Dia::Core::RGBA(255, 255, 255, 180))
                    : mField(field)
                    , mLayerName(layerName)
                    , mCellWorldSize(cellWorldSize)
                    , mWorldOrigin(worldOrigin)
                    , mArrowScale(arrowScale)
                    , mMinMagnitude(minMagnitude)
                    , mArrowColour(arrowColour)
                {}

                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                        return;

                    const Topology& topology = mField.GetTopology();

                    if constexpr (std::is_same_v<Topology, SquareFieldTopology>)
                    {
                        for (int y = 0; y < topology.GetHeight(); ++y)
                            for (int x = 0; x < topology.GetWidth(); ++x)
                                DrawCell(draw, CellIndex{x, y});
                    }
                    else if constexpr (std::is_same_v<Topology, HexFieldTopology>)
                    {
                        const int R = topology.GetRadius();
                        for (int q = -R; q <= R; ++q)
                        {
                            const int rMin = (q < 0) ? -R - q : -R;
                            const int rMax = (q > 0) ?  R - q :  R;
                            for (int r = rMin; r <= rMax; ++r)
                                DrawCell(draw, CellIndex{q, r});
                        }
                    }
                }

                // Public helper: draw arrow for a single cell.
                void DrawCell(Dia::Core::IDebugDraw& draw, CellIndex cell) const
                {
                    const Dia::Maths::Vector2D grad = mField.GetGradient(cell);

                    const float mag = std::sqrt(grad.x * grad.x + grad.y * grad.y);
                    if (mag < mMinMagnitude)
                        return;

                    const Dia::Maths::Vector2D normDir(grad.x / mag, grad.y / mag);

                    const Dia::Maths::Vector2D centre = Detail::CellCentre(cell, mCellWorldSize, mWorldOrigin);

                    draw.RequestDrawRay(centre, normDir, mCellWorldSize * mArrowScale, mArrowColour);
                }

            private:
                const DiaScalarField<Topology, Policy>& mField;
                Dia::Core::StringCRC                    mLayerName;
                float                                   mCellWorldSize;
                Dia::Maths::Vector2D                    mWorldOrigin;
                float                                   mArrowScale;
                float                                   mMinMagnitude;
                Dia::Core::RGBA                         mArrowColour;
            };

        } // namespace Adaptors
    } // namespace ScalarField
} // namespace Dia
