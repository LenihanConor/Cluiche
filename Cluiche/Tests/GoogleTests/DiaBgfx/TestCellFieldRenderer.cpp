// TestCellFieldRenderer.cpp - drain/bookkeeping half of CellFieldRenderer, tested with fake
// (GPU-free) geometry and texture factories. Draw is a safe no-op for submission when no program.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

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

    struct Fixture
    {
        FakeGeo geo;
        FakeTex tex;
        CellFieldRegistry registry;
        CellFieldRenderer renderer{ 0, nullptr, &registry, geo, tex };
        HexDescStorage storage;

        Dia::Core::CellFieldLayerHandle Make(uint32_t w, uint32_t h)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
            Dia::Core::CellFieldLayerHandle handle;
            EXPECT_TRUE(registry.CreateLayer(d, CellFieldValueKind::Float, handle).ok);
            return handle;
        }
    };
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
