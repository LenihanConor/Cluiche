////////////////////////////////////////////////////////////////////////////////
// TestCellFieldRegistry.cpp
// Tests for CellFieldRegistry layer lifetime and command queue - DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <algorithm>
#include <string.h>
#include <vector>

using namespace Dia::Graphics;
using Dia::Core::CellFieldLayerHandle;

namespace
{
	struct GridStorage
	{
		std::vector<float>   centres;
		std::vector<int32_t> neighbours;
		CellGridDesc         desc;

		GridStorage(uint16_t w, uint16_t h)
		{
			const uint32_t n = (uint32_t)w * h;
			centres.assign(2 * n, 1.5f);
			neighbours.assign(6 * n, -1);
			desc.width = w;
			desc.height = h;
			desc.cellSize = 2.0f;
			desc.centresXY = centres.data();
			desc.neighbours = neighbours.data();
		}
	};

	struct RecordingSink : ICellFieldCommandSink
	{
		struct Rec
		{
			char type = '?';	// C, U, D
			CellFieldLayerHandle h;
			CellFieldValueKind kind = CellFieldValueKind::Category;
			uint16_t w = 0, ht = 0;
			std::vector<float> centres;
			std::vector<float> values;
		};
		std::vector<Rec> recs;

		void OnCreate(CellFieldLayerHandle h, const CellGridDesc& d, CellFieldValueKind k) override
		{
			Rec r; r.type = 'C'; r.h = h; r.kind = k; r.w = d.width; r.ht = d.height;
			r.centres.assign(d.centresXY, d.centresXY + 2 * d.width * d.height);
			recs.push_back(r);
		}
		void OnUpdate(CellFieldLayerHandle h, const float* v, uint32_t c) override
		{
			Rec r; r.type = 'U'; r.h = h; r.values.assign(v, v + c); recs.push_back(r);
		}
		void OnDestroy(CellFieldLayerHandle h) override { Rec r; r.type = 'D'; r.h = h; recs.push_back(r); }
	};
}

TEST(CellFieldRegistry, CreateLayerReturnsValidHandle)
{
	CellFieldRegistry reg;
	GridStorage g(4, 3);
	CellFieldLayerHandle h;
	CellFieldResult r = reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	EXPECT_TRUE(r.ok);
	EXPECT_TRUE(h.IsValid());
	EXPECT_TRUE(reg.ResolveHandle(h));
	EXPECT_EQ(reg.GetStats().creates, 1u);
}

TEST(CellFieldRegistry, UpdateValidHandleQueuesCommand)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	float v[4] = { 1, 2, 3, 4 };
	EXPECT_TRUE(reg.Update(h, v, 4).ok);
	CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.updates, 1u);
	EXPECT_EQ(s.commandsQueued, 2u);
}

TEST(CellFieldRegistry, UpdateWrongCountRejected)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	float v[3] = { 1, 2, 3 };
	CellFieldResult r = reg.Update(h, v, 3);
	EXPECT_FALSE(r.ok);
	ASSERT_NE(r.error, nullptr);
	EXPECT_STREQ(r.error, "Value count mismatch");
	EXPECT_EQ(reg.GetStats().updates, 0u);
}

TEST(CellFieldRegistry, UpdateAfterDestroyIsStale)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	reg.Destroy(h);
	float v[4] = {};
	CellFieldResult r = reg.Update(h, v, 4);
	EXPECT_FALSE(r.ok);
	EXPECT_EQ(reg.GetStats().staleHandles, 1u);
	EXPECT_FALSE(reg.ResolveHandle(h));
	EXPECT_EQ(reg.GetStats().staleHandles, 2u);
}

TEST(CellFieldRegistry, RecycledSlotGetsNewGeneration)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	RecordingSink sink;
	CellFieldLayerHandle a, b;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, a);
	reg.Destroy(a);
	reg.Drain(sink);
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, b).ok);
	EXPECT_EQ(a.index, b.index);
	EXPECT_NE(a.generation, b.generation);
	EXPECT_FALSE(reg.ResolveHandle(a));
	EXPECT_TRUE(reg.ResolveHandle(b));
	float v[4] = {};
	EXPECT_FALSE(reg.Update(a, v, 4).ok);
	EXPECT_TRUE(reg.Update(b, v, 4).ok);
}

TEST(CellFieldRegistry, StaleDestroyCountedAndHarmless)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	reg.Destroy(h);
	reg.Destroy(h);
	CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.staleHandles, 1u);
	EXPECT_EQ(s.destroys, 1u);
	reg.Destroy(CellFieldLayerHandle());	// invalid handle
	EXPECT_EQ(reg.GetStats().staleHandles, 2u);
}

TEST(CellFieldRegistry, DestroyIsDeferredUntilDrain)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	RecordingSink sink;
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	reg.Destroy(h);
	EXPECT_EQ(reg.GetStats().layersAllocated, 1u);	// still in layer table
	EXPECT_EQ(reg.GetStats().layersLive, 0u);
	reg.Drain(sink);
	EXPECT_EQ(reg.GetStats().layersAllocated, 0u);
}

TEST(CellFieldRegistry, DrainDeliversCopiesInSubmitOrder)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	RecordingSink sink;
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Category, h);
	std::vector<float> vals = { 5, 6, 7, 8 };
	reg.Update(h, vals.data(), 4);
	reg.Destroy(h);

	// Caller mutates/frees its buffers after the calls return
	std::fill(g.centres.begin(), g.centres.end(), -9.0f);
	std::fill(vals.begin(), vals.end(), -1.0f);
	g.centres.clear(); g.centres.shrink_to_fit();
	vals.clear(); vals.shrink_to_fit();

	reg.Drain(sink);
	ASSERT_EQ(sink.recs.size(), 3u);
	EXPECT_EQ(sink.recs[0].type, 'C');
	EXPECT_EQ(sink.recs[0].kind, CellFieldValueKind::Category);
	EXPECT_EQ(sink.recs[0].w, 2);
	EXPECT_EQ(sink.recs[0].ht, 2);
	ASSERT_EQ(sink.recs[0].centres.size(), 8u);
	EXPECT_FLOAT_EQ(sink.recs[0].centres[3], 1.5f);
	EXPECT_EQ(sink.recs[1].type, 'U');
	ASSERT_EQ(sink.recs[1].values.size(), 4u);
	EXPECT_FLOAT_EQ(sink.recs[1].values[0], 5.0f);
	EXPECT_FLOAT_EQ(sink.recs[1].values[3], 8.0f);
	EXPECT_EQ(sink.recs[2].type, 'D');
	EXPECT_EQ(reg.GetStats().commandsQueued, 0u);

	sink.recs.clear();
	reg.Drain(sink);
	EXPECT_TRUE(sink.recs.empty());
}

TEST(CellFieldRegistry, ThirtyThirdLayerRejectedUntilDestroyDrained)
{
	CellFieldRegistry reg;
	GridStorage g(1, 1);
	RecordingSink sink;
	CellFieldLayerHandle hs[32];
	for (int i = 0; i < 32; ++i) { ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, hs[i]).ok); }
	CellFieldLayerHandle extra;
	CellFieldResult r = reg.CreateLayer(g.desc, CellFieldValueKind::Float, extra);
	EXPECT_FALSE(r.ok);
	EXPECT_STREQ(r.error, "Max 32 layers");
	EXPECT_FALSE(extra.IsValid());

	// Destroy alone does not free the slot: it is released only when drained.
	reg.Destroy(hs[5]);
	EXPECT_FALSE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, extra).ok);
	reg.Drain(sink);
	EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, extra).ok);
	EXPECT_EQ(extra.index, hs[5].index);
}

TEST(CellFieldRegistry, OversizedGridRejected)
{
	CellFieldRegistry reg;
	GridStorage g(129, 1);
	CellFieldLayerHandle h;
	CellFieldResult r = reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	EXPECT_FALSE(r.ok);
	ASSERT_NE(r.error, nullptr);
	EXPECT_EQ(strncmp(r.error, "Grid exceeds 128", 16), 0);
	EXPECT_FALSE(h.IsValid());
	GridStorage tall(1, 129);
	EXPECT_FALSE(reg.CreateLayer(tall.desc, CellFieldValueKind::Float, h).ok);
	GridStorage big(128, 128);
	EXPECT_TRUE(reg.CreateLayer(big.desc, CellFieldValueKind::Float, h).ok);
}

TEST(CellFieldRegistry, StatsReflectDrainedWork)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	RecordingSink sink;
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	float v[4] = {};
	reg.Update(h, v, 4);
	reg.Update(h, v, 4);
	reg.Drain(sink);
	CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.geometryBuilds, 1u);
	EXPECT_EQ(s.uploads, 2u);
	EXPECT_EQ(s.uploadedBytes, 32u);
	EXPECT_EQ(s.layersLive, 1u);
	EXPECT_EQ(s.droppedCommands, 0u);
}
