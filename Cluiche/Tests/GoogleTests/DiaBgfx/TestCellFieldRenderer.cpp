// TestCellFieldRenderer.cpp - drain/bookkeeping half of CellFieldRenderer, tested with fake
// (GPU-free) geometry and texture factories. Draw is a safe no-op for submission when no program.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

#include <limits>
#include <map>
#include <set>
#include <vector>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;

namespace
{
    class FakeGeo : public ICellFieldGpuGeometryFactory
    {
    public:
        bool Create(const CellGridGeometry&, CellFieldGpuHandle& out) override { ++createCalls; out = nextHandle++; live.insert(out); return true; }
        void Destroy(CellFieldGpuHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, destroyCalls = 0;
        uint32_t nextHandle = 1;
        std::set<CellFieldGpuHandle> live;
    };

    class FakeTex : public ICellFieldGpuTextureFactory
    {
    public:
        bool Create(uint16_t w, uint16_t h, CellFieldGpuTextureHandle& out) override
        {
            ++createCalls; out = nextHandle++; live[out] = { w, h }; return true;
        }
        void Update(CellFieldGpuTextureHandle, const float*, uint32_t count) override { ++updateCalls; updatedBytes += count * 4u; }
        void Destroy(CellFieldGpuTextureHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, updateCalls = 0, destroyCalls = 0;
        uint64_t updatedBytes = 0;
        uint32_t nextHandle = 1;
        std::map<CellFieldGpuTextureHandle, std::pair<uint16_t, uint16_t>> live;
    };

    class FakeColourMap : public ICellFieldGpuColourMapFactory
    {
    public:
        bool CreateRgba8(uint32_t width, const uint32_t* texels, uint32_t count, bool, CellFieldGpuColourMapHandle& out) override
        {
            ++createCalls; out = nextHandle++;
            live[out].assign(texels, texels + count);
            EXPECT_EQ(width, count);
            return true;
        }
        void UpdateRgba8(CellFieldGpuColourMapHandle h, const uint32_t* texels, uint32_t count) override
        {
            ++updateCalls; live[h].assign(texels, texels + count);
        }
        void Destroy(CellFieldGpuColourMapHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, updateCalls = 0, destroyCalls = 0;
        uint32_t nextHandle = 1;
        std::map<CellFieldGpuColourMapHandle, std::vector<uint32_t>> live;
    };

    struct Fixture
    {
        FakeGeo geo;
        FakeTex tex;
        FakeColourMap cmap;
        CellFieldRegistry registry;
        CellFieldRenderer renderer{ 0, nullptr, &registry, geo, tex, cmap };
        HexDescStorage storage;

        Dia::Core::CellFieldLayerHandle Make(uint32_t w, uint32_t h, CellFieldValueKind kind = CellFieldValueKind::Float)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
            Dia::Core::CellFieldLayerHandle handle;
            EXPECT_TRUE(registry.CreateLayer(d, kind, handle).ok);
            return handle;
        }
    };

    CellFieldPaletteData MakePalette(uint32_t count, uint32_t fallback)
    {
        CellFieldPaletteData p;
        p.count = count;
        for (uint32_t i = 0; i < count; ++i)
            p.colours[i] = 0x11000000u + (i << 8) + 0xFFu;   // distinct per index
        p.fallback = fallback;
        return p;
    }

    Dia::Core::CellFieldDrawRequest PaletteDraw(Dia::Core::CellFieldLayerHandle h, const char* name)
    {
        Dia::Core::CellFieldDrawRequest r;
        r.layer = h;
        r.colourMap = Dia::Core::StringCRC(name).Value();
        return r;
    }
}

TEST(CellFieldRenderer, CreateAcquiresGeometryAndTextureWithoutUpload)
{
    Fixture f;
    f.Make(4, 3);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.geo.createCalls);
    EXPECT_EQ(1, f.tex.createCalls);
    EXPECT_EQ(0, f.tex.updateCalls);
    EXPECT_EQ(0u, f.registry.GetStats().uploads);
}

TEST(CellFieldRenderer, IdleFramesUploadNothing)
{
    Fixture f;
    f.Make(4, 3);
    f.renderer.DrainRegistry();
    const CellFieldStats before = f.registry.GetStats();
    CellFieldFrameData frame;
    for (int i = 0; i < 300; ++i)
    {
        f.renderer.DrainRegistry();
        f.renderer.Draw(frame);
    }
    const CellFieldStats after = f.registry.GetStats();
    EXPECT_EQ(0, f.tex.updateCalls);
    EXPECT_EQ(before.uploads, after.uploads);
    EXPECT_EQ(before.uploadedBytes, after.uploadedBytes);
    EXPECT_EQ(1, f.geo.createCalls);
}

TEST(CellFieldRenderer, DrawWithRequestDoesNotUpload)
{
    Fixture f;
    auto h = f.Make(4, 3);
    CellFieldFrameData frame;
    Dia::Core::CellFieldDrawRequest req;
    req.layer = h;
    frame.RequestDrawCellFieldData(req);
    f.renderer.Draw(frame);
    f.renderer.Draw(frame);
    EXPECT_EQ(0, f.tex.updateCalls);
}

TEST(CellFieldRenderer, UpdateUploadsOnceWithFullSize)
{
    Fixture f;
    auto h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    std::vector<float> values(12, 0.5f);
    ASSERT_TRUE(f.registry.Update(h, values.data(), 12).ok);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.tex.updateCalls);
    EXPECT_EQ(48u, f.tex.updatedBytes);
    EXPECT_EQ(1u, f.registry.GetStats().uploads);
    EXPECT_EQ(48u, f.registry.GetStats().uploadedBytes);
}

TEST(CellFieldRenderer, UpdateAfterDestroyIsSkipped)
{
    Fixture f;
    auto h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.registry.Destroy(h);
    f.renderer.DrainRegistry();
    std::vector<float> values(12, 0.5f);
    EXPECT_FALSE(f.registry.Update(h, values.data(), 12).ok);
    f.renderer.DrainRegistry();
    EXPECT_EQ(0, f.tex.updateCalls);
    EXPECT_EQ(0u, f.registry.GetStats().uploads);
}

TEST(CellFieldRenderer, DestroyReleasesGeometryAndTexture)
{
    Fixture f;
    auto a = f.Make(4, 3);
    auto b = f.Make(4, 3);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.geo.createCalls);   // shared grid size -> one build
    EXPECT_EQ(2, f.tex.createCalls);

    f.registry.Destroy(a);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.tex.destroyCalls);
    EXPECT_EQ(0, f.geo.destroyCalls);  // still referenced by b

    f.registry.Destroy(b);
    f.renderer.DrainRegistry();
    EXPECT_EQ(2, f.tex.destroyCalls);
    EXPECT_EQ(1, f.geo.destroyCalls);
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_TRUE(f.geo.live.empty());
}

TEST(CellFieldRenderer, ShutdownFreesEverything)
{
    Fixture f;
    f.Make(4, 3);
    f.Make(5, 5);
    f.renderer.DrainRegistry();
    EXPECT_EQ(2u, f.tex.live.size());
    EXPECT_EQ(2u, f.geo.live.size());
    f.renderer.Shutdown();
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_TRUE(f.geo.live.empty());
    f.renderer.Shutdown();  // idempotent
    EXPECT_EQ(2, f.tex.destroyCalls);
    EXPECT_EQ(2, f.geo.destroyCalls);
}

TEST(CellFieldRenderer, DrawSkipsStaleHandleAndCountsIt)
{
    Fixture f;
    auto h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.registry.Destroy(h);
    f.renderer.DrainRegistry();
    CellFieldFrameData frame;
    Dia::Core::CellFieldDrawRequest req;
    req.layer = h;
    frame.RequestDrawCellFieldData(req);
    f.renderer.Draw(frame);
    EXPECT_EQ(1u, f.registry.GetStats().staleHandles);
}

// ---- Palette colour mapping (Task 10) ----------------------------------------

TEST(CellFieldPaletteLookup, CategoryIdMapsToEntry)
{
    const CellFieldPaletteData p = MakePalette(5, 0xFF00FFFFu);
    EXPECT_EQ(p.colours[0], CellFieldPaletteLookup(p, 0.0f));
    EXPECT_EQ(p.colours[3], CellFieldPaletteLookup(p, 3.0f));
    EXPECT_EQ(p.colours[4], CellFieldPaletteLookup(p, 4.0f));
}

TEST(CellFieldPaletteLookup, OutOfRangeIsFallback)
{
    const CellFieldPaletteData p = MakePalette(5, 0xFF00FFFFu);
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, -1.0f));
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, 5.0f));    // >= count
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, 63.0f));   // < 64 but >= count
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, 64.0f));
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, 1.0e9f));
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, std::numeric_limits<float>::quiet_NaN()));
}

TEST(CellFieldPaletteLookup, RoundsToNearest)
{
    const CellFieldPaletteData p = MakePalette(5, 0xFF00FFFFu);
    EXPECT_EQ(p.colours[3], CellFieldPaletteLookup(p, 2.6f));
    EXPECT_EQ(p.colours[2], CellFieldPaletteLookup(p, 2.4f));
    EXPECT_EQ(p.colours[0], CellFieldPaletteLookup(p, -0.4f));
    EXPECT_EQ(0xFF00FFFFu, CellFieldPaletteLookup(p, -0.6f));
}

TEST(CellFieldDrawParams, PaletteDrawBuildsPaletteModeParams)
{
    const CellFieldPaletteData p = MakePalette(5, 0xFF8000FFu);
    Dia::Core::CellFieldDrawRequest req = PaletteDraw(Dia::Core::CellFieldLayerHandle(), "terrain");
    CellFieldLayerInfo info;
    info.width = 8; info.height = 4; info.cellSize = 2.5f; info.kind = CellFieldValueKind::Category;
    CellFieldDrawParams out;
    ASSERT_TRUE(BuildCellFieldDrawParams(req, info, &p, nullptr, out));
    EXPECT_EQ(CellFieldMapMode::Palette, out.mapMode);
    EXPECT_FLOAT_EQ(0.0f, out.params1[2]);
    EXPECT_FLOAT_EQ(0.0f, out.params1[3]);   // no elevation
    EXPECT_FLOAT_EQ(2.5f, out.params0[3]);
    EXPECT_FLOAT_EQ(8.0f, out.gridInfo[0]);
    EXPECT_FLOAT_EQ(4.0f, out.gridInfo[1]);
    EXPECT_FLOAT_EQ(0.125f, out.gridInfo[2]);
    EXPECT_FLOAT_EQ(0.25f, out.gridInfo[3]);
    EXPECT_FLOAT_EQ(1.0f, out.fallbackColour[0]);
    EXPECT_NEAR(128.0f / 255.0f, out.fallbackColour[1], 1e-5f);
    EXPECT_FLOAT_EQ(0.0f, out.fallbackColour[2]);
    EXPECT_FLOAT_EQ(1.0f, out.fallbackColour[3]);
    EXPECT_EQ(req.colourMap, out.colourMapId);
    EXPECT_EQ(p.revision, out.colourMapRevision);
}

TEST(CellFieldDrawParams, MissingColourMapFails)
{
    CellFieldLayerInfo info;
    info.kind = CellFieldValueKind::Category;
    CellFieldDrawParams out;
    EXPECT_FALSE(BuildCellFieldDrawParams(Dia::Core::CellFieldDrawRequest(), info, nullptr, nullptr, out));
}

TEST(CellFieldPaletteDraw, PaletteTextureUploadedOnceAcrossManyDraws)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    const uint32_t colours[3] = { 0xFF0000FFu, 0x00FF00FFu, 0x0000FFFFu };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("terrain"), colours, 3, 0x808080FFu).ok);
    CellFieldFrameData frame;
    frame.RequestDrawCellFieldData(PaletteDraw(h, "terrain"));
    for (int i = 0; i < 300; ++i)
        f.renderer.Draw(frame);
    EXPECT_EQ(1, f.cmap.createCalls);
    EXPECT_EQ(0, f.cmap.updateCalls);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapUploads);
    // Baked 64 texels: 3 entries then fallback padding.
    ASSERT_EQ(1u, f.cmap.live.size());
    const std::vector<uint32_t>& texels = f.cmap.live.begin()->second;
    ASSERT_EQ(64u, texels.size());
    EXPECT_EQ(0x00FF00FFu, texels[1]);
    EXPECT_EQ(0x808080FFu, texels[3]);
    EXPECT_EQ(0x808080FFu, texels[63]);
}

TEST(CellFieldPaletteDraw, SwitchingColourMapUploadsNoValueData)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    const uint32_t a[2] = { 0xFF0000FFu, 0x00FF00FFu };
    const uint32_t b[2] = { 0x0000FFFFu, 0xFFFF00FFu };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palA"), a, 2, 0).ok);
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palB"), b, 2, 0).ok);
    std::vector<float> values(12, 1.0f);
    ASSERT_TRUE(f.registry.Update(h, values.data(), 12).ok);
    f.renderer.DrainRegistry();
    const CellFieldStats before = f.registry.GetStats();
    const int valueUpdatesBefore = f.tex.updateCalls;

    for (int i = 0; i < 100; ++i)
    {
        CellFieldFrameData frame;
        frame.RequestDrawCellFieldData(PaletteDraw(h, (i & 1) ? "palA" : "palB"));
        f.renderer.Draw(frame);
    }
    const CellFieldStats after = f.registry.GetStats();
    EXPECT_EQ(before.uploadedBytes, after.uploadedBytes);
    EXPECT_EQ(before.uploads, after.uploads);
    EXPECT_EQ(valueUpdatesBefore, f.tex.updateCalls);
    EXPECT_EQ(2, f.cmap.createCalls);   // each palette texture exactly once
    EXPECT_EQ(0, f.cmap.updateCalls);
}

TEST(CellFieldPaletteDraw, ReRegisteringPaletteUploadsOneMoreTexture)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    const uint32_t a[2] = { 0xFF0000FFu, 0x00FF00FFu };
    const uint32_t a2[2] = { 0x010203FFu, 0x040506FFu };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palA"), a, 2, 0).ok);
    CellFieldFrameData frame;
    frame.RequestDrawCellFieldData(PaletteDraw(h, "palA"));
    f.renderer.Draw(frame);
    f.renderer.Draw(frame);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapUploads);

    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palA"), a2, 2, 0).ok);
    f.renderer.Draw(frame);
    f.renderer.Draw(frame);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().colourMapUploads);
    EXPECT_EQ(1, f.cmap.createCalls);
    EXPECT_EQ(1, f.cmap.updateCalls);
    EXPECT_EQ(0x010203FFu, f.cmap.live.begin()->second[0]);
}

TEST(CellFieldPaletteDraw, UnknownColourMapSkippedAndCounted)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    CellFieldFrameData frame;
    frame.RequestDrawCellFieldData(PaletteDraw(h, "nope"));
    f.renderer.Draw(frame);
    f.renderer.Draw(frame);
    f.renderer.Draw(frame);
    EXPECT_EQ(3u, f.renderer.GetRendererStats().unknownColourMaps);
    EXPECT_EQ(0, f.cmap.createCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().colourMapsResident);
}

TEST(CellFieldPaletteDraw, OutOfRangeCategoryCellsCountedOncePerValuesAndPalette)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    const uint32_t a[3] = { 1, 2, 3 };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palA"), a, 3, 0).ok);
    // 12 cells; 3, 3, -1, 70, 5 are outside a 3-entry palette (5 cells).
    std::vector<float> values = { 0, 1, 2, 3, 3, -1, 70, 5, 0, 1, 2, 0 };
    ASSERT_TRUE(f.registry.Update(h, values.data(), 12).ok);
    CellFieldFrameData frame;
    frame.RequestDrawCellFieldData(PaletteDraw(h, "palA"));
    for (int i = 0; i < 50; ++i)
        f.renderer.Draw(frame);
    EXPECT_EQ(5u, f.renderer.GetRendererStats().outOfRangeCategoryCells);

    // New values -> one more scan.
    std::vector<float> values2(12, 9.0f);
    ASSERT_TRUE(f.registry.Update(h, values2.data(), 12).ok);
    for (int i = 0; i < 50; ++i)
        f.renderer.Draw(frame);
    EXPECT_EQ(5u + 12u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
}

TEST(CellFieldPaletteDraw, ShutdownDestroysPaletteTextures)
{
    Fixture f;
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    const uint32_t a[1] = { 1 };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("palA"), a, 1, 0).ok);
    CellFieldFrameData frame;
    frame.RequestDrawCellFieldData(PaletteDraw(h, "palA"));
    f.renderer.Draw(frame);
    EXPECT_EQ(1u, f.cmap.live.size());
    f.renderer.Shutdown();
    EXPECT_TRUE(f.cmap.live.empty());
}
