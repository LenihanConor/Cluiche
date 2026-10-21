// TestCellFieldGapsBgfxParams.cpp - Task 26B exhaustive gap tests for the pure (GPU-free) CellField
// draw maths: BuildCellFieldDrawParams and the CPU mirrors of the shaders, Ortho2D and view order.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldDrawParams.h>
#include <DiaBgfx/Renderers/CellFieldViewOrder.h>
#include <DiaBgfx/Renderers/Ortho2D.h>
#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

#include <limits>
#include <cmath>
#include <math.h>
#include <string.h>
#include <vector>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;
using Dia::Maths::Vector2D;

namespace
{
    const float kNaN = std::numeric_limits<float>::quiet_NaN();
    const float kInf = std::numeric_limits<float>::infinity();

    CellFieldLayerInfo Info(uint16_t w, uint16_t h, CellFieldValueKind kind, float cellSize = 1.0f)
    {
        CellFieldLayerInfo i;
        i.width = w; i.height = h; i.kind = kind; i.cellSize = cellSize;
        return i;
    }

    CellFieldPaletteData Palette(uint32_t count, uint32_t fallback, uint32_t revision = 0)
    {
        CellFieldPaletteData p;
        p.count = count;
        for (uint32_t i = 0; i < count; ++i)
            p.colours[i] = 0x10000000u + (i << 8) + 0xFFu;
        p.fallback = fallback;
        p.revision = revision;
        return p;
    }

    CellFieldRampData Ramp(uint32_t revision = 0)
    {
        CellFieldRampData r;
        r.count = 2;
        r.stops[0] = { 0.0f, 0x000000FFu };
        r.stops[1] = { 1.0f, 0xFFFFFFFFu };
        r.revision = revision;
        return r;
    }

    float Len3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
}

// ===========================================================================
// CellFieldUnpackRgba8
// ===========================================================================

TEST(CellFieldGapsParams, UnpackRgba8ChannelOrderAndExtremes)
{
    float c[4];
    CellFieldUnpackRgba8(0x00000000u, c);
    EXPECT_EQ(0.0f, c[0]); EXPECT_EQ(0.0f, c[1]); EXPECT_EQ(0.0f, c[2]); EXPECT_EQ(0.0f, c[3]);
    CellFieldUnpackRgba8(0xFFFFFFFFu, c);
    EXPECT_EQ(1.0f, c[0]); EXPECT_EQ(1.0f, c[1]); EXPECT_EQ(1.0f, c[2]); EXPECT_EQ(1.0f, c[3]);
    CellFieldUnpackRgba8(0xFF000000u, c);
    EXPECT_EQ(1.0f, c[0]); EXPECT_EQ(0.0f, c[1]); EXPECT_EQ(0.0f, c[2]); EXPECT_EQ(0.0f, c[3]);
    CellFieldUnpackRgba8(0x00FF0000u, c);
    EXPECT_EQ(0.0f, c[0]); EXPECT_EQ(1.0f, c[1]);
    CellFieldUnpackRgba8(0x0000FF00u, c);
    EXPECT_EQ(1.0f, c[2]);
    CellFieldUnpackRgba8(0x000000FFu, c);
    EXPECT_EQ(1.0f, c[3]);
    CellFieldUnpackRgba8(0x80402010u, c);
    EXPECT_NEAR(128.0f / 255.0f, c[0], 1e-6f);
    EXPECT_NEAR(64.0f / 255.0f, c[1], 1e-6f);
    EXPECT_NEAR(32.0f / 255.0f, c[2], 1e-6f);
    EXPECT_NEAR(16.0f / 255.0f, c[3], 1e-6f);
}

// ===========================================================================
// Palette / ramp lookup mirrors
// ===========================================================================

TEST(CellFieldGapsParams, PaletteIndexTableDriven)
{
    struct Row { uint32_t count; float value; bool inRange; uint32_t index; };
    const Row rows[] = {
        { 3, 0.0f, true, 0 },    { 3, 2.0f, true, 2 },     { 3, 3.0f, false, 0 },   { 3, -0.5f, true, 0 },
        { 3, -0.51f, false, 0 }, { 3, 2.49f, true, 2 },    { 3, 2.5f, false, 0 },   { 64, 63.0f, true, 63 },
        { 64, 64.0f, false, 0 }, { 64, 63.49f, true, 63 }, { 64, 63.5f, false, 0 }, { 64, -1.0f, false, 0 },
        { 64, 1e9f, false, 0 },  { 64, -1e9f, false, 0 },  { 64, 1.0e10f, false, 0 }, { 64, kNaN, false, 0 },
        { 64, kInf, false, 0 },  { 64, -kInf, false, 0 },  { 1, 0.0f, true, 0 },    { 1, 1.0f, false, 0 },
        { 0, 0.0f, false, 0 },
    };
    for (const Row& r : rows)
    {
        CellFieldPaletteData p = Palette(r.count, 0xAABBCCDDu);
        uint32_t index = 99;
        EXPECT_EQ(r.inRange, CellFieldPaletteIndex(p, r.value, index)) << r.count << " " << r.value;
        if (r.inRange)
            EXPECT_EQ(r.index, index) << r.count << " " << r.value;
        else
        {
            EXPECT_EQ(0u, index);   // out-param is zeroed on failure
            EXPECT_EQ(0xAABBCCDDu, CellFieldPaletteLookup(p, r.value)) << r.count << " " << r.value;
        }
    }
}

TEST(CellFieldGapsParams, PaletteLookupReturnsTheEntryColour)
{
    CellFieldPaletteData p = Palette(5, 0x11223344u);
    for (uint32_t i = 0; i < 5; ++i)
        EXPECT_EQ(p.colours[i], CellFieldPaletteLookup(p, static_cast<float>(i)));
}

TEST(CellFieldGapsParams, RampLookupNonFiniteAndExtremeInputs)
{
    const CellFieldRampData r = Ramp();
    const uint32_t first = 0x000000FFu, last = 0xFFFFFFFFu;
    EXPECT_EQ(last, CellFieldRampLookup(r, kInf, 0.0f, 1.0f));
    EXPECT_EQ(first, CellFieldRampLookup(r, -kInf, 0.0f, 1.0f));
    EXPECT_EQ(first, CellFieldRampLookup(r, kNaN, 0.0f, 1.0f));
    EXPECT_EQ(first, CellFieldRampLookup(r, 0.5f, kNaN, 1.0f));    // NaN min -> t = 0
}

TEST(CellFieldGapsParams, RampLookupNaNMaxMeansUnitRange)
{
    // range is NaN -> |range| > 1e-20 is false -> range = 1, so the lookup is value - min.
    const CellFieldRampData r = Ramp();
    EXPECT_EQ(CellFieldRampLookup(r, 0.0f, 0.0f, 1.0f), CellFieldRampLookup(r, 0.0f, 0.0f, kNaN));
    EXPECT_EQ(CellFieldRampLookup(r, 1.0f, 0.0f, 1.0f), CellFieldRampLookup(r, 1.0f, 0.0f, kNaN));
}

TEST(CellFieldGapsParams, RampLookupIsMonotoneForBlackToWhite)
{
    const CellFieldRampData r = Ramp();
    uint32_t texels[256];
    BakeRamp(r.stops, r.count, texels);
    uint32_t prev = 0;
    for (int i = 0; i <= 1000; ++i)
    {
        const uint32_t c = CellFieldRampLookup(texels, i / 1000.0f, 0.0f, 1.0f) >> 24;   // red channel
        EXPECT_GE(c, prev) << i;
        prev = c;
    }
    EXPECT_EQ(255u, prev);
}

TEST(CellFieldGapsParams, RampLookupSingleStopIsConstant)
{
    CellFieldRampData r;
    r.count = 1;
    r.stops[0] = { 0.5f, 0x12345678u };
    for (float v : { -5.0f, 0.0f, 0.5f, 1.0f, 9.0f })
        EXPECT_EQ(0x12345678u, CellFieldRampLookup(r, v, 0.0f, 1.0f)) << v;
}

TEST(CellFieldGapsParams, RampLookupEndsExactAtRampMinMaxWithNegativeAndLargeRanges)
{
    const CellFieldRampData r = Ramp();
    EXPECT_EQ(0x000000FFu, CellFieldRampLookup(r, -100.0f, -100.0f, 100.0f));
    EXPECT_EQ(0xFFFFFFFFu, CellFieldRampLookup(r, 100.0f, -100.0f, 100.0f));
    EXPECT_EQ(0x000000FFu, CellFieldRampLookup(r, 1.0e30f, 1.0e30f, 3.0e38f));
    EXPECT_EQ(0xFFFFFFFFu, CellFieldRampLookup(r, 6.0f, 5.0f, 6.0f));
}

// ===========================================================================
// Inset / backdrop / bounds / vertex placement
// ===========================================================================

TEST(CellFieldGapsParams, EffectiveInsetTableDriven)
{
    struct Row { float in; float out; };
    const Row rows[] = {
        { 0.0f, 0.0f }, { -0.0f, 0.0f }, { 1e-9f, 1e-9f }, { 0.25f, 0.25f }, { 0.4989f, 0.4989f },
        { 0.499f, 0.499f }, { 0.4995f, 0.4995f }, { 0.5f, kCellFieldMaxInset }, { 0.5001f, kCellFieldMaxInset },
        { 1.0f, kCellFieldMaxInset }, { 1.0e9f, kCellFieldMaxInset }, { kInf, kCellFieldMaxInset },
        { -1.0f, 0.0f }, { -1.0e9f, 0.0f }, { -kInf, 0.0f }, { kNaN, 0.0f },
    };
    for (const Row& r : rows)
        EXPECT_FLOAT_EQ(r.out, CellFieldEffectiveInset(r.in)) << r.in;
}

TEST(CellFieldGapsParams, NeedsBackdropAlphaAndInsetMatrix)
{
    EXPECT_FALSE(CellFieldNeedsBackdrop(0.0f, 0xFFFFFFFFu));
    EXPECT_FALSE(CellFieldNeedsBackdrop(0.1f, 0xFFFFFF00u));
    EXPECT_FALSE(CellFieldNeedsBackdrop(0.1f, 0u));
    EXPECT_TRUE(CellFieldNeedsBackdrop(0.1f, 0x00000001u));
    EXPECT_TRUE(CellFieldNeedsBackdrop(0.1f, 0x000000FFu));
    EXPECT_TRUE(CellFieldNeedsBackdrop(0.1f, 0xFFFFFFFFu));
    EXPECT_FALSE(CellFieldNeedsBackdrop(kNaN, 0xFFu));
    EXPECT_FALSE(CellFieldNeedsBackdrop(-0.5f, 0xFFu));
    EXPECT_TRUE(CellFieldNeedsBackdrop(7.0f, 0xFFu));
}

TEST(CellFieldGapsParams, InsetVertexPositionExtremes)
{
    const float centre[2] = { 10.0f, -4.0f };
    const float offsets[3][2] = { { 1.0f, 0.0f }, { -0.5f, 0.8660254f }, { 0.0f, 0.0f } };
    float out[2];
    for (const auto& o : offsets)
    {
        CellFieldInsetVertexPosition(centre, o, 2.0f, 0.0f, out);
        EXPECT_NEAR(centre[0] + o[0] * 2.0f, out[0], 1e-5f);
        EXPECT_NEAR(centre[1] + o[1] * 2.0f, out[1], 1e-5f);
        CellFieldInsetVertexPosition(centre, o, 2.0f, kCellFieldMaxInset, out);
        EXPECT_NEAR(centre[0] + o[0] * 2.0f * (1.0f - kCellFieldMaxInset), out[0], 1e-5f);
        CellFieldInsetVertexPosition(centre, o, 2.0f, 1.0f, out);   // full inset collapses to the centre
        EXPECT_FLOAT_EQ(centre[0], out[0]);
        EXPECT_FLOAT_EQ(centre[1], out[1]);
        CellFieldInsetVertexPosition(centre, o, 0.0f, 0.2f, out);   // zero cell size collapses too
        EXPECT_FLOAT_EQ(centre[0], out[0]);
        EXPECT_FLOAT_EQ(centre[1], out[1]);
    }
}

TEST(CellFieldGapsParams, GridBoundsSingleCellIsOneHexAroundTheCentre)
{
    const float centre[2] = { 3.0f, 5.0f };
    CellFieldBounds b;
    ASSERT_TRUE(CellFieldGridBounds(centre, 1, 1, 2.0f, b));
    EXPECT_FLOAT_EQ(1.0f, b.minX);
    EXPECT_FLOAT_EQ(5.0f, b.maxX);
    EXPECT_NEAR(5.0f - 2.0f * 0.8660254f, b.minY, 1e-5f);
    EXPECT_NEAR(5.0f + 2.0f * 0.8660254f, b.maxY, 1e-5f);
}

TEST(CellFieldGapsParams, GridBoundsRejectsNullAndZeroDimensions)
{
    const float c[2] = { 0.0f, 0.0f };
    CellFieldBounds b;
    EXPECT_FALSE(CellFieldGridBounds(nullptr, 2, 2, 1.0f, b));
    EXPECT_FALSE(CellFieldGridBounds(c, 0, 2, 1.0f, b));
    EXPECT_FALSE(CellFieldGridBounds(c, 2, 0, 1.0f, b));
    EXPECT_FALSE(CellFieldGridBounds(c, 0, 0, 1.0f, b));
}

TEST(CellFieldGapsParams, GridBoundsOddQStaggerOddColumnSitsHalfHexLower)
{
    // 2x1 flat-top odd-q: the odd column (x = 1) is shifted down half a hex height, so the
    // bounds' lower edge comes from column 1 and the upper edge from column 0.
    Dia::Pathfinding::HexTopology topo(2, 1);
    HexDescStorage storage;
    const CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
    const float y0 = d.centresXY[1], y1 = d.centresXY[3];
    EXPECT_GT(y1, y0);
    EXPECT_NEAR(0.8660254f, y1 - y0, 1e-4f);
    CellFieldBounds b;
    ASSERT_TRUE(CellFieldGridBounds(d.centresXY, 2, 1, 1.0f, b));
    EXPECT_NEAR(y0 - 0.8660254f, b.minY, 1e-4f);
    EXPECT_NEAR(y1 + 0.8660254f, b.maxY, 1e-4f);
}

TEST(CellFieldGapsParams, GridBoundsHugeGridMatchesBuiltGeometry)
{
    Dia::Pathfinding::HexTopology topo(128, 128);
    HexDescStorage storage;
    const CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.5f, storage);
    CellFieldBounds b;
    ASSERT_TRUE(CellFieldGridBounds(d.centresXY, 128, 128, 1.5f, b));
    HexFlatTopOddQBuilder builder;
    CellGridGeometry geo;
    ASSERT_TRUE(builder.Build(d, geo));
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    for (uint32_t i = 0; i < geo.vertexCount; ++i)
    {
        float p[2];
        CellFieldInsetVertexPosition(geo.vertices[i].centre, geo.vertices[i].offset, 1.5f, 0.0f, p);
        minX = fminf(minX, p[0]); maxX = fmaxf(maxX, p[0]);
        minY = fminf(minY, p[1]); maxY = fmaxf(maxY, p[1]);
    }
    EXPECT_NEAR(minX, b.minX, 1e-3f);
    EXPECT_NEAR(maxX, b.maxX, 1e-3f);
    EXPECT_NEAR(minY, b.minY, 1e-3f);
    EXPECT_NEAR(maxY, b.maxY, 1e-3f);
}

TEST(CellFieldGapsParams, GridBoundsZeroCellSizeCollapsesToCentreRange)
{
    const float centres[4] = { 1.0f, 2.0f, 4.0f, -3.0f };
    CellFieldBounds b;
    ASSERT_TRUE(CellFieldGridBounds(centres, 2, 1, 0.0f, b));
    EXPECT_FLOAT_EQ(1.0f, b.minX); EXPECT_FLOAT_EQ(4.0f, b.maxX);
    EXPECT_FLOAT_EQ(-3.0f, b.minY); EXPECT_FLOAT_EQ(2.0f, b.maxY);
}

// ===========================================================================
// BuildCellFieldDrawParams: every uniform field per branch
// ===========================================================================

TEST(CellFieldGapsParams, PaletteBranchFillsEveryField)
{
    Dia::Core::CellFieldDrawRequest req;
    req.colourMap = 0xCAFEF00Du;
    req.rampMin = -3.0f; req.rampMax = 7.0f;       // ignored by the palette path but passed through
    req.inset = 0.2f;
    req.backdropColour = 0x11223344u;
    CellFieldDrawParams p;
    const CellFieldPaletteData pal = Palette(4, 0xFF8000C0u, 9);
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(5, 3, CellFieldValueKind::Category, 2.5f), &pal, nullptr, p));

    EXPECT_EQ(CellFieldMapMode::Palette, p.mapMode);
    EXPECT_EQ(0xCAFEF00Du, p.colourMapId);
    EXPECT_EQ(9u, p.colourMapRevision);
    EXPECT_FLOAT_EQ(-3.0f, p.params0[0]);
    EXPECT_FLOAT_EQ(7.0f, p.params0[1]);
    EXPECT_FLOAT_EQ(0.2f, p.params0[2]);
    EXPECT_FLOAT_EQ(2.5f, p.params0[3]);
    EXPECT_FLOAT_EQ(0.0f, p.params1[0]);   // no hillshade
    EXPECT_FLOAT_EQ(0.0f, p.params1[2]);   // map mode 0
    EXPECT_FLOAT_EQ(0.0f, p.params1[3]);
    EXPECT_FLOAT_EQ(0.0f, p.params2[0]);
    EXPECT_FLOAT_EQ(1.0f, p.params2[1]);   // ambient 1 when no hillshade
    EXPECT_FLOAT_EQ(5.0f, p.gridInfo[0]);
    EXPECT_FLOAT_EQ(3.0f, p.gridInfo[1]);
    EXPECT_FLOAT_EQ(1.0f / 5.0f, p.gridInfo[2]);
    EXPECT_FLOAT_EQ(1.0f / 3.0f, p.gridInfo[3]);
    float fb[4], bd[4];
    CellFieldUnpackRgba8(0xFF8000C0u, fb);
    CellFieldUnpackRgba8(0x11223344u, bd);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(fb[i], p.fallbackColour[i]) << i;
        EXPECT_FLOAT_EQ(bd[i], p.backdropColour[i]) << i;
    }
    EXPECT_FLOAT_EQ(0.0f, p.lightDir[0]);
    EXPECT_FLOAT_EQ(0.0f, p.lightDir[1]);
    EXPECT_FLOAT_EQ(1.0f, p.lightDir[2]);
}

TEST(CellFieldGapsParams, RampBranchFillsEveryFieldAndIgnoresPalette)
{
    Dia::Core::CellFieldDrawRequest req;
    req.colourMap = 0x12345678u;
    req.rampMin = 2.0f; req.rampMax = -2.0f;       // reversed range passes through untouched
    CellFieldDrawParams p;
    const CellFieldRampData ramp = Ramp(4);
    const CellFieldPaletteData pal = Palette(4, 0xFFFFFFFFu, 77);   // supplied but must be ignored for a Float layer
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(1, 1, CellFieldValueKind::Float), &pal, &ramp, p));
    EXPECT_EQ(CellFieldMapMode::Ramp, p.mapMode);
    EXPECT_EQ(4u, p.colourMapRevision);
    EXPECT_FLOAT_EQ(2.0f, p.params0[0]);
    EXPECT_FLOAT_EQ(-2.0f, p.params0[1]);
    EXPECT_FLOAT_EQ(1.0f, p.params1[2]);   // map mode 1
    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(0.0f, p.fallbackColour[i]);   // ramps have no fallback
}

TEST(CellFieldGapsParams, MissingColourMapFailsAndZeroesOutput)
{
    Dia::Core::CellFieldDrawRequest req;
    CellFieldDrawParams p;
    p.params0[0] = 42.0f;
    p.mapMode = CellFieldMapMode::Backdrop;
    p.colourMapId = 5;
    const CellFieldRampData ramp = Ramp();
    const CellFieldPaletteData pal = Palette(2, 0);
    EXPECT_FALSE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Category), nullptr, &ramp, p));   // ramp is not a palette
    EXPECT_EQ(0.0f, p.params0[0]);
    EXPECT_EQ(CellFieldMapMode::Palette, p.mapMode);
    EXPECT_EQ(0u, p.colourMapId);
    EXPECT_FALSE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Float), &pal, nullptr, p));
    EXPECT_FALSE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Float), nullptr, nullptr, p));
}

TEST(CellFieldGapsParams, GridInfoReciprocals)
{
    Dia::Core::CellFieldDrawRequest req;
    const CellFieldRampData ramp = Ramp();
    CellFieldDrawParams p;
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(1, 1, CellFieldValueKind::Float), nullptr, &ramp, p));
    EXPECT_EQ(1.0f, p.gridInfo[0]); EXPECT_EQ(1.0f, p.gridInfo[1]);
    EXPECT_EQ(1.0f, p.gridInfo[2]); EXPECT_EQ(1.0f, p.gridInfo[3]);
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(128, 128, CellFieldValueKind::Float), nullptr, &ramp, p));
    EXPECT_EQ(128.0f, p.gridInfo[0]); EXPECT_EQ(128.0f, p.gridInfo[1]);
    EXPECT_EQ(1.0f / 128.0f, p.gridInfo[2]); EXPECT_EQ(1.0f / 128.0f, p.gridInfo[3]);
    EXPECT_EQ(1.0f, p.gridInfo[0] * p.gridInfo[2]);   // 128 is a power of two: exact
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(66, 42, CellFieldValueKind::Float), nullptr, &ramp, p));
    EXPECT_NEAR(1.0f, p.gridInfo[0] * p.gridInfo[2], 1e-6f);
    EXPECT_NEAR(1.0f, p.gridInfo[1] * p.gridInfo[3], 1e-6f);
    // Degenerate 0-size layer: no division by zero.
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(0, 0, CellFieldValueKind::Float), nullptr, &ramp, p));
    EXPECT_EQ(0.0f, p.gridInfo[2]); EXPECT_EQ(0.0f, p.gridInfo[3]);
}

TEST(CellFieldGapsParams, CellSizeZeroAndNegativePassThroughWithoutCrash)
{
    Dia::Core::CellFieldDrawRequest req;
    const CellFieldRampData ramp = Ramp();
    CellFieldDrawParams p;
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Float, 0.0f), nullptr, &ramp, p));
    EXPECT_EQ(0.0f, p.params0[3]);
    ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Float, -3.0f), nullptr, &ramp, p));
    EXPECT_EQ(-3.0f, p.params0[3]);
}

TEST(CellFieldGapsParams, InsetClampedInParams)
{
    Dia::Core::CellFieldDrawRequest req;
    const CellFieldRampData ramp = Ramp();
    CellFieldDrawParams p;
    const float cases[][2] = { { kNaN, 0.0f }, { -1.0f, 0.0f }, { 0.5f, kCellFieldMaxInset }, { 1.0e9f, kCellFieldMaxInset }, { 0.3f, 0.3f } };
    for (const auto& c : cases)
    {
        req.inset = c[0];
        ASSERT_TRUE(BuildCellFieldDrawParams(req, Info(2, 2, CellFieldValueKind::Float), nullptr, &ramp, p));
        EXPECT_FLOAT_EQ(c[1], p.params0[2]) << c[0];
    }
}

TEST(CellFieldGapsParams, HillshadeOnFillsHillshadeFieldsAndOffZeroesThem)
{
    Dia::Core::CellFieldDrawRequest req;
    req.elevationLayer.index = 3;
    req.hillshadeStrength = 0.75f;
    req.elevationScale = 4.0f;
    req.lightAzimuthDeg = 90.0f;
    req.lightElevationDeg = 0.0f;
    const CellFieldRampData ramp = Ramp();
    const CellFieldLayerInfo layer = Info(4, 4, CellFieldValueKind::Float);
    CellFieldDrawParams on, off;
    ASSERT_TRUE(BuildCellFieldDrawParams(req, layer, nullptr, &ramp, on, &layer));
    EXPECT_FLOAT_EQ(0.75f, on.params1[0]);
    EXPECT_FLOAT_EQ(4.0f, on.params1[1]);
    EXPECT_FLOAT_EQ(1.0f, on.params1[3]);
    EXPECT_FLOAT_EQ(kCellFieldHillshadeAmbient, on.params2[1]);
    EXPECT_NEAR(1.0f, on.lightDir[0], 1e-6f);   // light from the east
    EXPECT_NEAR(0.0f, on.lightDir[1], 1e-6f);
    EXPECT_NEAR(0.0f, on.lightDir[2], 1e-6f);

    ASSERT_TRUE(BuildCellFieldDrawParams(req, layer, nullptr, &ramp, off, nullptr));
    EXPECT_FLOAT_EQ(0.0f, off.params1[0]);
    EXPECT_FLOAT_EQ(4.0f, off.params1[1]);   // elevation scale passes through either way
    EXPECT_FLOAT_EQ(0.0f, off.params1[3]);
    EXPECT_FLOAT_EQ(1.0f, off.params2[1]);
    EXPECT_FLOAT_EQ(1.0f, off.lightDir[2]);
}

TEST(CellFieldGapsParams, HillshadeEnablementNeverDependsOnShadingMode)
{
    // Task 22 shader bug: elevation must always blend through the four-cell weights, so Flat shading
    // with an elevation layer still hillshades. The CPU side must never gate hillshade on `shading`.
    const CellFieldRampData ramp = Ramp();
    const CellFieldLayerInfo layer = Info(4, 3, CellFieldValueKind::Float);
    const CellFieldPaletteData pal = Palette(3, 0);
    const CellFieldLayerInfo catLayer = Info(4, 3, CellFieldValueKind::Category);
    for (auto shading : { Dia::Core::CellFieldShading::Flat, Dia::Core::CellFieldShading::Smooth })
    {
        Dia::Core::CellFieldDrawRequest req;
        req.elevationLayer.index = 1;
        req.hillshadeStrength = 0.5f;
        req.shading = shading;
        CellFieldDrawParams p;
        ASSERT_TRUE(BuildCellFieldDrawParams(req, layer, nullptr, &ramp, p, &layer));
        EXPECT_FLOAT_EQ(1.0f, p.params1[3]) << int(shading);
        EXPECT_FLOAT_EQ(0.5f, p.params1[0]) << int(shading);
        ASSERT_TRUE(BuildCellFieldDrawParams(req, catLayer, &pal, nullptr, p, &layer));
        EXPECT_FLOAT_EQ(1.0f, p.params1[3]) << int(shading);
    }
    // Full-struct equality between the two modes (params do not carry the mode at all).
    Dia::Core::CellFieldDrawRequest flat, smooth;
    flat.elevationLayer.index = smooth.elevationLayer.index = 1;
    flat.hillshadeStrength = smooth.hillshadeStrength = 0.5f;
    flat.shading = Dia::Core::CellFieldShading::Flat;
    smooth.shading = Dia::Core::CellFieldShading::Smooth;
    CellFieldDrawParams a, b;
    ASSERT_TRUE(BuildCellFieldDrawParams(flat, layer, nullptr, &ramp, a, &layer));
    ASSERT_TRUE(BuildCellFieldDrawParams(smooth, layer, nullptr, &ramp, b, &layer));
    EXPECT_EQ(0, memcmp(&a, &b, sizeof(a)));
}

TEST(CellFieldGapsParams, HillshadeStrengthAndHandleEdges)
{
    const CellFieldRampData ramp = Ramp();
    const CellFieldLayerInfo layer = Info(4, 3, CellFieldValueKind::Float);
    auto enabled = [&](float strength, bool validHandle)
    {
        Dia::Core::CellFieldDrawRequest req;
        if (validHandle) req.elevationLayer.index = 2;
        req.hillshadeStrength = strength;
        CellFieldDrawParams p;
        EXPECT_TRUE(BuildCellFieldDrawParams(req, layer, nullptr, &ramp, p, &layer));
        return p.params1[3] > 0.5f;
    };
    EXPECT_FALSE(enabled(kNaN, true));
    EXPECT_FALSE(enabled(-1.0f, true));
    EXPECT_FALSE(enabled(0.0f, true));
    EXPECT_TRUE(enabled(0.01f, true));
    EXPECT_TRUE(enabled(1.0f, true));
    EXPECT_TRUE(enabled(5.0f, true));
    EXPECT_FALSE(enabled(kInf, true));
    EXPECT_FALSE(enabled(0.5f, false));   // invalid elevation handle
}

// ===========================================================================
// Light direction + hillshade reference
// ===========================================================================

TEST(CellFieldGapsHillshade, LightDirectionAtAllEightCompassPoints)
{
    // Horizontal light (elevation 0): x = sin(az), y = -cos(az), z = 0. North is screen up (-y).
    const float s = 0.70710678f;
    struct Row { float az; float x, y; };
    const Row rows[] = {
        { 0.0f, 0.0f, -1.0f },    // N
        { 45.0f, s, -s },         // NE
        { 90.0f, 1.0f, 0.0f },    // E
        { 135.0f, s, s },         // SE
        { 180.0f, 0.0f, 1.0f },   // S
        { 225.0f, -s, s },        // SW
        { 270.0f, -1.0f, 0.0f },  // W
        { 315.0f, -s, -s },       // NW
    };
    for (const Row& r : rows)
    {
        float l[3];
        CellFieldLightDirection(r.az, 0.0f, l);
        EXPECT_NEAR(r.x, l[0], 1e-5f) << r.az;
        EXPECT_NEAR(r.y, l[1], 1e-5f) << r.az;
        EXPECT_NEAR(0.0f, l[2], 1e-5f) << r.az;
        EXPECT_NEAR(1.0f, Len3(l), 1e-5f) << r.az;
    }
}

TEST(CellFieldGapsHillshade, LightDirectionAtThePolesIgnoresAzimuth)
{
    for (float az : { 0.0f, 90.0f, 200.0f, -33.0f, 720.0f })
    {
        float l[3];
        CellFieldLightDirection(az, 90.0f, l);
        EXPECT_NEAR(0.0f, l[0], 1e-5f) << az;
        EXPECT_NEAR(0.0f, l[1], 1e-5f) << az;
        EXPECT_NEAR(1.0f, l[2], 1e-5f) << az;
        CellFieldLightDirection(az, -90.0f, l);   // below the horizon: z negative, still unit
        EXPECT_NEAR(0.0f, l[0], 1e-5f) << az;
        EXPECT_NEAR(0.0f, l[1], 1e-5f) << az;
        EXPECT_NEAR(-1.0f, l[2], 1e-5f) << az;
    }
}

TEST(CellFieldGapsHillshade, LightDirectionWrapsAzimuthAndRejectsAbsurdMagnitudes)
{
    float a[3], b[3], d[3];
    CellFieldLightDirection(40.0f, 30.0f, a);
    CellFieldLightDirection(400.0f, 30.0f, b);
    EXPECT_NEAR(a[0], b[0], 1e-4f); EXPECT_NEAR(a[1], b[1], 1e-4f); EXPECT_NEAR(a[2], b[2], 1e-4f);
    CellFieldLightDirection(kCellFieldDefaultLightAzimuthDeg, kCellFieldDefaultLightElevationDeg, d);
    CellFieldLightDirection(1.0e12f, 10.0f, a);   // > 1e9 magnitude -> defaults
    EXPECT_FLOAT_EQ(d[0], a[0]); EXPECT_FLOAT_EQ(d[1], a[1]); EXPECT_FLOAT_EQ(d[2], a[2]);
    CellFieldLightDirection(10.0f, -kInf, a);
    EXPECT_FLOAT_EQ(d[0], a[0]); EXPECT_FLOAT_EQ(d[1], a[1]); EXPECT_FLOAT_EQ(d[2], a[2]);
    CellFieldLightDirection(kNaN, kNaN, a);
    EXPECT_FLOAT_EQ(d[0], a[0]); EXPECT_FLOAT_EQ(d[1], a[1]); EXPECT_FLOAT_EQ(d[2], a[2]);
}

TEST(CellFieldGapsHillshade, ReferenceBrighterAsSlopeFacesLightUpToSaturation)
{
    float l[3];
    CellFieldLightDirection(270.0f, 30.0f, l);   // light from the west
    // Height rising with +x faces west (toward the light). Brightness peaks where the normal meets the
    // light: slope = cot(elevation) = 1.732 for a 30 degree light.
    float prev = -1.0f;
    for (float slope = 0.0f; slope <= 1.7f; slope += 0.1f)
    {
        const float shade = CellFieldHillshadeReference(slope, 0.0f, 1.0f, 1.0f, l, 1.0f);
        EXPECT_GE(shade, prev - 1e-6f) << slope;
        prev = shade;
    }
    const float peak = CellFieldHillshadeReference(1.7320508f, 0.0f, 1.0f, 1.0f, l, 1.0f);
    EXPECT_NEAR(kCellFieldHillshadeAmbient + 1.0f, peak, 1e-4f);   // n == light: dot = 1
    // Steeper than the peak falls off again but stays bounded.
    const float steeper = CellFieldHillshadeReference(50.0f, 0.0f, 1.0f, 1.0f, l, 1.0f);
    EXPECT_LT(steeper, peak);
    EXPECT_LE(steeper, kCellFieldHillshadeAmbient + 1.0f + 1e-5f);
}

TEST(CellFieldGapsHillshade, ReferenceNeverBelowAmbientAndNeverAboveAmbientPlusStrength)
{
    for (float az = 0.0f; az < 360.0f; az += 45.0f)
        for (float el = 0.0f; el <= 90.0f; el += 30.0f)
        {
            float l[3];
            CellFieldLightDirection(az, el, l);
            for (float strength : { 0.25f, 0.5f, 1.0f })
                for (float gx = -100.0f; gx <= 100.0f; gx += 12.5f)
                    for (float gy = -100.0f; gy <= 100.0f; gy += 12.5f)
                    {
                        const float s = CellFieldHillshadeReference(gx, gy, 1.0f, 1.0f, l, strength);
                        EXPECT_TRUE(s == s);
                        EXPECT_GE(s, kCellFieldHillshadeAmbient - 1e-6f);
                        EXPECT_LE(s, kCellFieldHillshadeAmbient + strength + 1e-5f);
                    }
        }
}

TEST(CellFieldGapsHillshade, ReferenceFacingAwayDecreasesTowardAmbient)
{
    float l[3];
    CellFieldLightDirection(270.0f, 30.0f, l);
    float prev = 1e9f;
    for (float slope = 0.0f; slope >= -20.0f; slope -= 1.0f)
    {
        const float shade = CellFieldHillshadeReference(slope, 0.0f, 1.0f, 1.0f, l, 1.0f);
        EXPECT_LE(shade, prev + 1e-6f) << slope;
        prev = shade;
    }
    EXPECT_NEAR(kCellFieldHillshadeAmbient, CellFieldHillshadeReference(-1.0e4f, 0.0f, 1.0f, 1.0f, l, 1.0f), 1e-3f);
}

TEST(CellFieldGapsHillshade, ReferenceDegenerateInputsFallBackToFlat)
{
    float l[3];
    CellFieldLightDirection(315.0f, 45.0f, l);
    const float flat = CellFieldHillshadeReference(0.0f, 0.0f, 1.0f, 1.0f, l, 0.5f);
    EXPECT_FLOAT_EQ(flat, CellFieldHillshadeReference(kNaN, 0.0f, 1.0f, 1.0f, l, 0.5f));
    EXPECT_FLOAT_EQ(flat, CellFieldHillshadeReference(0.0f, kInf, 1.0f, 1.0f, l, 0.5f));
    EXPECT_FLOAT_EQ(flat, CellFieldHillshadeReference(1.0f, 1.0f, kNaN, 1.0f, l, 0.5f));
    EXPECT_FLOAT_EQ(flat, CellFieldHillshadeReference(1.0f, 1.0f, 1.0f, 0.0f, l, 0.5f));   // zero pixel size
    EXPECT_FLOAT_EQ(1.0f, CellFieldHillshadeReference(1.0f, 1.0f, 1.0f, 1.0f, l, kNaN));   // NaN strength: colour unchanged
    EXPECT_FLOAT_EQ(1.0f, CellFieldHillshadeReference(1.0f, 1.0f, 1.0f, 1.0f, l, 1.0e10f));
    EXPECT_FLOAT_EQ(1.0f, CellFieldHillshadeReference(1.0f, 1.0f, 1.0f, 1.0f, l, -kInf));
}

TEST(CellFieldGapsHillshade, ReferenceNegativeScaleMirrorsSlope)
{
    float l[3];
    CellFieldLightDirection(270.0f, 30.0f, l);
    EXPECT_NEAR(CellFieldHillshadeReference(0.5f, 0.0f, 2.0f, 1.0f, l, 1.0f),
                CellFieldHillshadeReference(-0.5f, 0.0f, -2.0f, 1.0f, l, 1.0f), 1e-6f);
    EXPECT_NEAR(CellFieldHillshadeReference(0.5f, 0.0f, 2.0f, 1.0f, l, 1.0f),
                CellFieldHillshadeReference(1.0f, 0.0f, 1.0f, 1.0f, l, 1.0f), 1e-6f);   // only slope*scale matters
}

// ===========================================================================
// Ortho2D
// ===========================================================================

namespace
{
    Dia::Graphics::Camera2D Cam(float x, float y, float zoom)
    {
        Dia::Graphics::Camera2D c;
        c.SetPosition(Vector2D(x, y));
        c.SetZoom(zoom);
        return c;
    }
}

TEST(CellFieldGapsCamera, ZoomNonPositiveNaNAndHugeBehaveSanely)
{
    const Vector2D canvas(800.0f, 600.0f);
    float one[16];
    BuildOrtho2D(Cam(0, 0, 1.0f), canvas, one);
    for (float z : { 0.0f, -1.0f, -1.0e9f, kNaN, -kInf })
    {
        float m[16];
        BuildOrtho2D(Cam(0, 0, z), canvas, m);
        for (int k = 0; k < 16; ++k)
            EXPECT_EQ(one[k], m[k]) << "zoom " << z << " idx " << k;
    }
    float huge[16];
    BuildOrtho2D(Cam(0, 0, 1.0e6f), canvas, huge);   // finite and 1e6x the unit scale
    EXPECT_TRUE(std::isfinite(huge[0]) && std::isfinite(huge[5]));
    EXPECT_NEAR(one[0] * 1.0e6f, huge[0], one[0] * 1.0e6f * 1e-4f);
    float tiny[16];
    BuildOrtho2D(Cam(0, 0, 1.0e-6f), canvas, tiny);
    EXPECT_TRUE(std::isfinite(tiny[0]) && std::isfinite(tiny[5]));
    EXPECT_GT(tiny[0], 0.0f);
}

TEST(CellFieldGapsCamera, ZeroCanvasDoesNotCrash)
{
    // A zero-size canvas has no well-defined matrix; the renderer never submits it (SetupView guard).
    float m[16];
    BuildOrtho2D(Cam(5, 5, 1.0f), Vector2D(0.0f, 0.0f), m);
    const Vector2D p = WorldToPixel(Cam(5, 5, 1.0f), Vector2D(0.0f, 0.0f), Vector2D(1, 1));
    (void)p;
    SUCCEED();
}

TEST(CellFieldGapsCamera, RotationIsIgnored)
{
    const Vector2D canvas(800.0f, 600.0f);
    Dia::Graphics::Camera2D plain = Cam(10, 20, 1.5f);
    Dia::Graphics::Camera2D rotated = plain;
    rotated.SetRotation(1.2345f);
    float a[16], b[16];
    BuildOrtho2D(plain, canvas, a);
    BuildOrtho2D(rotated, canvas, b);
    for (int k = 0; k < 16; ++k)
        EXPECT_EQ(a[k], b[k]) << k;
}

TEST(CellFieldGapsCamera, WorldToPixelRoundTripOnAGridOfPoints)
{
    const Vector2D canvas(800.0f, 600.0f);
    const float cams[4][3] = { { 0, 0, 1 }, { 100, 50, 2 }, { -50, 25, 0.5f }, { 1234.5f, -987.25f, 3.5f } };
    for (const auto& c : cams)
    {
        const Dia::Graphics::Camera2D cam = Cam(c[0], c[1], c[2]);
        float m[16];
        BuildOrtho2D(cam, canvas, m);
        for (float wx = -300.0f; wx <= 300.0f; wx += 75.0f)
            for (float wy = -300.0f; wy <= 300.0f; wy += 75.0f)
            {
                const Vector2D world(c[0] + wx, c[1] + wy);
                const Vector2D px = MatrixWorldToPixel(m, canvas, world);
                // Invert analytically: pixel = canvas/2 + (world - cam) * zoom (y down).
                EXPECT_NEAR(canvas.X() * 0.5f + wx * c[2], px.X(), 0.02f);
                EXPECT_NEAR(canvas.Y() * 0.5f + wy * c[2], px.Y(), 0.02f);
                const Vector2D viaCamera = WorldToPixel(cam, canvas, world);
                EXPECT_EQ(px.X(), viaCamera.X());
                EXPECT_EQ(px.Y(), viaCamera.Y());
                // And back to world.
                EXPECT_NEAR(world.X(), c[0] + (px.X() - canvas.X() * 0.5f) / c[2], 0.02f);
                EXPECT_NEAR(world.Y(), c[1] + (px.Y() - canvas.Y() * 0.5f) / c[2], 0.02f);
            }
    }
}

TEST(CellFieldGapsCamera, ViewCornersMapToCanvasCorners)
{
    const Vector2D canvas(640.0f, 480.0f);
    const Dia::Graphics::Camera2D cam = Cam(30.0f, -10.0f, 2.0f);
    const float halfW = canvas.X() * 0.5f / 2.0f, halfH = canvas.Y() * 0.5f / 2.0f;
    const Vector2D tl = WorldToPixel(cam, canvas, Vector2D(30.0f - halfW, -10.0f - halfH));
    const Vector2D br = WorldToPixel(cam, canvas, Vector2D(30.0f + halfW, -10.0f + halfH));
    EXPECT_NEAR(0.0f, tl.X(), 1e-3f);
    EXPECT_NEAR(0.0f, tl.Y(), 1e-3f);
    EXPECT_NEAR(640.0f, br.X(), 1e-3f);
    EXPECT_NEAR(480.0f, br.Y(), 1e-3f);
}

TEST(CellFieldGapsCamera, HomogeneousDepthOnlyChangesTheDepthTerms)
{
    const Vector2D canvas(800.0f, 600.0f);
    const float cams[3][3] = { { 0, 0, 1 }, { 100, 50, 2 }, { -50, 25, 0.5f } };
    for (const auto& c : cams)
    {
        float plain[16], homo[16];
        BuildOrtho2D(Cam(c[0], c[1], c[2]), canvas, plain, false);
        BuildOrtho2D(Cam(c[0], c[1], c[2]), canvas, homo, true);
        for (int k : { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 15 })
            EXPECT_EQ(plain[k], homo[k]) << k;
        EXPECT_NEAR(0.001f, plain[10], 1e-9f);    // 1 / (far - near), near 0 far 1000
        EXPECT_NEAR(0.0f, plain[14], 1e-9f);
        EXPECT_NEAR(0.002f, homo[10], 1e-9f);     // 2 / (far - near)
        EXPECT_NEAR(-1.0f, homo[14], 1e-6f);      // -(far + near) / (far - near)
        // World-to-pixel is identical in both depth conventions.
        const Vector2D a = MatrixWorldToPixel(plain, canvas, Vector2D(c[0] + 10, c[1] - 7));
        const Vector2D b = MatrixWorldToPixel(homo, canvas, Vector2D(c[0] + 10, c[1] - 7));
        EXPECT_EQ(a.X(), b.X());
        EXPECT_EQ(a.Y(), b.Y());
    }
}

TEST(CellFieldGapsCamera, FarPlaneDepthSpansWorldZeroToOne)
{
    // A 2D point at z = 0 lands on depth 0 (non-homogeneous) or -1 (homogeneous): inside the clip volume.
    const Vector2D canvas(800.0f, 600.0f);
    float plain[16], homo[16];
    BuildOrtho2D(Cam(0, 0, 1), canvas, plain, false);
    BuildOrtho2D(Cam(0, 0, 1), canvas, homo, true);
    EXPECT_NEAR(0.0f, plain[14], 1e-9f);
    EXPECT_NEAR(-1.0f, homo[14], 1e-6f);
    EXPECT_NEAR(1.0f, plain[10] * 1000.0f + plain[14], 1e-6f);   // far plane maps to depth 1
    EXPECT_NEAR(1.0f, homo[10] * 1000.0f + homo[14], 1e-6f);     // far plane maps to NDC 1
}

// ===========================================================================
// View order
// ===========================================================================

TEST(CellFieldGapsViewOrder, PermutationNegativeCases)
{
    const unsigned short good[5] = { 4, 8, 5, 6, 7 };
    EXPECT_TRUE(IsViewOrderPermutation(good, 5, 4));
    const unsigned short dupEnd[5] = { 4, 8, 5, 6, 6 };
    EXPECT_FALSE(IsViewOrderPermutation(dupEnd, 5, 4));
    const unsigned short low[5] = { 3, 8, 5, 6, 7 };
    EXPECT_FALSE(IsViewOrderPermutation(low, 5, 4));
    const unsigned short high[5] = { 4, 9, 5, 6, 7 };
    EXPECT_FALSE(IsViewOrderPermutation(high, 5, 4));
    const unsigned short zeroStartOnFourBased[5] = { 4, 8, 5, 6, 7 };
    EXPECT_FALSE(IsViewOrderPermutation(zeroStartOnFourBased, 5, 0));   // values exceed [0,5)
    EXPECT_FALSE(IsViewOrderPermutation(good, 6, 4));                   // more than the supported count
    const unsigned short big[8] = { 4, 5, 6, 7, 8, 9, 10, 11 };
    EXPECT_FALSE(IsViewOrderPermutation(big, 8, 4));
    const unsigned short maxed[2] = { 65535, 65535 };
    EXPECT_FALSE(IsViewOrderPermutation(maxed, 2, 65535));
}

TEST(CellFieldGapsViewOrder, PermutationStartAndCountVariants)
{
    const unsigned short rev[5] = { 4, 3, 2, 1, 0 };
    EXPECT_TRUE(IsViewOrderPermutation(rev, 5, 0));
    const unsigned short three[3] = { 6, 4, 5 };
    EXPECT_TRUE(IsViewOrderPermutation(three, 3, 4));
    const unsigned short threeBad[3] = { 6, 4, 7 };
    EXPECT_FALSE(IsViewOrderPermutation(threeBad, 3, 4));
    const unsigned short single[1] = { 9 };
    EXPECT_TRUE(IsViewOrderPermutation(single, 1, 9));
    EXPECT_FALSE(IsViewOrderPermutation(single, 1, 8));
    EXPECT_TRUE(IsViewOrderPermutation(nullptr, 0, 4));   // empty remap is trivially a permutation
}

TEST(CellFieldGapsViewOrder, RemapVariantsKeepTheCellFieldBetweenEntityAndDebug)
{
    for (unsigned short start : { (unsigned short)0, (unsigned short)4, (unsigned short)100 })
    {
        const unsigned short e = start;
        const unsigned short d = static_cast<unsigned short>(start + 1);
        const unsigned short u = static_cast<unsigned short>(start + 2);
        const unsigned short g = static_cast<unsigned short>(start + 3);
        const unsigned short cf = static_cast<unsigned short>(start + 4);
        const auto remap = BuildCellFieldViewRemap(e, cf, d, u, g);
        ASSERT_EQ(5u, remap.size());
        EXPECT_EQ(e, remap[0]);
        EXPECT_EQ(cf, remap[1]);
        EXPECT_EQ(d, remap[2]);
        EXPECT_EQ(u, remap[3]);
        EXPECT_EQ(g, remap[4]);
        EXPECT_TRUE(IsViewOrderPermutation(remap.data(), remap.size(), start));
        EXPECT_EQ(remap, BuildCellFieldViewOrder(e, cf, d, u, g));
    }
    // The production constants: views 4..8, CellField is 8, order starts at 4 with 5 entries.
    EXPECT_EQ(8, kCellFieldViewId);
    EXPECT_EQ(4, kCellFieldViewOrderStart);
    EXPECT_EQ(5u, kCellFieldViewOrderCount);
    EXPECT_GE(kCellFieldViewId, kCellFieldViewOrderStart);
    EXPECT_LT(kCellFieldViewId, kCellFieldViewOrderStart + kCellFieldViewOrderCount);
}
