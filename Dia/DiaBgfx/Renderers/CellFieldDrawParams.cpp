////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldDrawParams.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/CellFieldDrawParams.h"

#include <math.h>

namespace Dia
{
    namespace Bgfx
    {
        void CellFieldUnpackRgba8(uint32_t rgba8, float out[4])
        {
            out[0] = static_cast<float>((rgba8 >> 24) & 0xFFu) / 255.0f;
            out[1] = static_cast<float>((rgba8 >> 16) & 0xFFu) / 255.0f;
            out[2] = static_cast<float>((rgba8 >> 8) & 0xFFu) / 255.0f;
            out[3] = static_cast<float>(rgba8 & 0xFFu) / 255.0f;
        }

        bool CellFieldPaletteIndex(const Dia::Graphics::CellFieldPaletteData& palette, float categoryValue, uint32_t& outIndex)
        {
            outIndex = 0;
            if (!(categoryValue == categoryValue) || fabsf(categoryValue) > 1.0e9f)
                return false;   // NaN / absurd magnitude
            const float i = floorf(categoryValue + 0.5f);
            if (i < 0.0f || i > 63.0f || i >= static_cast<float>(palette.count))
                return false;
            outIndex = static_cast<uint32_t>(i);
            return true;
        }

        uint32_t CellFieldPaletteLookup(const Dia::Graphics::CellFieldPaletteData& palette, float categoryValue)
        {
            uint32_t index = 0;
            if (!CellFieldPaletteIndex(palette, categoryValue, index))
                return palette.fallback;
            return palette.colours[index];
        }

        bool BuildCellFieldDrawParams(const Dia::Core::CellFieldDrawRequest& request,
                                      const CellFieldLayerInfo& layer,
                                      const Dia::Graphics::CellFieldPaletteData* palette,
                                      const Dia::Graphics::CellFieldRampData* ramp,
                                      CellFieldDrawParams& out)
        {
            out = CellFieldDrawParams();

            const bool isCategory = layer.kind == Dia::Graphics::CellFieldValueKind::Category;
            if (isCategory && palette == nullptr)
                return false;
            if (!isCategory && ramp == nullptr)
                return false;

            out.mapMode = isCategory ? CellFieldMapMode::Palette : CellFieldMapMode::Ramp;
            out.colourMapId = request.colourMap;
            out.colourMapRevision = isCategory ? palette->revision : ramp->revision;

            // params0: rampMin, rampMax, inset, cellSize.
            // Extension points: ramp range (Task 11) is passed through; inset (Task 12) stays 0.
            out.params0[0] = request.rampMin;
            out.params0[1] = request.rampMax;
            out.params0[2] = 0.0f;
            out.params0[3] = layer.cellSize;

            // params1: hillshadeStrength, elevationScale, mapMode, hasElevation.
            // Extension point (Task 13): hillshade off until the elevation texture is wired.
            out.params1[0] = 0.0f;
            out.params1[1] = request.elevationScale;
            out.params1[2] = static_cast<float>(static_cast<uint32_t>(out.mapMode));
            out.params1[3] = 0.0f;

            // params2: smooth (Step 2, off), ambient.
            out.params2[0] = 0.0f;
            out.params2[1] = 1.0f;

            const float w = static_cast<float>(layer.width);
            const float h = static_cast<float>(layer.height);
            out.gridInfo[0] = w;
            out.gridInfo[1] = h;
            out.gridInfo[2] = layer.width > 0 ? 1.0f / w : 0.0f;
            out.gridInfo[3] = layer.height > 0 ? 1.0f / h : 0.0f;

            if (isCategory)
                CellFieldUnpackRgba8(palette->fallback, out.fallbackColour);
            // Extension point (Task 12): backdrop colour stays 0 (transparent).

            out.lightDir[2] = 1.0f;   // straight up; replaced by azimuth/elevation in Task 13
            return true;
        }

    } // namespace Bgfx
} // namespace Dia
