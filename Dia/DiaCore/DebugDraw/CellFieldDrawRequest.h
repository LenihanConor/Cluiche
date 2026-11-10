////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldDrawRequest.h
// Description: Plain-data request types for rendering grid-based cell field data.
//              POD structs for submitting layer rendering requests to CellField
//              renderer. Trivially copyable for efficient queue submission.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>
#include <type_traits>

namespace Dia
{
    namespace Core
    {
        ////////////////////////////////////////////////////////////////////////////////
        // CellFieldLayerHandle
        //
        // Opaque handle to a single layer in a CellField. Uniquely identifies
        // a layer across allocations via generation counter.
        ////////////////////////////////////////////////////////////////////////////////
        struct CellFieldLayerHandle
        {
            uint16_t index = 0xFFFF;
            uint16_t generation = 0;

            /// True if handle points to a valid layer (index != 0xFFFF)
            inline bool IsValid() const
            {
                return index != 0xFFFF;
            }
        };

        ////////////////////////////////////////////////////////////////////////////////
        // CellFieldShading
        //
        // Shading mode for layer rendering.
        // Flat (Step 1): per-cell constant shading.
        // Smooth (Step 2): interpolated shading across cells.
        ////////////////////////////////////////////////////////////////////////////////
        enum class CellFieldShading : uint8_t
        {
            Flat = 0,   // Constant per-cell value
            Smooth = 1  // Interpolated across cells (Step 2)
        };

        ////////////////////////////////////////////////////////////////////////////////
        // CellFieldDrawRequest
        //
        // Request to render a single layer of cell field data.
        // Specifies the layer, colour mapping, elevation hillshade, and blend modes.
        // POD struct; trivially copyable for queue submission.
        //
        // Rendering steps:
        //   Step 1: Base layer colour + optional hillshade overlay
        //   Step 2: Shading mode selection + state blending
        ////////////////////////////////////////////////////////////////////////////////
        struct CellFieldDrawRequest
        {
            // Layer to render
            CellFieldLayerHandle layer;

            // Colour mapping
            // For category layers: palette ID (StringCRC(name).Value())
            // For float layers: colour ramp ID (StringCRC(name).Value())
            // Stored as the CRC value, not a StringCRC: StringCRC carries a 64-byte
            // string and is not trivially copyable, which this request must be.
            uint32_t colourMap = 0;

            // Float layer mapping parameters (ignored for category layers)
            float rampMin = 0.0f;   ///< Map range: [rampMin, rampMax] to [0, 1] for lookup
            float rampMax = 1.0f;

            // Cell inset for rendering gap between cells
            float inset = 0.0f;     ///< Inset distance in [0, 0.5) as fraction of cell size

            // Optional backdrop fill colour (RGBA8 packed uint32_t)
            // Alpha = 0 disables backdrop rendering
            uint32_t backdropColour = 0;

            // Relief hillshade overlay (Step 1)
            // Modulates layer colour with elevation relief lighting
            CellFieldLayerHandle elevationLayer;    ///< Source elevation data (invalid = no hillshade)
            float lightAzimuthDeg = 315.0f;         ///< Light direction (degrees clockwise from north)
            float lightElevationDeg = 45.0f;        ///< Light angle above horizon
            float hillshadeStrength = 0.0f;         ///< Blend factor [0, 1]: 0 = no effect, 1 = full hillshade
            float elevationScale = 1.0f;            ///< Vertical exaggeration factor for relief

            // Step 2 fields (present from Step 1, defaulted, logged if set before Step 2 ready)
            CellFieldShading shading = CellFieldShading::Flat;  ///< Interpolation mode (Step 2)
            float opacity = 1.0f;                               ///< Layer opacity [0, 1]
            float stateBlend = 1.0f;                            ///< Temporal blend [0, 1]: 0 = previous state, 1 = current state
        };

        // Compile-time assertion: CellFieldLayerHandle must be trivially copyable
        static_assert(std::is_trivially_copyable<CellFieldLayerHandle>::value,
            "CellFieldLayerHandle must be trivially copyable");

        // Compile-time assertion: CellFieldDrawRequest must be trivially copyable
        static_assert(std::is_trivially_copyable<CellFieldDrawRequest>::value,
            "CellFieldDrawRequest must be trivially copyable");

    } // namespace Core
} // namespace Dia
