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

        uint32_t CellFieldRampLookup(const uint32_t* texels, float value, float rampMin, float rampMax)
        {
            float range = rampMax - rampMin;
            if (!(fabsf(range) > 1e-20f))   // also catches NaN
                range = 1.0f;
            float t = (value - rampMin) / range;
            if (!(t == t))
                t = 0.0f;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

            const float pos = t * 255.0f;
            uint32_t i0 = static_cast<uint32_t>(pos);
            if (i0 > 255u)
                i0 = 255u;
            const uint32_t i1 = i0 < 255u ? i0 + 1u : 255u;
            const float f = pos - static_cast<float>(i0);

            uint32_t result = 0;
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                const float a = static_cast<float>((texels[i0] >> shift) & 0xFFu);
                const float b = static_cast<float>((texels[i1] >> shift) & 0xFFu);
                const uint32_t c = static_cast<uint32_t>(floorf(a + (b - a) * f + 0.5f));
                result |= (c & 0xFFu) << shift;
            }
            return result;
        }

        uint32_t CellFieldRampLookup(const Dia::Graphics::CellFieldRampData& ramp, float value, float rampMin, float rampMax)
        {
            uint32_t texels[Dia::Graphics::CellFieldRegistry::kRampTexels];
            Dia::Graphics::BakeRamp(ramp.stops, ramp.count, texels);
            return CellFieldRampLookup(texels, value, rampMin, rampMax);
        }

        float CellFieldEffectiveInset(float inset)
        {
            if (!(inset == inset) || inset <= 0.0f)
                return 0.0f;   // NaN, zero, negative
            return inset >= 0.5f ? kCellFieldMaxInset : inset;
        }

        bool CellFieldGridBounds(const float* centresXY, uint32_t width, uint32_t height, float cellSize, CellFieldBounds& out)
        {
            const uint32_t cells = width * height;
            if (centresXY == nullptr || cells == 0)
                return false;
            float minX = centresXY[0], maxX = centresXY[0];
            float minY = centresXY[1], maxY = centresXY[1];
            for (uint32_t i = 1; i < cells; ++i)
            {
                const float x = centresXY[2u * i];
                const float y = centresXY[2u * i + 1u];
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
            const float halfH = cellSize * 0.8660254037844386f;   // sqrt(3)/2
            out.minX = minX - cellSize;
            out.maxX = maxX + cellSize;
            out.minY = minY - halfH;
            out.maxY = maxY + halfH;
            return true;
        }

        void CellFieldInsetVertexPosition(const float centre[2], const float offset[2], float cellSize, float inset, float out[2])
        {
            const float scale = cellSize * (1.0f - inset);
            out[0] = centre[0] + offset[0] * scale;
            out[1] = centre[1] + offset[1] * scale;
        }

        bool CellFieldNeedsBackdrop(float inset, uint32_t backdropRgba8)
        {
            return CellFieldEffectiveInset(inset) > 0.0f && (backdropRgba8 & 0xFFu) != 0u;
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
            // Ramp range and the clamped inset are passed through.
            out.params0[0] = request.rampMin;
            out.params0[1] = request.rampMax;
            out.params0[2] = CellFieldEffectiveInset(request.inset);
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
            CellFieldUnpackRgba8(request.backdropColour, out.backdropColour);

            out.lightDir[2] = 1.0f;   // straight up; replaced by azimuth/elevation in Task 13
            return true;
        }

    } // namespace Bgfx
} // namespace Dia
