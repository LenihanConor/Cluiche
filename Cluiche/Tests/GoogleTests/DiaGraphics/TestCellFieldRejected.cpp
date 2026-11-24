////////////////////////////////////////////////////////////////////////////////
// TestCellFieldRejected.cpp
// AC13 (rejected): every rejected CellField request returns an error result (or is skipped and
// counted), never asserts, queues nothing, changes no state and leaves the registry usable.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>

#include <limits>
#include <string.h>
#include <vector>

using namespace Dia::Graphics;
using Dia::Core::CellFieldLayerHandle;

namespace
{
	struct Grid
	{
		std::vector<float>   centres;
		std::vector<int32_t> neighbours;
		CellGridDesc         desc;

		Grid(uint32_t w, uint32_t h)
		{
			const uint32_t n = w * h;
			centres.assign(2 * n + 2, 1.0f);
			neighbours.assign(6 * n + 6, -1);
			desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
			desc.width = (uint16_t)w;
			desc.height = (uint16_t)h;
			desc.cellSize = 1.0f;
			desc.centresXY = centres.data();
			desc.neighbours = neighbours.data();
		}
	};

	struct NullSink : ICellFieldCommandSink
	{
		void OnCreate(CellFieldLayerHandle, const CellGridDesc&, CellFieldValueKind) override {}
		void OnUpdate(CellFieldLayerHandle, const float*, uint32_t) override {}
		void OnDestroy(CellFieldLayerHandle) override {}
	};

	// Asserts the rejection contract for a CreateLayer call.
	void ExpectCreateRejected(CellFieldRegistry& reg, const CellGridDesc& desc, const char* error)
	{
		const CellFieldStats before = reg.GetStats();
		CellFieldLayerHandle h;
		const CellFieldResult r = reg.CreateLayer(desc, CellFieldValueKind::Float, h);
		const CellFieldStats after = reg.GetStats();
		EXPECT_FALSE(r.ok);
		ASSERT_NE(r.error, nullptr);
		EXPECT_STREQ(error, r.error);
		EXPECT_FALSE(h.IsValid());
		EXPECT_EQ(before.rejectedRequests + 1, after.rejectedRequests);
		EXPECT_EQ(before.commandsQueued, after.commandsQueued);
		EXPECT_EQ(before.queuedBytes, after.queuedBytes);
		EXPECT_EQ(before.layersLive, after.layersLive);
		EXPECT_EQ(before.layersAllocated, after.layersAllocated);
		EXPECT_EQ(before.creates, after.creates);
	}

	void ExpectRegistryUsable(CellFieldRegistry& reg)
	{
		Grid g(2, 2);
		CellFieldLayerHandle h;
		EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
		EXPECT_TRUE(h.IsValid());
		const float v[4] = { 1, 2, 3, 4 };
		EXPECT_TRUE(reg.Update(h, v, 4).ok);
		reg.Destroy(h);
		NullSink sink;
		reg.Drain(sink);
	}

	// GPU-free fakes for the renderer-level cases.
	class FakeGeo : public Dia::Bgfx::ICellFieldGpuGeometryFactory
	{
	public:
		bool Create(const CellGridGeometry&, Dia::Bgfx::CellFieldGpuHandle& out) override { out = nextHandle++; return true; }
		void Destroy(Dia::Bgfx::CellFieldGpuHandle) override {}
		uint32_t nextHandle = 1;
	};
	class FakeTex : public Dia::Bgfx::ICellFieldGpuTextureFactory
	{
	public:
		bool Create(uint16_t, uint16_t, Dia::Bgfx::CellFieldGpuTextureHandle& out) override { out = nextHandle++; return true; }
		void Update(Dia::Bgfx::CellFieldGpuTextureHandle, const float*, uint32_t) override { ++updateCalls; }
		void Destroy(Dia::Bgfx::CellFieldGpuTextureHandle) override {}
		int updateCalls = 0;
		uint32_t nextHandle = 1;
	};
	class FakeColourMap : public Dia::Bgfx::ICellFieldGpuColourMapFactory
	{
	public:
		bool CreateRgba8(uint32_t, const uint32_t*, uint32_t, bool, Dia::Bgfx::CellFieldGpuColourMapHandle& out) override { out = nextHandle++; return true; }
		void UpdateRgba8(Dia::Bgfx::CellFieldGpuColourMapHandle, const uint32_t*, uint32_t) override {}
		void Destroy(Dia::Bgfx::CellFieldGpuColourMapHandle) override {}
		uint32_t nextHandle = 1;
	};

	struct RenderFixture
	{
		FakeGeo geo;
		FakeTex tex;
		FakeColourMap cmap;
		CellFieldRegistry registry;
		Dia::Bgfx::CellFieldRenderer renderer{ 0, nullptr, &registry, geo, tex, cmap };
	};

	void DrawOne(RenderFixture& f, CellFieldLayerHandle h, const char* colourMap)
	{
		Dia::Core::CellFieldDrawRequest req;
		req.layer = h;
		req.colourMap = Dia::Core::StringCRC(colourMap).Value();
		CellFieldFrameData frame;
		frame.RequestDrawCellFieldData(req);
		f.renderer.Draw(frame);
	}
}

TEST(CellFieldRejected, GridTooLargeRejected)
{
	CellFieldRegistry reg;
	Grid g(129, 1);
	ExpectCreateRejected(reg, g.desc, CellFieldRegistry::kErrGridTooLarge);
	EXPECT_STREQ("Grid exceeds 128" "\xC3\x97" "128", CellFieldRegistry::kErrGridTooLarge);
	Grid g2(1, 129);
	ExpectCreateRejected(reg, g2.desc, CellFieldRegistry::kErrGridTooLarge);
	Grid g3(200, 200);
	ExpectCreateRejected(reg, g3.desc, CellFieldRegistry::kErrGridTooLarge);
	ExpectRegistryUsable(reg);
}

TEST(CellFieldRejected, Exactly128x128Accepted)
{
	CellFieldRegistry reg;
	Grid g(128, 128);
	CellFieldLayerHandle h;
	EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	EXPECT_EQ(0u, reg.GetStats().rejectedRequests);
}

TEST(CellFieldRejected, InvalidGridDescriptionsRejected)
{
	CellFieldRegistry reg;
	Grid g(4, 4);
	CellGridDesc d = g.desc;

	d = g.desc; d.width = 0;
	ExpectCreateRejected(reg, d, CellFieldRegistry::kErrInvalidGrid);
	d = g.desc; d.height = 0;
	ExpectCreateRejected(reg, d, CellFieldRegistry::kErrInvalidGrid);
	d = g.desc; d.centresXY = nullptr;
	ExpectCreateRejected(reg, d, CellFieldRegistry::kErrInvalidGrid);
	d = g.desc; d.neighbours = nullptr;
	ExpectCreateRejected(reg, d, CellFieldRegistry::kErrInvalidGrid);
	ExpectRegistryUsable(reg);
}

TEST(CellFieldRejected, NonPositiveOrNonFiniteCellSizeRejected)
{
	CellFieldRegistry reg;
	Grid g(4, 4);
	const float bad[] = { 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
	for (float s : bad)
	{
		CellGridDesc d = g.desc;
		d.cellSize = s;
		ExpectCreateRejected(reg, d, CellFieldRegistry::kErrInvalidGrid);
	}
	ExpectRegistryUsable(reg);
}

TEST(CellFieldRejected, UnsupportedTopologyRejected)
{
	CellFieldRegistry reg;
	Grid g(4, 4);
	CellGridDesc d = g.desc;
	d.topology = Dia::Core::StringCRC("square.unknown");
	ExpectCreateRejected(reg, d, CellFieldRegistry::kErrUnsupportedTopology);
	EXPECT_STREQ("Unsupported topology", CellFieldRegistry::kErrUnsupportedTopology);

	reg.RegisterSupportedTopology(Dia::Core::StringCRC("square.unknown"));
	CellFieldLayerHandle h;
	EXPECT_TRUE(reg.CreateLayer(d, CellFieldValueKind::Float, h).ok);
	ExpectRegistryUsable(reg);
}

TEST(CellFieldRejected, ThirtyThirdLayerRejectedThenSucceedsAfterDestroyAndDrain)
{
	CellFieldRegistry reg;
	Grid g(2, 2);
	std::vector<CellFieldLayerHandle> handles;
	for (uint32_t i = 0; i < CellFieldRegistry::kMaxLayers; ++i)
	{
		CellFieldLayerHandle h;
		ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok) << i;
		handles.push_back(h);
	}
	ExpectCreateRejected(reg, g.desc, CellFieldRegistry::kErrMaxLayers);
	EXPECT_STREQ("Max 32 layers", CellFieldRegistry::kErrMaxLayers);
	EXPECT_EQ(32u, reg.GetStats().layersLive);

	reg.Destroy(handles[5]);
	// Before the drain the slot is still held: the retry is rejected cleanly.
	ExpectCreateRejected(reg, g.desc, CellFieldRegistry::kErrMaxLayers);

	NullSink sink;
	reg.Drain(sink);
	CellFieldLayerHandle h;
	EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	EXPECT_EQ(32u, reg.GetStats().layersLive);
	ExpectCreateRejected(reg, g.desc, CellFieldRegistry::kErrMaxLayers);
}

TEST(CellFieldRejected, UpdateInvalidCountsRejected)
{
	CellFieldRegistry reg;
	Grid g(2, 2);
	CellFieldLayerHandle h;
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	std::vector<float> big(129u * 129u, 0.0f);

	struct Case { const float* values; uint32_t count; };
	const Case cases[] = {
		{ big.data(), 3 },                       // mismatch
		{ big.data(), 0 },                       // zero count
		{ nullptr, 4 },                          // null values
		{ big.data(), 128u * 128u + 1u },        // above the largest grid (no copy attempted)
	};
	for (const Case& c : cases)
	{
		const CellFieldStats before = reg.GetStats();
		const CellFieldResult r = reg.Update(h, c.values, c.count);
		const CellFieldStats after = reg.GetStats();
		EXPECT_FALSE(r.ok);
		ASSERT_NE(r.error, nullptr);
		EXPECT_STREQ(CellFieldRegistry::kErrCountMismatch, r.error);
		EXPECT_STREQ("Value count mismatch", r.error);
		EXPECT_EQ(before.rejectedRequests + 1, after.rejectedRequests);
		EXPECT_EQ(before.commandsQueued, after.commandsQueued);
		EXPECT_EQ(before.queuedBytes, after.queuedBytes);
		EXPECT_EQ(before.updates, after.updates);
		EXPECT_EQ(before.layersLive, after.layersLive);
	}
	const float ok[4] = { 1, 2, 3, 4 };
	EXPECT_TRUE(reg.Update(h, ok, 4).ok);
}

TEST(CellFieldRejected, UnknownColourMapSkippedAndCounted)
{
	RenderFixture f;
	Grid g(2, 2);
	CellFieldLayerHandle h;
	ASSERT_TRUE(f.registry.CreateLayer(g.desc, CellFieldValueKind::Category, h).ok);

	// Many draws of the same unknown id: every one skipped and counted (the Warning is logged
	// once per id inside the renderer; the counter proves each draw was skipped, not crashed).
	for (int i = 0; i < 50; ++i)
		DrawOne(f, h, "no.such.palette");
	EXPECT_EQ(50u, f.renderer.GetRendererStats().unknownColourMaps);
	EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);

	// A ramp id registered for a Category layer is still an unknown palette.
	const CellFieldRampStop stops[2] = { { 0.0f, 0x000000FFu }, { 1.0f, 0xFFFFFFFFu } };
	ASSERT_TRUE(f.registry.RegisterRamp(Dia::Core::StringCRC("a.ramp"), stops, 2).ok);
	DrawOne(f, h, "a.ramp");
	EXPECT_EQ(51u, f.renderer.GetRendererStats().unknownColourMaps);
	EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
}

TEST(CellFieldRejected, FailedGeometryBuildDropsLayerAndLaterRequestsAreSkippedAndCounted)
{
	RenderFixture f;
	Grid g(2, 2);
	g.neighbours[0] = 999;	// neighbour outside the grid: registry accepts, the builder rejects
	CellFieldLayerHandle bad;
	ASSERT_TRUE(f.registry.CreateLayer(g.desc, CellFieldValueKind::Float, bad).ok);
	f.renderer.DrainRegistry();	// Error logged on channel CFL, layer dropped

	const CellFieldRampStop stops[2] = { { 0.0f, 0x000000FFu }, { 1.0f, 0xFFFFFFFFu } };
	ASSERT_TRUE(f.registry.RegisterRamp(Dia::Core::StringCRC("r"), stops, 2).ok);

	const float values[4] = { 1, 2, 3, 4 };
	EXPECT_TRUE(f.registry.Update(bad, values, 4).ok);
	DrawOne(f, bad, "r");	// drains the Update (skipped + counted), then the draw (skipped + counted)

	EXPECT_EQ(2u, f.renderer.GetRendererStats().unbuiltLayerRequests);
	EXPECT_EQ(0u, f.renderer.GetRendererStats().drawCalls);
	EXPECT_EQ(0, f.tex.updateCalls);

	// Destroying the dropped layer is safe and the renderer keeps working for a good layer.
	f.registry.Destroy(bad);
	Grid good(2, 2);
	CellFieldLayerHandle h;
	ASSERT_TRUE(f.registry.CreateLayer(good.desc, CellFieldValueKind::Float, h).ok);
	DrawOne(f, h, "r");
	EXPECT_EQ(1u, f.renderer.GetRendererStats().drawCalls);
}
