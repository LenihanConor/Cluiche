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
	EXPECT_EQ(s.uploads, 1u);			// two Updates to one layer coalesce into one upload
	EXPECT_EQ(s.uploadedBytes, 16u);
	EXPECT_EQ(s.coalescedCommands, 1u);
	EXPECT_EQ(s.layersLive, 1u);
	EXPECT_EQ(s.droppedCommands, 0u);
}

////////////////////////////////////////////////////////////////////////////////
// Palette and ramp tables
////////////////////////////////////////////////////////////////////////////////
using Dia::Core::StringCRC;

TEST(CellFieldColourMaps, PaletteRegisterAndLookup)
{
	CellFieldRegistry reg;
	const uint32_t cols[3] = { 0xFF0000FFu, 0x00FF00FFu, 0x0000FFFFu };
	CellFieldResult r = reg.RegisterPalette(StringCRC("cat.biome"), cols, 3, 0x808080FFu);
	ASSERT_TRUE(r.ok);
	EXPECT_EQ(r.error, nullptr);

	CellFieldPaletteData d;
	ASSERT_TRUE(reg.GetPalette(StringCRC("cat.biome").Value(), d));
	EXPECT_EQ(d.count, 3u);
	EXPECT_EQ(d.colours[0], cols[0]);
	EXPECT_EQ(d.colours[2], cols[2]);
	EXPECT_EQ(d.fallback, 0x808080FFu);
	EXPECT_EQ(d.revision, 1u);
}

TEST(CellFieldColourMaps, RampRegisterDottedNameAndLookup)
{
	CellFieldRegistry reg;
	const CellFieldRampStop stops[3] = { { 0.0f, 0x000000FFu }, { 0.5f, 0x00FF00FFu }, { 1.0f, 0xFFFFFFFFu } };
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("sequential.green"), stops, 3).ok);

	CellFieldRampData d;
	ASSERT_TRUE(reg.GetRamp(StringCRC("sequential.green").Value(), d));
	EXPECT_EQ(d.count, 3u);
	for (uint32_t i = 0; i < 3; ++i)
	{
		EXPECT_FLOAT_EQ(d.stops[i].position, stops[i].position);
		EXPECT_EQ(d.stops[i].rgba8, stops[i].rgba8);
	}
	EXPECT_EQ(d.revision, 1u);
}

TEST(CellFieldColourMaps, PaletteInvalidInputsRejected)
{
	CellFieldRegistry reg;
	const uint32_t cols[65] = {};
	EXPECT_FALSE(reg.RegisterPalette(StringCRC("p"), cols, 0, 0).ok);
	EXPECT_FALSE(reg.RegisterPalette(StringCRC("p"), cols, 65, 0).ok);
	EXPECT_FALSE(reg.RegisterPalette(StringCRC("p"), nullptr, 3, 0).ok);
	CellFieldResult r = reg.RegisterPalette(StringCRC("p"), cols, 64, 0);
	EXPECT_TRUE(r.ok);
	EXPECT_EQ(reg.GetStats().rejectedRequests, 3u);
	CellFieldResult bad = reg.RegisterPalette(StringCRC("p"), nullptr, 3, 0);
	EXPECT_NE(bad.error, nullptr);
}

TEST(CellFieldColourMaps, RampInvalidInputsRejected)
{
	CellFieldRegistry reg;
	CellFieldRampStop s[17];
	for (int i = 0; i < 17; ++i) { s[i].position = i / 16.0f; s[i].rgba8 = 0xFFu; }
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("r"), s, 0).ok);
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("r"), s, 17).ok);
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("r"), nullptr, 2).ok);
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("r"), s, 16).ok);

	const CellFieldRampStop decreasing[2] = { { 0.8f, 0 }, { 0.2f, 0 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("d"), decreasing, 2).ok);
	const CellFieldRampStop below[2] = { { -0.1f, 0 }, { 0.2f, 0 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("b"), below, 2).ok);
	const CellFieldRampStop above[2] = { { 0.1f, 0 }, { 1.2f, 0 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("a"), above, 2).ok);
	const CellFieldRampStop equalPos[2] = { { 0.5f, 0 }, { 0.5f, 0xFFu } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("e"), equalPos, 2).ok);
	EXPECT_EQ(reg.GetStats().rejectedRequests, 6u);
	CellFieldRampData d;
	EXPECT_FALSE(reg.GetRamp(StringCRC("d").Value(), d));
}

TEST(CellFieldColourMaps, TableFullRejects129thDistinctId)
{
	CellFieldRegistry reg;
	const uint32_t col = 0xFFu;
	const CellFieldRampStop st = { 0.0f, 0xFFu };
	char name[32];
	for (uint32_t i = 0; i < 128; ++i)
	{
		snprintf(name, sizeof(name), "pal%u", i);
		ASSERT_TRUE(reg.RegisterPalette(StringCRC(name), &col, 1, 0).ok) << i;
		snprintf(name, sizeof(name), "ramp%u", i);
		ASSERT_TRUE(reg.RegisterRamp(StringCRC(name), &st, 1).ok) << i;
	}
	CellFieldResult p = reg.RegisterPalette(StringCRC("palExtra"), &col, 1, 0);
	CellFieldResult q = reg.RegisterRamp(StringCRC("rampExtra"), &st, 1);
	EXPECT_FALSE(p.ok);
	EXPECT_NE(p.error, nullptr);
	EXPECT_FALSE(q.ok);
	EXPECT_NE(q.error, nullptr);
	// Replacing an existing entry still works when full
	EXPECT_TRUE(reg.RegisterPalette(StringCRC("pal5"), &col, 1, 0).ok);
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("ramp5"), &st, 1).ok);
}

TEST(CellFieldColourMaps, UnknownIdLookupNotFoundAndUncounted)
{
	CellFieldRegistry reg;
	CellFieldPaletteData p;
	CellFieldRampData r;
	EXPECT_FALSE(reg.GetPalette(0x1234u, p));
	EXPECT_FALSE(reg.GetRamp(0x1234u, r));
	const CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.rejectedRequests, 0u);
	EXPECT_EQ(s.staleHandles, 0u);
}

TEST(CellFieldColourMaps, PaletteAndRampNamespacesAreSeparate)
{
	CellFieldRegistry reg;
	const uint32_t col = 0x11223344u;
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("x"), &col, 1, 0).ok);
	CellFieldRampData r;
	EXPECT_FALSE(reg.GetRamp(StringCRC("x").Value(), r));
}

TEST(CellFieldColourMaps, ReRegisterReplacesAndBumpsRevision)
{
	CellFieldRegistry reg;
	const uint32_t a[1] = { 0xAAu };
	const uint32_t b[2] = { 0xBBu, 0xCCu };
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), a, 1, 1).ok);
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), b, 2, 2).ok);
	CellFieldPaletteData d;
	ASSERT_TRUE(reg.GetPalette(StringCRC("p").Value(), d));
	EXPECT_EQ(d.count, 2u);
	EXPECT_EQ(d.colours[0], 0xBBu);
	EXPECT_EQ(d.fallback, 2u);
	EXPECT_EQ(d.revision, 2u);

	const CellFieldRampStop r1[1] = { { 0.0f, 0x11u } };
	const CellFieldRampStop r2[2] = { { 0.0f, 0x22u }, { 1.0f, 0x33u } };
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("r"), r1, 1).ok);
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("r"), r2, 2).ok);
	CellFieldRampData rd;
	ASSERT_TRUE(reg.GetRamp(StringCRC("r").Value(), rd));
	EXPECT_EQ(rd.count, 2u);
	EXPECT_EQ(rd.stops[1].rgba8, 0x33u);
	EXPECT_EQ(rd.revision, 2u);

	// A rejected re-register leaves the entry and revision untouched
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("r"), nullptr, 2).ok);
	ASSERT_TRUE(reg.GetRamp(StringCRC("r").Value(), rd));
	EXPECT_EQ(rd.revision, 2u);
	EXPECT_EQ(rd.count, 2u);
}

TEST(CellFieldColourMaps, BakeRampEndpointsExactAndMidpoint)
{
	const CellFieldRampStop s[2] = { { 0.0f, 0x000000FFu }, { 1.0f, 0xFF8040FFu } };
	uint32_t out[256];
	BakeRamp(s, 2, out);
	EXPECT_EQ(out[0], 0x000000FFu);
	EXPECT_EQ(out[255], 0xFF8040FFu);
	// texel centre 127.5/255 = 0.5 -> halfway; allow +-1 per channel
	const uint32_t mid = out[128];
	const int rr = (int)((mid >> 24) & 0xFF), gg = (int)((mid >> 16) & 0xFF), bb = (int)((mid >> 8) & 0xFF);
	EXPECT_NEAR(rr, 128, 2);
	EXPECT_NEAR(gg, 64, 2);
	EXPECT_NEAR(bb, 32, 2);
	EXPECT_EQ(mid & 0xFF, 0xFFu);
}

TEST(CellFieldColourMaps, BakeRampClampsEndsAndSingleStopConstant)
{
	const CellFieldRampStop s[2] = { { 0.25f, 0x102030FFu }, { 0.75f, 0x405060FFu } };
	uint32_t out[256];
	BakeRamp(s, 2, out);
	EXPECT_EQ(out[0], 0x102030FFu);
	EXPECT_EQ(out[63], 0x102030FFu);	// 63/255 = 0.247 < 0.25
	EXPECT_EQ(out[255], 0x405060FFu);
	EXPECT_EQ(out[192], 0x405060FFu);	// 192/255 = 0.753 > 0.75

	const CellFieldRampStop one[1] = { { 0.5f, 0xDEADBEEFu } };
	BakeRamp(one, 1, out);
	for (int i = 0; i < 256; ++i) { ASSERT_EQ(out[i], 0xDEADBEEFu) << i; }
}

TEST(CellFieldColourMaps, BakeRampInterpolatesAlpha)
{
	const CellFieldRampStop s[2] = { { 0.0f, 0xFFFFFF00u }, { 1.0f, 0xFFFFFFFFu } };
	uint32_t out[256];
	BakeRamp(s, 2, out);
	EXPECT_EQ(out[0] & 0xFF, 0x00u);
	EXPECT_EQ(out[255] & 0xFF, 0xFFu);
	EXPECT_NEAR((int)(out[128] & 0xFF), 128, 2);
}

TEST(CellFieldColourMaps, BakePalettePadsWithFallback)
{
	const uint32_t cols[3] = { 1u, 2u, 3u };
	uint32_t out[64];
	BakePalette(cols, 3, 0xABCDEF01u, out);
	EXPECT_EQ(out[0], 1u);
	EXPECT_EQ(out[2], 3u);
	for (int i = 3; i < 64; ++i) { ASSERT_EQ(out[i], 0xABCDEF01u) << i; }
}

TEST(CellFieldColourMaps, BakePaletteFullNoPadding)
{
	uint32_t cols[64];
	for (uint32_t i = 0; i < 64; ++i) { cols[i] = i + 10; }
	uint32_t out[64];
	BakePalette(cols, 64, 0xFFu, out);
	for (uint32_t i = 0; i < 64; ++i) { ASSERT_EQ(out[i], i + 10) << i; }
}
