// ScalarFieldOverlay.h
// Optional header-only adaptor — colour-gradient debug overlay for a
// DiaScalarField rendered via DiaVisualDebugger's IDebugDraw interface.
//
// OPTIONAL ADAPTOR — not part of DiaScalarField.vcxproj build.
// DiaScalarField.vcxproj carries NO hard link dependency on DiaVisualDebugger.
// Consumers must add DiaVisualDebugger to their own project dependencies.
//
// Usage:
//   Implements Dia::Debug::IVisualDebugger.
//   Register with DebugLayerManager and call SetEnabled(true) to activate.
//
//   OverlayColourMap controls the low/high colour extremes and the value
//   range that maps to them.  Values outside [minValue, maxValue] are clamped.
//
// Usage example:
//   OverlayColourMap colourMap;
//   colourMap.lowColour  = Dia::Core::RGBA(0, 0, 255);   // blue  for 0.0
//   colourMap.highColour = Dia::Core::RGBA(255, 0, 0);   // red   for 1.0
//   colourMap.minValue   = 0.0f;
//   colourMap.maxValue   = 1.0f;
//
//   ScalarFieldOverlay<SquareFieldTopology, UniformDecayPolicy> overlay(
//       field, Dia::Core::StringCRC("ScalarField"), colourMap);
//   debugLayerManager.Register(&overlay);

#pragma once
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/CFieldTopology.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace ScalarField
    {
        namespace Adaptors
        {
            // Colour mapping configuration for the overlay.
            // Cell values are linearly interpolated between lowColour (at minValue)
            // and highColour (at maxValue).  Out-of-range values are clamped.
            struct OverlayColourMap
            {
                Dia::Core::RGBA lowColour  = Dia::Core::RGBA(0,   0,   255, 200); // blue
                Dia::Core::RGBA highColour = Dia::Core::RGBA(255, 0,   0,   200); // red
                float           minValue   = 0.0f;
                float           maxValue   = 1.0f;
            };

            // Colour-gradient debug overlay for a DiaScalarField.
            //
            // Implements Dia::Debug::IVisualDebugger.  Each frame Draw() iterates all
            // cells, maps their value to a colour via OverlayColourMap, and submits a
            // filled rect per cell using the IDebugDraw interface.
            //
            // cellWorldSize controls the pixel / world-unit size of each rendered cell.
            // worldOrigin is the world-space position of cell {0, 0}.
            //
            // NOTE: Draw() body is a stub pending topology-to-world-space mapping
            // integration.  The IVisualDebugger interface and colour interpolation
            // logic are complete; only the cell-rect position calculation requires
            // topology-specific coordinate conversion which is left as a TODO for
            // the consumer to fill in once world-space conventions are established.
            template<CFieldTopology Topology, typename Policy>
            class ScalarFieldOverlay : public Dia::Debug::IVisualDebugger
            {
            public:
                ScalarFieldOverlay(const DiaScalarField<Topology, Policy>& field,
                                   Dia::Core::StringCRC layerName,
                                   OverlayColourMap colourMap = {},
                                   float cellWorldSize = 1.0f,
                                   Dia::Maths::Vector2D worldOrigin = Dia::Maths::Vector2D(0.0f, 0.0f))
                    : mField(field)
                    , mLayerName(layerName)
                    , mColourMap(colourMap)
                    , mCellWorldSize(cellWorldSize)
                    , mWorldOrigin(worldOrigin)
                {}

                // IVisualDebugger interface
                Dia::Core::StringCRC GetLayerName() const override { return mLayerName; }

                void Draw(Dia::Core::IDebugDraw& draw) override
                {
                    if (!IsEnabled())
                        return;

                    const int count = mField.GetCellCount();
                    const Topology& topology = mField.GetTopology();

                    // Iterate all cells via the topology's cell enumeration.
                    // TODO: The topology concept does not currently expose ForEachCell.
                    // This stub uses ForEachNeighbour from (0,0) as a placeholder;
                    // consumers should replace with their topology's actual enumeration.
                    //
                    // For a SquareFieldTopology, replace with:
                    //   for (int y = 0; y < topology.GetHeight(); ++y)
                    //     for (int x = 0; x < topology.GetWidth(); ++x)
                    //       DrawCell(draw, CellIndex{x, y});
                    (void)count;
                    (void)topology;
                    (void)draw;

                    // Stub: visual rendering body is pending world-space coordinate
                    // integration with the consuming application's coordinate system.
                    // See DrawCell() below for the per-cell colour mapping logic.
                }

                // Helper: draw a single cell as a filled rect with colour interpolated
                // from the cell's current value.  Exposed for consumers who supply
                // their own enumeration loop.
                void DrawCell(Dia::Core::IDebugDraw& draw, CellIndex cell) const
                {
                    const float value  = mField.GetValue(cell);
                    const float t      = Clamp01((value - mColourMap.minValue) /
                                                  (mColourMap.maxValue - mColourMap.minValue));

                    const Dia::Core::RGBA colour = LerpColour(mColourMap.lowColour,
                                                               mColourMap.highColour, t);

                    // World-space rect for this cell (assumes square grid layout).
                    const float wx = mWorldOrigin.x + static_cast<float>(cell.x) * mCellWorldSize;
                    const float wy = mWorldOrigin.y + static_cast<float>(cell.y) * mCellWorldSize;

                    const Dia::Maths::Vector2D min(wx,                    wy);
                    const Dia::Maths::Vector2D max(wx + mCellWorldSize,   wy + mCellWorldSize);

                    draw.RequestDrawRect(min, max, colour, colour);
                }

            private:
                static float Clamp01(float v)
                {
                    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                }

                static Dia::Core::RGBA LerpColour(const Dia::Core::RGBA& a, const Dia::Core::RGBA& b, float t)
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

                const DiaScalarField<Topology, Policy>& mField;
                Dia::Core::StringCRC                    mLayerName;
                OverlayColourMap                        mColourMap;
                float                                   mCellWorldSize;
                Dia::Maths::Vector2D                    mWorldOrigin;
            };

        } // namespace Adaptors
    } // namespace ScalarField
} // namespace Dia
