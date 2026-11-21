// TestCellFieldGeometryCache.cpp - cache bookkeeping tested with a fake (GPU-free) factory.
#include <gtest/gtest.h>

#include <DiaBgfx/Resources/CellFieldGeometryCache.h>
#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

#include <set>

using namespace Dia::Bgfx;

namespace
{
    class FakeFactory : public ICellFieldGpuGeometryFactory
    {
    public:
        bool Create(const Dia::Graphics::CellGridGeometry&, CellFieldGpuHandle& out) override
        {
            ++createCalls;
            out = nextHandle++;
            live.insert(out);
            return true;
        }
        void Destroy(CellFieldGpuHandle h) override { ++destroyCalls; live.erase(h); }

        int createCalls = 0;
        int destroyCalls = 0;
        uint32_t nextHandle = 1;
        std::set<CellFieldGpuHandle> live;
    };

    struct Fixture
    {
        FakeFactory factory;
        Dia::Graphics::HexFlatTopOddQBuilder builder;
        CellFieldGeometryCache cache{ factory };
        HexDescStorage storage;

        Fixture() { cache.AddBuilder(&builder); }

        Dia::Graphics::CellGridDesc Desc(uint32_t w, uint32_t h)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            return HexTopologyToCellGridDesc(topo, 1.0f, storage);
        }
    };
}

TEST(CellFieldGeometryCache, TwoLayersSameKeyBuildOnce)
{
    Fixture f;
    const auto desc = f.Desc(4, 4);
    const auto* a = f.cache.Acquire(desc);
    const auto* b = f.cache.Acquire(desc);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_EQ(f.factory.createCalls, 1);
    EXPECT_EQ(f.cache.Stats().geometryBuilds, 1u);
    EXPECT_EQ(a->refCount, 2u);
    EXPECT_EQ(a->vertexCount, 4u * 4u * 18u);
}

TEST(CellFieldGeometryCache, ReleaseOneStillCached)
{
    Fixture f;
    const auto desc = f.Desc(4, 4);
    f.cache.Acquire(desc);
    f.cache.Acquire(desc);
    f.cache.Release(CellFieldGeometryCache::MakeKey(desc));
    EXPECT_EQ(f.factory.destroyCalls, 0);
    EXPECT_EQ(f.cache.Stats().liveEntries, 1u);
}

TEST(CellFieldGeometryCache, ReleaseBothFrees)
{
    Fixture f;
    const auto desc = f.Desc(4, 4);
    f.cache.Acquire(desc);
    f.cache.Acquire(desc);
    f.cache.Release(CellFieldGeometryCache::MakeKey(desc));
    f.cache.Release(CellFieldGeometryCache::MakeKey(desc));
    EXPECT_EQ(f.factory.destroyCalls, 1);
    EXPECT_EQ(f.cache.Stats().liveEntries, 0u);
    EXPECT_EQ(f.cache.Stats().gpuBytes, 0u);
    EXPECT_TRUE(f.factory.live.empty());
}

TEST(CellFieldGeometryCache, DifferentSizesBuildSeparately)
{
    Fixture f;
    const auto* a = f.cache.Acquire(f.Desc(4, 4));
    const auto* b = f.cache.Acquire(f.Desc(5, 4));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
    EXPECT_EQ(f.factory.createCalls, 2);
    EXPECT_EQ(f.cache.Stats().liveEntries, 2u);
}

TEST(CellFieldGeometryCache, SameSizeAfterFullReleaseRebuilds)
{
    Fixture f;
    const auto desc = f.Desc(4, 4);
    f.cache.Acquire(desc);
    f.cache.Release(CellFieldGeometryCache::MakeKey(desc));
    ASSERT_NE(f.cache.Acquire(desc), nullptr);
    EXPECT_EQ(f.factory.createCalls, 2);
    EXPECT_EQ(f.cache.Stats().geometryBuilds, 2u);
}

TEST(CellFieldGeometryCache, ShutdownDestroysAllLive)
{
    Fixture f;
    f.cache.Acquire(f.Desc(4, 4));
    f.cache.Acquire(f.Desc(4, 4));
    f.cache.Acquire(f.Desc(6, 6));
    f.cache.Shutdown();
    EXPECT_EQ(f.factory.destroyCalls, 2);
    EXPECT_TRUE(f.factory.live.empty());
    EXPECT_EQ(f.cache.Stats().liveEntries, 0u);
}

TEST(CellFieldGeometryCache, UnknownTopologyReturnsNullNoGpuCall)
{
    Fixture f;
    auto desc = f.Desc(4, 4);
    desc.topology = Dia::Core::StringCRC("square.unknown");
    EXPECT_EQ(f.cache.Acquire(desc), nullptr);
    EXPECT_EQ(f.factory.createCalls, 0);
    EXPECT_EQ(f.cache.Stats().liveEntries, 0u);
    EXPECT_EQ(f.cache.Stats().geometryBuilds, 0u);
}

TEST(CellFieldGeometryCache, StatsReportBytes)
{
    Fixture f;
    const auto* e = f.cache.Acquire(f.Desc(4, 4));
    ASSERT_NE(e, nullptr);
    const uint64_t expected = uint64_t(e->vertexCount) * 36u + uint64_t(e->indexCount) * 4u;
    EXPECT_EQ(f.cache.Stats().gpuBytes, expected);
    EXPECT_EQ(f.cache.Stats().liveEntries, 1u);
}
