// TestCellFieldGapsBgfxRenderer.cpp - Task 26B exhaustive gap tests for the GPU-free half of DiaBgfx
// CellField: geometry cache bookkeeping, renderer drain/draw edge cases and failure injection.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaBgfx/Resources/CellFieldGeometryCache.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;

namespace
{
    const float kNaN = std::numeric_limits<float>::quiet_NaN();
    const float kInf = std::numeric_limits<float>::infinity();

    class FakeGeo : public ICellFieldGpuGeometryFactory
    {
    public:
        bool Create(const CellGridGeometry&, CellFieldGpuHandle& out) override
        {
            ++createCalls;
            if (failNext > 0) { --failNext; return false; }
            out = nextHandle++;
            live.insert(out);
            return true;
        }
        void Destroy(CellFieldGpuHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, destroyCalls = 0, failNext = 0;
        uint32_t nextHandle = 1;
        std::set<CellFieldGpuHandle> live;
    };

    class FakeTex : public ICellFieldGpuTextureFactory
    {
    public:
        bool Create(uint16_t w, uint16_t h, CellFieldGpuTextureHandle& out) override
        {
            ++createCalls;
            if (failNext > 0) { --failNext; return false; }
            out = nextHandle++;
            live[out] = { w, h };
            return true;
        }
        void Update(CellFieldGpuTextureHandle h, const float* v, uint32_t count) override
        {
            ++updateCalls;
            lastValues[h].assign(v, v + count);
        }
        void Destroy(CellFieldGpuTextureHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, updateCalls = 0, destroyCalls = 0, failNext = 0;
        uint32_t nextHandle = 1;
        std::map<CellFieldGpuTextureHandle, std::pair<uint16_t, uint16_t>> live;
        std::map<CellFieldGpuTextureHandle, std::vector<float>> lastValues;
    };

    class FakeColourMap : public ICellFieldGpuColourMapFactory
    {
    public:
        bool CreateRgba8(uint32_t, const uint32_t* texels, uint32_t count, bool linear, CellFieldGpuColourMapHandle& out) override
        {
            ++createCalls;
            if (failNext > 0) { --failNext; return false; }
            out = nextHandle++;
            live[out].assign(texels, texels + count);
            linearOf[out] = linear;
            return true;
        }
        void UpdateRgba8(CellFieldGpuColourMapHandle h, const uint32_t* texels, uint32_t count) override
        {
            ++updateCalls;
            live[h].assign(texels, texels + count);
        }
        void Destroy(CellFieldGpuColourMapHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, updateCalls = 0, destroyCalls = 0, failNext = 0;
        uint32_t nextHandle = 1;
        std::map<CellFieldGpuColourMapHandle, std::vector<uint32_t>> live;
        std::map<CellFieldGpuColourMapHandle, bool> linearOf;
    };

    using Handle = Dia::Core::CellFieldLayerHandle;
    using Request = Dia::Core::CellFieldDrawRequest;

    struct RFixture
    {
        FakeGeo geo;
        FakeTex tex;
        FakeColourMap cmap;
        CellFieldRegistry registry;
        std::unique_ptr<CellFieldRenderer> rendererPtr{ new CellFieldRenderer(0, nullptr, &registry, geo, tex, cmap) };
        CellFieldRenderer& renderer{ *rendererPtr };
        HexDescStorage storage;

        Handle Make(uint32_t w, uint32_t h, CellFieldValueKind kind = CellFieldValueKind::Float)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
            Handle handle;
            EXPECT_TRUE(registry.CreateLayer(d, kind, handle).ok);
            return handle;
        }

        // The registry accepts a neighbour index outside the grid; the geometry builder rejects it.
        Handle MakeBad(uint32_t w, uint32_t h, CellFieldValueKind kind = CellFieldValueKind::Float)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
            storage.neighbours[0] = 99999;
            Handle handle;
            EXPECT_TRUE(registry.CreateLayer(d, kind, handle).ok);
            return handle;
        }

        void Draw(const Request& req)
        {
            CellFieldFrameData frame;
            frame.RequestDrawCellFieldData(req);
            renderer.Draw(frame);
        }

        void RegPal(const char* name, uint32_t count, uint32_t fallback = 0x808080FFu, uint32_t base = 0x01000000u)
        {
            uint32_t c[64];
            for (uint32_t i = 0; i < count; ++i)
                c[i] = base + (i << 8) + 0xFFu;
            ASSERT_TRUE(registry.RegisterPalette(Dia::Core::StringCRC(name), c, count, fallback).ok);
        }

        void RegRamp(const char* name, uint32_t a = 0x000000FFu, uint32_t b = 0xFFFFFFFFu)
        {
            const CellFieldRampStop stops[2] = { { 0.0f, a }, { 1.0f, b } };
            ASSERT_TRUE(registry.RegisterRamp(Dia::Core::StringCRC(name), stops, 2).ok);
        }
    };

    Request Req(Handle h, const char* colourMap)
    {
        Request r;
        r.layer = h;
        r.colourMap = Dia::Core::StringCRC(colourMap).Value();
        return r;
    }

    // ---- geometry-cache fakes ------------------------------------------------

    class CacheFactory : public ICellFieldGpuGeometryFactory
    {
    public:
        bool Create(const CellGridGeometry&, CellFieldGpuHandle& out) override
        {
            ++createCalls;
            if (failNext > 0) { --failNext; return false; }
            out = nextHandle++;
            live.insert(out);
            return true;
        }
        void Destroy(CellFieldGpuHandle h) override { ++destroyCalls; live.erase(h); }
        int createCalls = 0, destroyCalls = 0, failNext = 0;
        uint32_t nextHandle = 1;
        std::set<CellFieldGpuHandle> live;
    };

    // Builder for an arbitrary topology id; succeeds with a fixed tiny geometry or always fails.
    class FakeBuilder : public ICellGridGeometryBuilder
    {
    public:
        FakeBuilder(const char* topology, bool succeed) : mTopology(topology), mSucceed(succeed) {}
        Dia::Core::StringCRC Topology() const override { return mTopology; }
        bool Build(const CellGridDesc& desc, CellGridGeometry& out) const override
        {
            ++buildCalls;
            if (!mSucceed)
                return false;
            const uint32_t cells = static_cast<uint32_t>(desc.width) * desc.height;
            out.Allocate(3u * cells, 3u * cells);
            for (uint32_t i = 0; i < out.vertexCount; ++i) { out.vertices[i] = VertexCellField(); out.indices[i] = i; }
            return true;
        }
        mutable int buildCalls = 0;
    private:
        Dia::Core::StringCRC mTopology;
        bool mSucceed;
    };

    struct CFixture
    {
        CacheFactory factory;
        HexFlatTopOddQBuilder builder;
        CellFieldGeometryCache cache{ factory };
        HexDescStorage storage;
        CFixture() { cache.AddBuilder(&builder); }
        CellGridDesc Desc(uint32_t w, uint32_t h)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            return HexTopologyToCellGridDesc(topo, 1.0f, storage);
        }
    };
}

// ===========================================================================
// CellFieldGeometryCache
// ===========================================================================

TEST(CellFieldGapsGeometryCache, ReleaseOfUnknownKeyIsNoOp)
{
    CFixture f;
    const auto desc = f.Desc(4, 4);
    ASSERT_NE(nullptr, f.cache.Acquire(desc));
    CellFieldGeometryKey ghost;
    ghost.topology = Dia::Core::StringCRC("hex.flat.oddq");
    ghost.width = 9; ghost.height = 9;
    f.cache.Release(ghost);
    f.cache.Release(CellFieldGeometryKey());   // default key
    EXPECT_EQ(0, f.factory.destroyCalls);
    EXPECT_EQ(1u, f.cache.Stats().liveEntries);
    EXPECT_EQ(1u, f.cache.Acquire(desc)->refCount - 1u);   // refcount untouched (now 2)
}

TEST(CellFieldGapsGeometryCache, ReleaseMoreThanAcquiredDestroysOnceAndStaysConsistent)
{
    CFixture f;
    const auto desc = f.Desc(4, 4);
    f.cache.Acquire(desc);
    const auto key = CellFieldGeometryCache::MakeKey(desc);
    f.cache.Release(key);
    f.cache.Release(key);   // extra
    f.cache.Release(key);   // extra
    EXPECT_EQ(1, f.factory.destroyCalls);
    EXPECT_EQ(0u, f.cache.Stats().liveEntries);
    EXPECT_EQ(0u, f.cache.Stats().gpuBytes);
    EXPECT_TRUE(f.factory.live.empty());
    // Cache still works afterwards.
    ASSERT_NE(nullptr, f.cache.Acquire(desc));
    EXPECT_EQ(1u, f.cache.Acquire(desc)->refCount - 1u);
}

TEST(CellFieldGapsGeometryCache, AcquireAfterShutdownRebuilds)
{
    CFixture f;
    const auto desc = f.Desc(4, 4);
    f.cache.Acquire(desc);
    f.cache.Shutdown();
    EXPECT_EQ(1, f.factory.destroyCalls);
    const auto* e = f.cache.Acquire(desc);
    ASSERT_NE(nullptr, e);
    EXPECT_EQ(1u, e->refCount);
    EXPECT_EQ(2u, f.cache.Stats().geometryBuilds);
    EXPECT_EQ(1u, f.cache.Stats().liveEntries);
}

TEST(CellFieldGapsGeometryCache, ShutdownTwiceDoesNotDoubleDestroyAndDestructorAfterShutdownIsSafe)
{
    CacheFactory factory;
    HexFlatTopOddQBuilder builder;
    HexDescStorage storage;
    {
        CellFieldGeometryCache cache(factory);
        cache.AddBuilder(&builder);
        Dia::Pathfinding::HexTopology topo(3, 3);
        cache.Acquire(HexTopologyToCellGridDesc(topo, 1.0f, storage));
        cache.Shutdown();
        cache.Shutdown();
        EXPECT_EQ(1, factory.destroyCalls);
    }   // destructor Shutdown on an already-empty cache
    EXPECT_EQ(1, factory.destroyCalls);
    EXPECT_TRUE(factory.live.empty());
}

TEST(CellFieldGapsGeometryCache, DestructorFreesLiveEntries)
{
    CacheFactory factory;
    HexFlatTopOddQBuilder builder;
    HexDescStorage storage;
    {
        CellFieldGeometryCache cache(factory);
        cache.AddBuilder(&builder);
        Dia::Pathfinding::HexTopology topo(3, 3);
        cache.Acquire(HexTopologyToCellGridDesc(topo, 1.0f, storage));
        EXPECT_EQ(1u, factory.live.size());
    }
    EXPECT_TRUE(factory.live.empty());
}

TEST(CellFieldGapsGeometryCache, ThousandAcquiresNeedThousandReleases)
{
    CFixture f;
    const auto desc = f.Desc(2, 2);
    const CellFieldGeometryCache::Entry* first = nullptr;
    for (int i = 0; i < 1000; ++i)
    {
        const auto* e = f.cache.Acquire(desc);
        ASSERT_NE(nullptr, e);
        if (!first) first = e;
        EXPECT_EQ(first, e);
        EXPECT_EQ(static_cast<uint32_t>(i + 1), e->refCount);
    }
    EXPECT_EQ(1, f.factory.createCalls);
    EXPECT_EQ(1u, f.cache.Stats().geometryBuilds);
    const auto key = CellFieldGeometryCache::MakeKey(desc);
    for (int i = 0; i < 999; ++i)
        f.cache.Release(key);
    EXPECT_EQ(0, f.factory.destroyCalls);
    EXPECT_EQ(1u, f.cache.Stats().liveEntries);
    f.cache.Release(key);
    EXPECT_EQ(1, f.factory.destroyCalls);
    EXPECT_EQ(0u, f.cache.Stats().liveEntries);
}

TEST(CellFieldGapsGeometryCache, EntryPointerStableWhileOtherEntriesAreAdded)
{
    CFixture f;
    const auto* first = f.cache.Acquire(f.Desc(2, 2));
    ASSERT_NE(nullptr, first);
    for (uint32_t w = 3; w < 40; ++w)
        ASSERT_NE(nullptr, f.cache.Acquire(f.Desc(w, 2)));
    EXPECT_EQ(first, f.cache.Acquire(f.Desc(2, 2)));
    EXPECT_EQ(2u, first->refCount);
}

TEST(CellFieldGapsGeometryCache, BuilderRegisteredTwiceStillBuildsOnce)
{
    CacheFactory factory;
    HexFlatTopOddQBuilder builder;
    CellFieldGeometryCache cache(factory);
    cache.AddBuilder(&builder);
    cache.AddBuilder(&builder);
    HexDescStorage storage;
    Dia::Pathfinding::HexTopology topo(3, 3);
    const auto desc = HexTopologyToCellGridDesc(topo, 1.0f, storage);
    ASSERT_NE(nullptr, cache.Acquire(desc));
    EXPECT_EQ(1, factory.createCalls);
    EXPECT_EQ(1u, cache.Stats().geometryBuilds);
}

TEST(CellFieldGapsGeometryCache, NullBuilderIgnored)
{
    CacheFactory factory;
    CellFieldGeometryCache cache(factory);
    cache.AddBuilder(nullptr);   // must not crash later
    HexDescStorage storage;
    Dia::Pathfinding::HexTopology topo(3, 3);
    const auto desc = HexTopologyToCellGridDesc(topo, 1.0f, storage);
    EXPECT_EQ(nullptr, cache.Acquire(desc));
    EXPECT_EQ(0, factory.createCalls);
    HexFlatTopOddQBuilder builder;
    cache.AddBuilder(&builder);
    EXPECT_NE(nullptr, cache.Acquire(desc));
}

TEST(CellFieldGapsGeometryCache, FirstMatchingBuilderWins)
{
    CacheFactory factory;
    FakeBuilder a("grid.test", true);
    FakeBuilder b("grid.test", true);
    CellFieldGeometryCache cache(factory);
    cache.AddBuilder(&a);
    cache.AddBuilder(&b);
    CellGridDesc desc;
    desc.topology = Dia::Core::StringCRC("grid.test");
    desc.width = 2; desc.height = 2;
    ASSERT_NE(nullptr, cache.Acquire(desc));
    EXPECT_EQ(1, a.buildCalls);
    EXPECT_EQ(0, b.buildCalls);
}

TEST(CellFieldGapsGeometryCache, BuilderFailureReturnsNullAndLeavesNoState)
{
    CacheFactory factory;
    FakeBuilder bad("grid.bad", false);
    CellFieldGeometryCache cache(factory);
    cache.AddBuilder(&bad);
    CellGridDesc desc;
    desc.topology = Dia::Core::StringCRC("grid.bad");
    desc.width = 2; desc.height = 2;
    EXPECT_EQ(nullptr, cache.Acquire(desc));
    EXPECT_EQ(nullptr, cache.Acquire(desc));   // retried, still no entry
    EXPECT_EQ(2, bad.buildCalls);
    EXPECT_EQ(0, factory.createCalls);
    EXPECT_EQ(0u, cache.Stats().liveEntries);
    EXPECT_EQ(0u, cache.Stats().geometryBuilds);
    EXPECT_EQ(0u, cache.Stats().gpuBytes);
    cache.Release(CellFieldGeometryCache::MakeKey(desc));   // releasing a failed key is a no-op
    EXPECT_EQ(0, factory.destroyCalls);
}

TEST(CellFieldGapsGeometryCache, GpuCreateFailureRollsBackCleanly)
{
    CFixture f;
    const auto desc = f.Desc(4, 4);
    f.factory.failNext = 1;
    EXPECT_EQ(nullptr, f.cache.Acquire(desc));
    EXPECT_EQ(1, f.factory.createCalls);
    EXPECT_EQ(0, f.factory.destroyCalls);
    EXPECT_TRUE(f.factory.live.empty());
    EXPECT_EQ(0u, f.cache.Stats().liveEntries);
    EXPECT_EQ(0u, f.cache.Stats().geometryBuilds);   // a failed upload is not a build
    EXPECT_EQ(0u, f.cache.Stats().gpuBytes);
    f.cache.Release(CellFieldGeometryCache::MakeKey(desc));   // no entry: no-op
    EXPECT_EQ(0, f.factory.destroyCalls);

    const auto* e = f.cache.Acquire(desc);   // recovers on the next attempt
    ASSERT_NE(nullptr, e);
    EXPECT_EQ(1u, e->refCount);
    EXPECT_EQ(1u, f.cache.Stats().geometryBuilds);
    EXPECT_EQ(1u, f.factory.live.size());
}

TEST(CellFieldGapsGeometryCache, KeyEqualityCoversTopologyWidthHeight)
{
    CellGridDesc a;
    a.topology = Dia::Core::StringCRC("hex.flat.oddq");
    a.width = 4; a.height = 5;
    CellGridDesc same = a;
    CellGridDesc otherTopo = a;  otherTopo.topology = Dia::Core::StringCRC("square");
    CellGridDesc swapped = a;    swapped.width = 5; swapped.height = 4;
    CellGridDesc otherW = a;     otherW.width = 6;
    CellGridDesc otherH = a;     otherH.height = 6;
    const auto ka = CellFieldGeometryCache::MakeKey(a);
    EXPECT_TRUE(ka == CellFieldGeometryCache::MakeKey(same));
    EXPECT_FALSE(ka == CellFieldGeometryCache::MakeKey(otherTopo));
    EXPECT_FALSE(ka == CellFieldGeometryCache::MakeKey(swapped));
    EXPECT_FALSE(ka == CellFieldGeometryCache::MakeKey(otherW));
    EXPECT_FALSE(ka == CellFieldGeometryCache::MakeKey(otherH));
    // cellSize and the data pointers are NOT part of the key.
    CellGridDesc otherSize = a;  otherSize.cellSize = 7.0f;
    EXPECT_TRUE(ka == CellFieldGeometryCache::MakeKey(otherSize));
}

TEST(CellFieldGapsGeometryCache, SwappedWidthHeightAreSeparateEntries)
{
    CFixture f;
    const auto* a = f.cache.Acquire(f.Desc(4, 5));
    const auto* b = f.cache.Acquire(f.Desc(5, 4));
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);
    EXPECT_NE(a, b);
    EXPECT_NE(a->gpu, b->gpu);
    EXPECT_EQ(2u, f.cache.Stats().liveEntries);
}

TEST(CellFieldGapsGeometryCache, TwoTopologiesSameSizeAreSeparateEntries)
{
    CacheFactory factory;
    HexFlatTopOddQBuilder hex;
    FakeBuilder other("grid.other", true);
    CellFieldGeometryCache cache(factory);
    cache.AddBuilder(&hex);
    cache.AddBuilder(&other);
    HexDescStorage storage;
    Dia::Pathfinding::HexTopology topo(3, 3);
    CellGridDesc hexDesc = HexTopologyToCellGridDesc(topo, 1.0f, storage);
    CellGridDesc otherDesc = hexDesc;
    otherDesc.topology = Dia::Core::StringCRC("grid.other");

    const auto* a = cache.Acquire(hexDesc);
    const auto* b = cache.Acquire(otherDesc);
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);
    EXPECT_NE(a, b);
    EXPECT_NE(a->gpu, b->gpu);
    EXPECT_EQ(2, factory.createCalls);
    EXPECT_EQ(2u, cache.Stats().liveEntries);
    EXPECT_EQ(18u * 9u, a->vertexCount);
    EXPECT_EQ(27u, b->vertexCount);

    cache.Release(CellFieldGeometryCache::MakeKey(hexDesc));
    EXPECT_EQ(1u, cache.Stats().liveEntries);
    EXPECT_EQ(1u, factory.live.size());
    EXPECT_EQ(1u, factory.live.count(b->gpu));
}

TEST(CellFieldGapsGeometryCache, MemoryStatsExactPerHex)
{
    // 18 vertices per hex at 36 bytes = 648 bytes of vertex data per hex, plus the uint32 indices.
    const uint32_t sizes[4][2] = { { 1, 1 }, { 2, 2 }, { 5, 4 }, { 66, 42 } };
    for (const auto& s : sizes)
    {
        CFixture f;
        const uint32_t cells = s[0] * s[1];
        const auto* e = f.cache.Acquire(f.Desc(s[0], s[1]));
        ASSERT_NE(nullptr, e);
        EXPECT_EQ(18u * cells, e->vertexCount) << s[0] << "x" << s[1];
        EXPECT_EQ(648ull * cells, uint64_t(e->vertexCount) * sizeof(VertexCellField)) << s[0] << "x" << s[1];
        EXPECT_EQ(uint64_t(e->vertexCount) * 36u + uint64_t(e->indexCount) * 4u, e->bytes);
        EXPECT_EQ(e->bytes, f.cache.Stats().gpuBytes);
        EXPECT_EQ(0u, e->indexCount % cells);   // whole number of indices per hex
    }
}

TEST(CellFieldGapsGeometryCache, GpuBytesSumAcrossEntriesAndDropOnRelease)
{
    CFixture f;
    const auto* a = f.cache.Acquire(f.Desc(4, 4));
    const auto* b = f.cache.Acquire(f.Desc(6, 3));
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);
    const uint64_t ab = a->bytes + b->bytes;
    const uint64_t bBytes = b->bytes;
    EXPECT_EQ(ab, f.cache.Stats().gpuBytes);
    f.cache.Release(CellFieldGeometryCache::MakeKey(f.Desc(4, 4)));
    EXPECT_EQ(bBytes, f.cache.Stats().gpuBytes);
    EXPECT_EQ(2u, f.cache.Stats().geometryBuilds);   // cumulative: never decreases
}

// ===========================================================================
// CellFieldRenderer: failure and lifecycle edges
// ===========================================================================

TEST(CellFieldGapsRenderer, UnbuiltLayerFullLifecycleIsSafeAndLeakFree)
{
    RFixture f;
    f.RegRamp("r");
    Handle bad = f.MakeBad(2, 2);
    f.renderer.DrainRegistry();
    EXPECT_EQ(0, f.geo.createCalls);          // builder rejected before any GPU call
    EXPECT_EQ(0, f.tex.createCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().valueTexturesResident);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().geometryEntries);

    const float v[4] = { 1, 2, 3, 4 };
    EXPECT_TRUE(f.registry.Update(bad, v, 4).ok);
    f.Draw(Req(bad, "r"));
    EXPECT_EQ(2u, f.renderer.GetRendererStats().unbuiltLayerRequests);   // the Update and the draw
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_TRUE(f.renderer.GetLastPlannedSubmits().empty());

    // Destroy of an unbuilt layer is silent: no GPU calls, no extra unbuilt count.
    f.registry.Destroy(bad);
    f.renderer.DrainRegistry();
    EXPECT_EQ(0, f.geo.destroyCalls);
    EXPECT_EQ(0, f.tex.destroyCalls);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().unbuiltLayerRequests);

    // The freed slot is reusable by a good layer.
    Handle good = f.Make(3, 3);
    f.Draw(Req(good, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
    f.renderer.Shutdown();
    EXPECT_TRUE(f.geo.live.empty());
    EXPECT_TRUE(f.tex.live.empty());
}

TEST(CellFieldGapsRenderer, UnbuiltLayerAsElevationDrawsColourLayerWithoutHillshade)
{
    RFixture f;
    f.RegRamp("r");
    Handle colour = f.Make(4, 3);
    Handle bad = f.MakeBad(2, 2);
    f.renderer.DrainRegistry();
    Request req = Req(colour, "r");
    req.elevationLayer = bad;
    req.hillshadeStrength = 0.5f;
    f.Draw(req);
    ASSERT_EQ(1u, f.renderer.GetLastPlannedSubmits().size());
    EXPECT_FALSE(f.renderer.GetLastPlannedSubmits()[0].hillshade);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, UpdateThenDestroyBothDrainedAppliesUpdateThenFrees)
{
    RFixture f;
    Handle h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    std::vector<float> v(12, 1.0f);
    ASSERT_TRUE(f.registry.Update(h, v.data(), 12).ok);
    f.registry.Destroy(h);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.tex.updateCalls);
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_TRUE(f.geo.live.empty());
}

TEST(CellFieldGapsRenderer, UpdateAfterDestroyBeforeDrainIsNeverUploaded)
{
    RFixture f;
    Handle h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.registry.Destroy(h);   // queued, not yet drained: handle is already stale for new calls
    std::vector<float> v(12, 1.0f);
    f.registry.Update(h, v.data(), 12);
    f.renderer.DrainRegistry();
    EXPECT_EQ(0, f.tex.updateCalls);
    EXPECT_TRUE(f.tex.live.empty());
}

TEST(CellFieldGapsRenderer, DestroyThenCreateInSameDrainKeepsOneGeometryAndOneTexture)
{
    RFixture f;
    Handle a = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.registry.Destroy(a);
    Handle b = f.Make(4, 3);
    f.renderer.DrainRegistry();
    EXPECT_EQ(1u, f.geo.live.size());
    EXPECT_EQ(1u, f.tex.live.size());
    EXPECT_EQ(1u, f.renderer.GetRendererStats().valueTexturesResident);
    f.RegRamp("r");
    f.Draw(Req(b, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, ThirtyTwoLayersShareGeometryAndOwnTheirTextures)
{
    RFixture f;
    std::vector<Handle> layers;
    for (uint32_t i = 0; i < CellFieldRegistry::kMaxLayers; ++i)
        layers.push_back(f.Make(4, 4));
    f.renderer.DrainRegistry();
    EXPECT_EQ(1, f.geo.createCalls);
    EXPECT_EQ(32, f.tex.createCalls);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().geometryEntries);
    EXPECT_EQ(32u, f.renderer.GetRendererStats().valueTexturesResident);

    for (uint32_t i = 0; i < layers.size(); ++i)
    {
        std::vector<float> v(16, static_cast<float>(i));
        ASSERT_TRUE(f.registry.Update(layers[i], v.data(), 16).ok);
    }
    f.renderer.DrainRegistry();
    EXPECT_EQ(32, f.tex.updateCalls);
    // Independent value textures: each holds its own layer's constant.
    std::set<float> seen;
    for (const auto& kv : f.tex.lastValues)
    {
        ASSERT_EQ(16u, kv.second.size());
        seen.insert(kv.second[0]);
    }
    EXPECT_EQ(32u, seen.size());

    for (size_t i = 0; i < layers.size(); ++i)
    {
        f.registry.Destroy(layers[i]);
        f.renderer.DrainRegistry();
        EXPECT_EQ(i + 1 < layers.size() ? 1u : 0u, static_cast<unsigned>(f.geo.live.size())) << i;
    }
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_EQ(0u, f.renderer.GetRendererStats().geometryBytes);
}

TEST(CellFieldGapsRenderer, NullRegistryRendererIsInert)
{
    FakeGeo geo; FakeTex tex; FakeColourMap cmap;
    CellFieldRenderer renderer(0, nullptr, nullptr, geo, tex, cmap);
    CellFieldFrameData frame;
    Request r;
    r.colourMap = 1;
    frame.RequestDrawCellFieldData(r);
    renderer.DrainRegistry();
    renderer.Draw(frame);
    const CellFieldRendererStats s = renderer.GetRendererStats();
    EXPECT_EQ(0u, s.drawCalls);
    EXPECT_EQ(0u, s.unknownColourMaps);
    EXPECT_EQ(0u, s.valueTexturesResident);
    renderer.Shutdown();
    renderer.Shutdown();
    EXPECT_EQ(0, geo.createCalls + tex.createCalls + cmap.createCalls);
}

// ---- Out-of-range counting semantics --------------------------------------------------------

namespace
{
    // Draws a 4x3 Category layer holding `values` against a palette of `paletteCount` colours and
    // returns the out-of-range count.
    uint64_t CountOutOfRange(const std::vector<float>& values, uint32_t paletteCount)
    {
        RFixture f;
        f.RegPal("p", paletteCount);
        Handle h = f.Make(4, 3, CellFieldValueKind::Category);
        f.renderer.DrainRegistry();
        EXPECT_TRUE(f.registry.Update(h, values.data(), static_cast<uint32_t>(values.size())).ok);
        f.Draw(Req(h, "p"));
        return f.renderer.GetRendererStats().outOfRangeCategoryCells;
    }
}

TEST(CellFieldGapsRenderer, OutOfRangeBoundaryValuesPalette64)
{
    // in range: 0, 63, -0.5 (rounds to 0), 63.49, 0.49
    // out of range: 64, -0.51 (rounds to -1), 63.5 (rounds to 64), 1e9
    const std::vector<float> v = { 0.0f, 63.0f, 64.0f, -0.5f, 63.5f, 1e9f, -0.51f, 63.49f, 0.49f, 0.0f, 0.0f, 0.0f };
    EXPECT_EQ(4u, CountOutOfRange(v, 64));
}

TEST(CellFieldGapsRenderer, OutOfRangeBoundaryValuesSmallPalette)
{
    // Palette of 10: valid 0..9 (9.49 still 9); 9.5 rounds to 10 and is out of range.
    const std::vector<float> v = { 0.0f, 9.0f, 9.49f, 9.5f, 10.0f, 63.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f };
    EXPECT_EQ(3u, CountOutOfRange(v, 10));
}

TEST(CellFieldGapsRenderer, NonFiniteCategoryValuesCountAsOutOfRangeAndDoNotCrash)
{
    const std::vector<float> v = { kNaN, kInf, -kInf, 0.0f, 1.0f, 2.0f, -kNaN, 1e30f, -1e30f, 1.0f, 1.0f, 1.0f };
    EXPECT_EQ(6u, CountOutOfRange(v, 3));
}

TEST(CellFieldGapsRenderer, NonFiniteFloatValuesUploadVerbatimAndCountNothing)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    std::vector<float> v(12, 0.5f);
    v[0] = kNaN; v[1] = kInf; v[2] = -kInf;
    ASSERT_TRUE(f.registry.Update(h, v.data(), 12).ok);
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1, f.tex.updateCalls);
    ASSERT_EQ(1u, f.tex.lastValues.size());
    const auto& got = f.tex.lastValues.begin()->second;
    EXPECT_TRUE(got[0] != got[0]);
    EXPECT_EQ(kInf, got[1]);
    EXPECT_EQ(-kInf, got[2]);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, OutOfRangeRescansWhenPaletteIdOrRevisionChanges)
{
    RFixture f;
    f.RegPal("small", 2);   // values 0..1 valid
    f.RegPal("big", 8);
    Handle h = f.Make(4, 3, CellFieldValueKind::Category);
    f.renderer.DrainRegistry();
    const std::vector<float> v = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 0, 1 };   // 6 outside "small", 0 outside "big"
    ASSERT_TRUE(f.registry.Update(h, v.data(), 12).ok);

    f.Draw(Req(h, "small"));
    EXPECT_EQ(6u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
    f.Draw(Req(h, "small"));
    EXPECT_EQ(6u, f.renderer.GetRendererStats().outOfRangeCategoryCells);   // same key: no rescan
    f.Draw(Req(h, "big"));                                                  // new palette id: rescan, 0 outside
    EXPECT_EQ(6u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
    f.Draw(Req(h, "small"));                                                // back: rescans, counted again
    EXPECT_EQ(12u, f.renderer.GetRendererStats().outOfRangeCategoryCells);

    f.RegPal("small", 3);                                                   // revision change: rescan, 5 outside
    f.Draw(Req(h, "small"));
    EXPECT_EQ(17u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
}

// ---- Colour-map resolution edges -----------------------------------------------------------

TEST(CellFieldGapsRenderer, PaletteRevisionChangeReuploadsExactlyOnce)
{
    RFixture f;
    f.RegPal("p", 4, 0x808080FFu, 0x01000000u);
    Handle h = f.Make(4, 3, CellFieldValueKind::Category);
    for (int i = 0; i < 5; ++i)
        f.Draw(Req(h, "p"));
    EXPECT_EQ(1, f.cmap.createCalls);
    EXPECT_EQ(0, f.cmap.updateCalls);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapUploads);

    f.RegPal("p", 4, 0x808080FFu, 0x7F000000u);   // re-register with different colours
    for (int i = 0; i < 20; ++i)
        f.Draw(Req(h, "p"));
    EXPECT_EQ(1, f.cmap.createCalls);              // same texture reused
    EXPECT_EQ(1, f.cmap.updateCalls);              // updated exactly once
    EXPECT_EQ(2u, f.renderer.GetRendererStats().colourMapUploads);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapsResident);
    ASSERT_EQ(1u, f.cmap.live.size());
    EXPECT_EQ(0x7F0000FFu, f.cmap.live.begin()->second[0]);
}

TEST(CellFieldGapsRenderer, RampRevisionChangeReuploadsExactlyOnce)
{
    RFixture f;
    f.RegRamp("r", 0x000000FFu, 0xFFFFFFFFu);
    Handle h = f.Make(4, 3);
    for (int i = 0; i < 5; ++i)
        f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapUploads);
    f.RegRamp("r", 0xFF0000FFu, 0x0000FFFFu);
    for (int i = 0; i < 20; ++i)
        f.Draw(Req(h, "r"));
    EXPECT_EQ(1, f.cmap.createCalls);
    EXPECT_EQ(1, f.cmap.updateCalls);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().colourMapUploads);
    EXPECT_EQ(0xFF0000FFu, f.cmap.live.begin()->second[0]);
    EXPECT_EQ(0x0000FFFFu, f.cmap.live.begin()->second[255]);
}

TEST(CellFieldGapsRenderer, RampAndPaletteSharingAnIdAreIndependent)
{
    RFixture f;
    f.RegPal("shared", 3);
    f.RegRamp("shared");
    Handle cat = f.Make(4, 3, CellFieldValueKind::Category);
    Handle flt = f.Make(4, 3, CellFieldValueKind::Float);
    f.Draw(Req(cat, "shared"));
    f.Draw(Req(flt, "shared"));
    EXPECT_EQ(2, f.cmap.createCalls);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().colourMapsResident);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().unknownColourMaps);
    bool sawLinear = false, sawPoint = false;
    for (const auto& kv : f.cmap.linearOf)
        (kv.second ? sawLinear : sawPoint) = true;
    EXPECT_TRUE(sawLinear && sawPoint);

    // Re-registering only the ramp must not touch the palette texture.
    f.RegRamp("shared", 0x111111FFu, 0x222222FFu);
    f.Draw(Req(cat, "shared"));
    f.Draw(Req(flt, "shared"));
    EXPECT_EQ(1, f.cmap.updateCalls);
    EXPECT_EQ(2, f.cmap.createCalls);
}

TEST(CellFieldGapsRenderer, KindMismatchBothWaysSkippedAndCounted)
{
    RFixture f;
    f.RegPal("onlyPalette", 3);
    f.RegRamp("onlyRamp");
    Handle cat = f.Make(4, 3, CellFieldValueKind::Category);
    Handle flt = f.Make(4, 3, CellFieldValueKind::Float);
    f.Draw(Req(cat, "onlyRamp"));       // ramp id on a Category layer
    EXPECT_EQ(1u, f.renderer.GetRendererStats().unknownColourMaps);
    f.Draw(Req(flt, "onlyPalette"));    // palette id on a Float layer
    EXPECT_EQ(2u, f.renderer.GetRendererStats().unknownColourMaps);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_TRUE(f.renderer.GetLastPlannedSubmits().empty());
    EXPECT_EQ(0, f.cmap.createCalls);
    f.Draw(Req(cat, "onlyPalette"));    // matching kinds still work afterwards
    f.Draw(Req(flt, "onlyRamp"));
    EXPECT_EQ(2u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, ColourMapZeroIsUnknown)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    Request req;
    req.layer = h;   // colourMap left 0
    f.Draw(req);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().unknownColourMaps);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, InvalidStaleAndForeignHandlesAreSkippedWithoutCrash)
{
    RFixture f;
    f.RegRamp("r");
    Handle live = f.Make(4, 3);
    f.renderer.DrainRegistry();

    Handle wrongGen = live;
    wrongGen.generation = static_cast<uint16_t>(live.generation + 1);
    Handle outOfTable; outOfTable.index = 31; outOfTable.generation = 0;
    Handle beyondTable; beyondTable.index = 5000; beyondTable.generation = 7;
    Handle invalid;   // 0xFFFF

    const Handle bad[4] = { wrongGen, outOfTable, beyondTable, invalid };
    for (const Handle& h : bad)
    {
        f.Draw(Req(h, "r"));
        EXPECT_TRUE(f.renderer.GetLastPlannedSubmits().empty());
    }
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);

    f.Draw(Req(live, "r"));   // the good layer is unaffected
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

// ---- Elevation edges -----------------------------------------------------------------------

TEST(CellFieldGapsRenderer, ElevationLayerIsTheColourLayerItself)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    Request req = Req(h, "r");
    req.elevationLayer = h;
    req.hillshadeStrength = 0.5f;
    f.Draw(req);
    ASSERT_EQ(1u, f.renderer.GetLastPlannedSubmits().size());
    EXPECT_TRUE(f.renderer.GetLastPlannedSubmits()[0].hillshade);
    EXPECT_EQ(0, f.cmap.updateCalls);
}

TEST(CellFieldGapsRenderer, CategoryColourLayerWithFloatElevationHillshades)
{
    RFixture f;
    f.RegPal("p", 3);
    Handle cat = f.Make(4, 3, CellFieldValueKind::Category);
    Handle elev = f.Make(4, 3, CellFieldValueKind::Float);
    Request req = Req(cat, "p");
    req.elevationLayer = elev;
    req.hillshadeStrength = 0.5f;
    f.Draw(req);
    ASSERT_EQ(1u, f.renderer.GetLastPlannedSubmits().size());
    EXPECT_TRUE(f.renderer.GetLastPlannedSubmits()[0].hillshade);
}

TEST(CellFieldGapsRenderer, ElevationCategoryLayerAndDifferentGridDisableHillshadeButColourDraws)
{
    RFixture f;
    f.RegRamp("r");
    Handle colour = f.Make(4, 3);
    Handle catElev = f.Make(4, 3, CellFieldValueKind::Category);
    Handle bigElev = f.Make(8, 8);
    Handle tallElev = f.Make(4, 4);
    for (Handle e : { catElev, bigElev, tallElev })
    {
        Request req = Req(colour, "r");
        req.elevationLayer = e;
        req.hillshadeStrength = 1.0f;
        f.Draw(req);
        ASSERT_EQ(1u, f.renderer.GetLastPlannedSubmits().size());
        EXPECT_FALSE(f.renderer.GetLastPlannedSubmits()[0].hillshade);
    }
    EXPECT_EQ(3u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, StaleElevationCountedOncePerDrawAndColourStillDrawn)
{
    RFixture f;
    f.RegRamp("r");
    Handle colour = f.Make(4, 3);
    Handle elev = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.registry.Destroy(elev);
    f.renderer.DrainRegistry();
    Request req = Req(colour, "r");
    req.elevationLayer = elev;
    req.hillshadeStrength = 0.5f;
    for (int i = 0; i < 3; ++i)
        f.Draw(req);
    EXPECT_EQ(3u, f.registry.GetStats().staleHandles);
    EXPECT_EQ(3u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_FALSE(f.renderer.GetLastPlannedSubmits()[0].hillshade);
}

TEST(CellFieldGapsRenderer, HillshadeStrengthEdgeValues)
{
    RFixture f;
    f.RegRamp("r");
    Handle colour = f.Make(4, 3);
    Handle elev = f.Make(4, 3);
    auto hillshadeFor = [&](float strength)
    {
        Request req = Req(colour, "r");
        req.elevationLayer = elev;
        req.hillshadeStrength = strength;
        f.Draw(req);
        return !f.renderer.GetLastPlannedSubmits().empty() && f.renderer.GetLastPlannedSubmits().back().hillshade;
    };
    EXPECT_FALSE(hillshadeFor(kNaN));
    EXPECT_FALSE(hillshadeFor(-0.5f));
    EXPECT_FALSE(hillshadeFor(-kInf));
    EXPECT_FALSE(hillshadeFor(0.0f));
    EXPECT_TRUE(hillshadeFor(1.0e-6f));
    EXPECT_TRUE(hillshadeFor(1.0f));
    EXPECT_TRUE(hillshadeFor(2.0f));      // above the documented [0,1]: accepted, not clamped
    EXPECT_FALSE(hillshadeFor(kInf));     // absurd magnitudes are rejected
    EXPECT_EQ(8u, f.renderer.GetRendererStats().drawCalls);   // colour layer drawn every time
}

TEST(CellFieldGapsRenderer, ShadingModeNeverAffectsHillshadeEnablement)
{
    RFixture f;
    f.RegRamp("r");
    Handle colour = f.Make(4, 3);
    Handle elev = f.Make(4, 3);
    for (auto shading : { Dia::Core::CellFieldShading::Flat, Dia::Core::CellFieldShading::Smooth })
    {
        Request req = Req(colour, "r");
        req.elevationLayer = elev;
        req.hillshadeStrength = 0.6f;
        req.shading = shading;
        f.Draw(req);
        ASSERT_EQ(1u, f.renderer.GetLastPlannedSubmits().size());
        EXPECT_TRUE(f.renderer.GetLastPlannedSubmits()[0].hillshade) << int(shading);
    }
}

// ---- Inset / backdrop planning -------------------------------------------------------------

TEST(CellFieldGapsRenderer, InsetEdgeValuesDecideBackdropPlanning)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    auto backdropFor = [&](float inset, uint32_t colour)
    {
        Request req = Req(h, "r");
        req.inset = inset;
        req.backdropColour = colour;
        f.Draw(req);
        const auto& plan = f.renderer.GetLastPlannedSubmits();
        return plan.size() == 2 && plan[0].backdrop && !plan[1].backdrop;
    };
    EXPECT_FALSE(backdropFor(kNaN, 0x000000FFu));
    EXPECT_FALSE(backdropFor(-1.0f, 0x000000FFu));
    EXPECT_FALSE(backdropFor(0.0f, 0x000000FFu));
    EXPECT_TRUE(backdropFor(0.001f, 0x000000FFu));
    EXPECT_TRUE(backdropFor(0.5f, 0x000000FFu));
    EXPECT_TRUE(backdropFor(1.0e9f, 0x000000FFu));
    EXPECT_TRUE(backdropFor(kInf, 0x000000FFu));
    EXPECT_FALSE(backdropFor(-kInf, 0x000000FFu));
}

TEST(CellFieldGapsRenderer, BackdropAlphaEdgeValues)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    auto backdropFor = [&](uint32_t colour)
    {
        Request req = Req(h, "r");
        req.inset = 0.1f;
        req.backdropColour = colour;
        f.Draw(req);
        return f.renderer.GetLastPlannedSubmits().size() == 2;
    };
    EXPECT_FALSE(backdropFor(0x00000000u));
    EXPECT_FALSE(backdropFor(0xFFFFFF00u));   // colour set, alpha 0
    EXPECT_TRUE(backdropFor(0x00000001u));
    EXPECT_TRUE(backdropFor(0x000000FFu));
    EXPECT_TRUE(backdropFor(0xFFFFFFFFu));
}

TEST(CellFieldGapsRenderer, PlanIsPerDrawNotAccumulatedAndStatsAccumulate)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    Request req = Req(h, "r");
    req.inset = 0.1f;
    req.backdropColour = 0x000000FFu;
    f.Draw(req);
    f.Draw(req);   // twice in one "frame" from the caller's perspective
    EXPECT_EQ(2u, f.renderer.GetLastPlannedSubmits().size());
    EXPECT_EQ(4u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().backdropDraws);

    CellFieldFrameData empty;
    f.renderer.Draw(empty);   // an empty frame clears the plan
    EXPECT_TRUE(f.renderer.GetLastPlannedSubmits().empty());
    EXPECT_EQ(4u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, DrawBeforeAnyDrainDrainsFirst)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);   // never explicitly drained
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(1, f.geo.createCalls);
}

TEST(CellFieldGapsRenderer, DrawWithNoLayersAtAllIsHarmless)
{
    RFixture f;
    f.RegRamp("r");
    f.Draw(Req(Handle(), "r"));
    CellFieldFrameData empty;
    f.renderer.Draw(empty);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().valueTexturesResident);
}

// ---- Shutdown ------------------------------------------------------------------------------

TEST(CellFieldGapsRenderer, ShutdownThenDrawAndDrainDoesNotCrashAndCountsUnbuilt)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    f.Draw(Req(h, "r"));
    f.renderer.Shutdown();
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_TRUE(f.geo.live.empty());
    EXPECT_TRUE(f.cmap.live.empty());

    f.renderer.DrainRegistry();
    f.Draw(Req(h, "r"));   // registry still has the layer; the renderer lost it at Shutdown
    EXPECT_EQ(1u, f.renderer.GetRendererStats().unbuiltLayerRequests);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);   // only the draw before Shutdown
}

TEST(CellFieldGapsRenderer, ShutdownThenNewLayerWorksAndDestructorFreesEverything)
{
    RFixture f;
    f.RegRamp("r");
    f.Draw(Req(f.Make(4, 3), "r"));
    f.renderer.Shutdown();
    Handle again = f.Make(5, 5);
    f.Draw(Req(again, "r"));
    EXPECT_EQ(2u, f.renderer.GetRendererStats().drawCalls);
    f.rendererPtr.reset();   // destructor after Shutdown
    EXPECT_TRUE(f.tex.live.empty());
    EXPECT_TRUE(f.geo.live.empty());
    EXPECT_TRUE(f.cmap.live.empty());
    EXPECT_EQ(f.tex.createCalls, f.tex.destroyCalls);
    EXPECT_EQ(f.geo.createCalls, f.geo.destroyCalls);
    EXPECT_EQ(f.cmap.createCalls, f.cmap.destroyCalls);
}

TEST(CellFieldGapsRenderer, ShutdownTwiceAndDestructorDoNotDoubleDestroy)
{
    RFixture f;
    f.RegRamp("r");
    f.RegPal("p", 3);
    Handle a = f.Make(4, 3);
    Handle b = f.Make(4, 3, CellFieldValueKind::Category);
    f.Draw(Req(a, "r"));
    f.Draw(Req(b, "p"));
    f.renderer.Shutdown();
    const int t = f.tex.destroyCalls, g = f.geo.destroyCalls, c = f.cmap.destroyCalls;
    EXPECT_EQ(3, t);   // two value textures + dummy elevation
    EXPECT_EQ(1, g);
    EXPECT_EQ(2, c);
    f.renderer.Shutdown();
    f.rendererPtr.reset();
    EXPECT_EQ(t, f.tex.destroyCalls);
    EXPECT_EQ(g, f.geo.destroyCalls);
    EXPECT_EQ(c, f.cmap.destroyCalls);
}

TEST(CellFieldGapsRenderer, DummyElevationCreatedOnceAcrossManyDrawsAndRecreatedAfterShutdown)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    for (int i = 0; i < 200; ++i)
        f.Draw(Req(h, "r"));
    EXPECT_EQ(2, f.tex.createCalls);   // layer + one dummy
    EXPECT_EQ(2u, f.renderer.GetRendererStats().valueTexturesResident);
    f.renderer.Shutdown();
    EXPECT_EQ(2, f.tex.destroyCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().valueTexturesResident);
    Handle h2 = f.Make(4, 3);
    for (int i = 0; i < 50; ++i)
        f.Draw(Req(h2, "r"));
    EXPECT_EQ(4, f.tex.createCalls);   // new layer + a second dummy
    f.rendererPtr.reset();
    EXPECT_EQ(4, f.tex.destroyCalls);
    EXPECT_TRUE(f.tex.live.empty());
}

// ---- GPU factory failure injection ---------------------------------------------------------

TEST(CellFieldGapsRenderer, ValueTextureCreateFailureReleasesGeometryAndRecovers)
{
    RFixture f;
    f.RegRamp("r");
    f.tex.failNext = 1;
    Handle h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    EXPECT_TRUE(f.geo.live.empty() || f.geo.destroyCalls == f.geo.createCalls);   // geometry rolled back
    EXPECT_EQ(0u, f.renderer.GetRendererStats().geometryEntries);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().valueTexturesResident);
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().unbuiltLayerRequests);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);

    Handle ok = f.Make(4, 3);
    f.Draw(Req(ok, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, GeometryUploadFailureDropsLayerAndLaterLayerSucceeds)
{
    RFixture f;
    f.RegRamp("r");
    f.geo.failNext = 1;
    Handle h = f.Make(4, 3);
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().unbuiltLayerRequests);
    EXPECT_EQ(0, f.tex.createCalls);
    Handle ok = f.Make(4, 3);
    f.Draw(Req(ok, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, ColourMapCreateFailureSkipsDrawWithoutLeakAndRecovers)
{
    RFixture f;
    f.RegPal("p", 3);
    Handle h = f.Make(4, 3, CellFieldValueKind::Category);
    const std::vector<float> v(12, 9.0f);
    ASSERT_TRUE(f.registry.Update(h, v.data(), 12).ok);
    f.cmap.failNext = 1;
    f.Draw(Req(h, "p"));
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().colourMapsResident);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().colourMapUploads);
    EXPECT_EQ(0u, f.renderer.GetRendererStats().outOfRangeCategoryCells);   // draw skipped before the scan
    EXPECT_TRUE(f.cmap.live.empty());

    f.Draw(Req(h, "p"));   // next frame retries and succeeds
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().colourMapUploads);
    EXPECT_EQ(12u, f.renderer.GetRendererStats().outOfRangeCategoryCells);
}

TEST(CellFieldGapsRenderer, RampCreateFailureSkipsDrawAndRecovers)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    f.cmap.failNext = 1;
    f.Draw(Req(h, "r"));
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldGapsRenderer, DummyElevationCreateFailureSkipsDrawAndRecovers)
{
    RFixture f;
    f.RegRamp("r");
    Handle h = f.Make(4, 3);
    f.renderer.DrainRegistry();
    f.tex.failNext = 1;   // the layer texture already exists; the next Create is the dummy
    f.Draw(Req(h, "r"));
    EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(1u, f.renderer.GetRendererStats().valueTexturesResident);
    f.Draw(Req(h, "r"));
    EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
    EXPECT_EQ(2u, f.renderer.GetRendererStats().valueTexturesResident);
}

// ---- GetRendererStats, field by field ------------------------------------------------------

namespace
{
    struct StatsExpect
    {
        uint64_t unknownColourMaps, outOfRangeCategoryCells, colourMapUploads;
        uint32_t colourMapsResident;
        uint64_t drawCalls, backdropDraws, unbuiltLayerRequests;
        uint32_t valueTexturesResident;
        bool     geometryNonZero;
        uint32_t geometryEntries;
    };

    void ExpectStats(const StatsExpect& e, const CellFieldRendererStats& s, const char* step)
    {
        EXPECT_EQ(e.unknownColourMaps, s.unknownColourMaps) << step;
        EXPECT_EQ(e.outOfRangeCategoryCells, s.outOfRangeCategoryCells) << step;
        EXPECT_EQ(e.colourMapUploads, s.colourMapUploads) << step;
        EXPECT_EQ(e.colourMapsResident, s.colourMapsResident) << step;
        EXPECT_EQ(e.drawCalls, s.drawCalls) << step;
        EXPECT_EQ(e.backdropDraws, s.backdropDraws) << step;
        EXPECT_EQ(e.unbuiltLayerRequests, s.unbuiltLayerRequests) << step;
        EXPECT_EQ(e.valueTexturesResident, s.valueTexturesResident) << step;
        EXPECT_EQ(e.geometryNonZero, s.geometryBytes > 0) << step;
        EXPECT_EQ(e.geometryEntries, s.geometryEntries) << step;
    }
}

TEST(CellFieldGapsRenderer, RendererStatsEveryFieldThroughAScriptedScenario)
{
    RFixture f;
    ExpectStats({ 0, 0, 0, 0, 0, 0, 0, 0, false, 0 }, f.renderer.GetRendererStats(), "fresh");

    Handle cat = f.Make(4, 3, CellFieldValueKind::Category);
    Handle flt = f.Make(4, 3, CellFieldValueKind::Float);
    f.renderer.DrainRegistry();
    ExpectStats({ 0, 0, 0, 0, 0, 0, 0, 2, true, 1 }, f.renderer.GetRendererStats(), "two layers share geometry");
    const uint64_t geometryBytes = f.renderer.GetRendererStats().geometryBytes;
    EXPECT_EQ(0u, geometryBytes % 12u);
    EXPECT_GE(geometryBytes, 648u * 12u);

    f.Draw(Req(cat, "missing"));
    ExpectStats({ 1, 0, 0, 0, 0, 0, 0, 2, true, 1 }, f.renderer.GetRendererStats(), "unknown palette");

    f.RegPal("p", 3);
    const std::vector<float> v = { 0, 1, 2, 3, 4, 0, 1, 2, 9, 9, -1, 1 };   // 5 outside
    ASSERT_TRUE(f.registry.Update(cat, v.data(), 12).ok);
    f.Draw(Req(cat, "p"));
    ExpectStats({ 1, 5, 1, 1, 1, 0, 0, 3, true, 1 }, f.renderer.GetRendererStats(), "first palette draw (+dummy elevation)");

    Request inset = Req(cat, "p");
    inset.inset = 0.1f;
    inset.backdropColour = 0x000000FFu;
    f.Draw(inset);
    ExpectStats({ 1, 5, 1, 1, 3, 1, 0, 3, true, 1 }, f.renderer.GetRendererStats(), "backdrop + layer");

    f.registry.Destroy(flt);
    f.renderer.DrainRegistry();
    ExpectStats({ 1, 5, 1, 1, 3, 1, 0, 2, true, 1 }, f.renderer.GetRendererStats(), "float layer destroyed");

    Handle bad = f.MakeBad(2, 2);
    f.Draw(Req(bad, "p"));
    ExpectStats({ 1, 5, 1, 1, 3, 1, 1, 2, true, 1 }, f.renderer.GetRendererStats(), "unbuilt layer drawn");

    f.renderer.Shutdown();
    ExpectStats({ 1, 5, 1, 0, 3, 1, 1, 0, false, 0 }, f.renderer.GetRendererStats(), "after Shutdown (cumulative counters kept)");
}
