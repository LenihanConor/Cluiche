////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldDrawParams.h
// Description: Pure (bgfx-free) description of one CellField draw: the shader uniform
//              values plus the colour-map texture the draw needs. Also the CPU reference
//              of the shader's palette lookup, which pins the shader semantics for tests.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <stdint.h>

namespace Dia
{
    namespace Bgfx
    {
        // Shader map modes (u_params1.z).
        enum class CellFieldMapMode : uint32_t { Palette = 0, Ramp = 1, Backdrop = 2 };

        // What the draw needs to know about its layer.
        struct CellFieldLayerInfo
        {
            uint16_t                          width = 0;
            uint16_t                          height = 0;
            float                             cellSize = 1.0f;
            Dia::Graphics::CellFieldValueKind kind = Dia::Graphics::CellFieldValueKind::Float;
        };

        // Uniform values in fs/vs_cellfield.sc layout, ready for bgfx::setUniform.
        struct CellFieldDrawParams
        {
            float params0[4]         = {};   // rampMin, rampMax, inset, cellSize
            float params1[4]         = {};   // hillshadeStrength, elevationScale, mapMode, hasElevation
            float params2[4]         = {};   // smooth, ambient, -, -
            float gridInfo[4]        = {};   // w, h, 1/w, 1/h
            float fallbackColour[4]  = {};
            float backdropColour[4]  = {};
            float lightDir[4]        = {};
            CellFieldMapMode mapMode = CellFieldMapMode::Palette;
            uint32_t colourMapId       = 0;  // colour-map texture required by this draw
            uint32_t colourMapRevision = 0;
        };

        // 0xRRGGBBAA -> float4 in [0,1].
        void CellFieldUnpackRgba8(uint32_t rgba8, float out[4]);

        // CPU mirror of the fragment shader's paletteColour(): index = floor(v + 0.5);
        // < 0, >= 64, >= count, or non-finite -> palette fallback.
        bool     CellFieldPaletteIndex(const Dia::Graphics::CellFieldPaletteData& palette, float categoryValue, uint32_t& outIndex);
        uint32_t CellFieldPaletteLookup(const Dia::Graphics::CellFieldPaletteData& palette, float categoryValue);

        // Computes the uniforms for one draw. Exactly one of palette / ramp is used, chosen by
        // layer.kind (Category -> palette, Float -> ramp). Returns false if the needed
        // colour map is null. Ramp, inset, backdrop and hillshade are extension points
        // (Tasks 11-13): they currently emit shader-neutral defaults.
        bool BuildCellFieldDrawParams(const Dia::Core::CellFieldDrawRequest& request,
                                      const CellFieldLayerInfo& layer,
                                      const Dia::Graphics::CellFieldPaletteData* palette,
                                      const Dia::Graphics::CellFieldRampData* ramp,
                                      CellFieldDrawParams& out);

    } // namespace Bgfx
} // namespace Dia
