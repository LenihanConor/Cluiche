////////////////////////////////////////////////////////////////////////////////
// TestCellFieldRegistryThreading.cpp
// Cross-thread hand-off tests for CellFieldRegistry (coalescing, byte cap, counters,
// producer/consumer races) and the renderer drain path - DiaGraphics / DiaBgfx (Task 16)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaPathfinding/HexTopology.h>
#include "HexTopologyToCellGridDesc.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <map>
#include <set>
#include <thread>
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
			desc.topology = Dia::Core::StringCRC("hex.flat.oddq");
			desc.centresXY = centres.data();
			desc.neighbours = neighbours.data();
		}
	};

	uint32_t Key(CellFieldLayerHandle h) { return ((uint32_t)h.index << 16) | h.generation; }

	// Light recording sink: order + last values per handle, and lifecycle sanity checks.
	struct OrderSink : ICellFieldCommandSink
	{
		std::vector<char>                    order;		// C / U / D
		std::vector<CellFieldLayerHandle>    handles;
		std::vector<float>                   firstValue;	// first value of each U (parallel to order, 0 for non-U)
		std::map<uint32_t, float>            lastValue;		// by handle key
		std::set<uint32_t>                   live;
		uint64_t                             updates = 0;
		uint64_t                             lifecycleViolations = 0;

		void OnCreate(CellFieldLayerHandle h, const CellGridDesc&, CellFieldValueKind) override
		{
			order.push_back('C'); handles.push_back(h); firstValue.push_back(0.0f);
			if (!live.insert(Key(h)).second) { ++lifecycleViolations; }
		}
		void OnUpdate(CellFieldLayerHandle h, const float* v, uint32_t c) override
		{
			order.push_back('U'); handles.push_back(h); firstValue.push_back(v[0]);
			++updates;
			if (live.find(Key(h)) == live.end()) { ++lifecycleViolations; }
			for (uint32_t i = 1; i < c; ++i) { if (v[i] != v[0]) { ++lifecycleViolations; break; } }	// torn copy check
			lastValue[Key(h)] = v[0];
		}
		void OnDestroy(CellFieldLayerHandle h) override
		{
			order.push_back('D'); handles.push_back(h); firstValue.push_back(0.0f);
			if (live.erase(Key(h)) == 0) { ++lifecycleViolations; }
		}
		std::string Order() const { return std::string(order.begin(), order.end()); }
	};

	std::vector<float> Filled(uint32_t n, float v) { return std::vector<float>(n, v); }
}

//------------------------------------------------------------------------------
// (a) coalescing
//------------------------------------------------------------------------------
TEST(CellFieldRegistryCoalescing, ManyUpdatesDeliverOneWithLastValues)
{
	CellFieldRegistry reg;
	GridStorage g(4, 3);
	CellFieldLayerHandle h;
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	OrderSink sink;
	reg.Drain(sink);

	const uint32_t N = 50;
	for (uint32_t i = 1; i <= N; ++i)
	{
		std::vector<float> v = Filled(12, (float)i);
		ASSERT_TRUE(reg.Update(h, v.data(), 12).ok);
	}
	const CellFieldStats before = reg.GetStats();
	EXPECT_EQ(before.coalescedCommands, (uint64_t)(N - 1));
	EXPECT_EQ(before.commandsQueued, 1u);
	EXPECT_EQ(before.queuedBytes, 12u * sizeof(float));

	sink.order.clear(); sink.firstValue.clear(); sink.handles.clear();
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "U");
	EXPECT_EQ(sink.firstValue[0], (float)N);
	EXPECT_EQ(reg.GetStats().uploads, 1u);
}

TEST(CellFieldRegistryCoalescing, TenThousandLargeUpdatesNeverHitCap)
{
	CellFieldRegistry reg;
	GridStorage g(128, 128);
	CellFieldLayerHandle h;
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	OrderSink sink;
	reg.Drain(sink);

	const uint32_t cells = 128u * 128u;
	std::vector<float> v(cells);
	for (uint32_t i = 1; i <= 10000; ++i)
	{
		std::fill(v.begin(), v.end(), (float)i);
		ASSERT_TRUE(reg.Update(h, v.data(), cells).ok) << "update " << i;
	}
	const CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.droppedCommands, 0u);
	EXPECT_EQ(s.capWarningsLogged, 0u);
	EXPECT_EQ(s.coalescedCommands, 9999u);
	EXPECT_EQ(s.commandsQueued, 1u);
	EXPECT_EQ(s.queuedBytes, (uint64_t)cells * sizeof(float));

	sink.order.clear(); sink.firstValue.clear(); sink.handles.clear();
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "U");
	EXPECT_EQ(sink.firstValue[0], 10000.0f);
	EXPECT_EQ(sink.lifecycleViolations, 0u);
}

TEST(CellFieldRegistryCoalescing, UpdatesAfterDrainStartFreshPendingUpdate)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	OrderSink sink;
	for (int round = 1; round <= 3; ++round)
	{
		std::vector<float> a = Filled(4, (float)round), b = Filled(4, (float)(round * 10));
		reg.Update(h, a.data(), 4);
		reg.Update(h, b.data(), 4);
		reg.Drain(sink);
	}
	EXPECT_EQ(sink.Order(), "CUUU");
	EXPECT_EQ(sink.lastValue[Key(h)], 30.0f);
	EXPECT_EQ(reg.GetStats().coalescedCommands, 3u);
	EXPECT_EQ(reg.GetStats().uploads, 3u);
}

//------------------------------------------------------------------------------
// (b) ordering rules
// Position rule: the replacement keeps the position of the FIRST pending Update. Between the
// first pending Update and a later one for the same layer the only possible commands for that
// layer are further Updates (a Destroy makes the handle stale immediately), so in-place
// replacement is equivalent to applying all updates in order, and stays after its Create.
//------------------------------------------------------------------------------
TEST(CellFieldRegistryCoalescing, CreateUpdateUpdateDeliversCreateThenLastUpdate)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	std::vector<float> a = Filled(4, 1.0f), b = Filled(4, 2.0f);
	reg.Update(h, a.data(), 4);
	reg.Update(h, b.data(), 4);
	OrderSink sink;
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "CU");
	EXPECT_EQ(sink.firstValue[1], 2.0f);
	EXPECT_EQ(sink.lifecycleViolations, 0u);
}

TEST(CellFieldRegistryCoalescing, UpdateThenDestroyKeepsOrderAndLaterUpdateIsStale)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	OrderSink sink;
	reg.Drain(sink);
	std::vector<float> a = Filled(4, 1.0f), b = Filled(4, 2.0f);
	EXPECT_TRUE(reg.Update(h, a.data(), 4).ok);
	reg.Destroy(h);
	CellFieldResult late = reg.Update(h, b.data(), 4);
	EXPECT_FALSE(late.ok);
	EXPECT_EQ(late.error, CellFieldRegistry::kErrStaleHandle);
	EXPECT_EQ(reg.GetStats().coalescedCommands, 0u);
	sink.order.clear(); sink.firstValue.clear(); sink.handles.clear();
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "UD");
	EXPECT_EQ(sink.firstValue[0], 1.0f);	// the pre-destroy value, never the stale one
}

TEST(CellFieldRegistryCoalescing, DifferentLayersAreNotMerged)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle l1, l2;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, l1);
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, l2);
	std::vector<float> a = Filled(4, 1.0f), b = Filled(4, 2.0f), c = Filled(4, 3.0f);
	reg.Update(l1, a.data(), 4);
	reg.Update(l2, b.data(), 4);
	reg.Update(l1, c.data(), 4);
	EXPECT_EQ(reg.GetStats().coalescedCommands, 1u);
	OrderSink sink;
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "CCUU");
	EXPECT_EQ(Key(sink.handles[2]), Key(l1));
	EXPECT_EQ(sink.firstValue[2], 3.0f);
	EXPECT_EQ(Key(sink.handles[3]), Key(l2));
	EXPECT_EQ(sink.firstValue[3], 2.0f);
}

TEST(CellFieldRegistryCoalescing, SlotReuseAfterDestroyDoesNotInheritPendingUpdate)
{
	CellFieldRegistry reg;
	GridStorage g(2, 2);
	CellFieldLayerHandle h1;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h1);
	std::vector<float> a = Filled(4, 1.0f), b = Filled(4, 2.0f);
	reg.Update(h1, a.data(), 4);
	reg.Destroy(h1);
	OrderSink sink;
	reg.Drain(sink);	// C U D, slot freed
	CellFieldLayerHandle h2;
	reg.CreateLayer(g.desc, CellFieldValueKind::Float, h2);
	ASSERT_EQ(h2.index, h1.index);
	ASSERT_NE(h2.generation, h1.generation);
	reg.Update(h2, b.data(), 4);
	sink.order.clear(); sink.firstValue.clear(); sink.handles.clear();
	reg.Drain(sink);
	ASSERT_EQ(sink.Order(), "CU");
	EXPECT_EQ(Key(sink.handles[1]), Key(h2));
	EXPECT_EQ(sink.firstValue[1], 2.0f);
	EXPECT_EQ(sink.lifecycleViolations, 0u);
}

//------------------------------------------------------------------------------
// (c) byte cap
//------------------------------------------------------------------------------
TEST(CellFieldRegistryByteCap, DropsCountsAndWarnsOncePerDrainCycle)
{
	CellFieldRegistry reg;
	GridStorage g(128, 128);	// 512 KB per Create -> 16 fill the 8 MB cap exactly
	const auto start = std::chrono::steady_clock::now();

	std::vector<CellFieldLayerHandle> layers;
	for (int i = 0; i < 16; ++i)
	{
		CellFieldLayerHandle h;
		ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok) << i;
		layers.push_back(h);
	}
	EXPECT_EQ(reg.GetStats().queuedBytes, CellFieldRegistry::kMaxQueueBytes);

	CellFieldLayerHandle extra;
	CellFieldResult r = reg.CreateLayer(g.desc, CellFieldValueKind::Float, extra);
	EXPECT_FALSE(r.ok);
	EXPECT_EQ(r.error, CellFieldRegistry::kErrQueueFull);

	std::vector<float> v = Filled(128u * 128u, 1.0f);
	for (auto& h : layers)
	{
		CellFieldResult u = reg.Update(h, v.data(), 128u * 128u);
		EXPECT_FALSE(u.ok);
		EXPECT_EQ(u.error, CellFieldRegistry::kErrQueueFull);
	}
	CellFieldStats s = reg.GetStats();
	EXPECT_EQ(s.droppedCommands, 17u);
	EXPECT_EQ(s.capWarningsLogged, 1u);
	EXPECT_EQ(s.queuedBytes, CellFieldRegistry::kMaxQueueBytes);

	// Destroy is never dropped, even when full.
	reg.Destroy(layers[0]);
	EXPECT_EQ(reg.GetStats().destroys, 1u);

	OrderSink sink;
	reg.Drain(sink);
	EXPECT_EQ(sink.updates, 0u);
	EXPECT_EQ(sink.Order().back(), 'D');

	// Submission works again after the drain; a new overflow cycle warns once more.
	EXPECT_TRUE(reg.Update(layers[1], v.data(), 128u * 128u).ok);
	EXPECT_EQ(reg.GetStats().capWarningsLogged, 1u);
	for (int i = 0; i < 16; ++i) { CellFieldLayerHandle h; reg.CreateLayer(g.desc, CellFieldValueKind::Float, h); }
	EXPECT_EQ(reg.GetStats().capWarningsLogged, 2u);

	const auto elapsed = std::chrono::steady_clock::now() - start;
	EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10) << "submit must never block";
}

TEST(CellFieldRegistryByteCap, CoalescedUpdateIsNeverDroppedWhenQueueIsFull)
{
	CellFieldRegistry reg;
	GridStorage small(2, 2), big(128, 128);
	CellFieldLayerHandle h;
	reg.CreateLayer(small.desc, CellFieldValueKind::Float, h);
	std::vector<float> v = Filled(4, 1.0f);
	ASSERT_TRUE(reg.Update(h, v.data(), 4).ok);
	// Fill the remainder of the queue with big Creates.
	for (int i = 0; i < 16; ++i) { CellFieldLayerHandle t; reg.CreateLayer(big.desc, CellFieldValueKind::Float, t); }
	const uint64_t dropsBefore = reg.GetStats().droppedCommands;
	v = Filled(4, 9.0f);
	EXPECT_TRUE(reg.Update(h, v.data(), 4).ok);	// replaces in place: no extra bytes
	EXPECT_EQ(reg.GetStats().droppedCommands, dropsBefore);
	OrderSink sink;
	reg.Drain(sink);
	EXPECT_EQ(sink.lastValue[Key(h)], 9.0f);
}

//------------------------------------------------------------------------------
// (d) threads
//------------------------------------------------------------------------------
TEST(CellFieldRegistryThreading, ProducerTenThousandUpdatesWhileConsumerDrains)
{
	CellFieldRegistry reg;
	GridStorage g(4, 3);
	CellFieldLayerHandle a;
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, a).ok);

	std::atomic<bool> done{ false };
	uint64_t submitted = 0, dropped = 0, stale = 0, accepted = 0;
	const int kUpdates = 10000;

	std::thread producer([&]
	{
		std::vector<float> v(12);
		auto submit = [&](CellFieldLayerHandle h, float value)
		{
			std::fill(v.begin(), v.end(), value);
			++submitted;
			CellFieldResult r = reg.Update(h, v.data(), 12);
			if (r.ok) { ++accepted; }
			else if (r.error == CellFieldRegistry::kErrQueueFull) { ++dropped; }
			else if (r.error == CellFieldRegistry::kErrStaleHandle) { ++stale; }
		};
		for (int i = 1; i <= kUpdates; ++i)
		{
			submit(a, (float)i);
			if (i % 500 == 0)
			{
				CellFieldLayerHandle b;
				if (reg.CreateLayer(g.desc, CellFieldValueKind::Float, b).ok)
				{
					submit(b, -1.0f);
					reg.Destroy(b);
					submit(b, -2.0f);	// stale by construction
				}
			}
		}
		done = true;
	});

	OrderSink sink;
	while (!done.load()) { reg.Drain(sink); }
	producer.join();
	reg.Drain(sink);

	const CellFieldStats s = reg.GetStats();
	EXPECT_EQ(sink.lifecycleViolations, 0u);
	EXPECT_EQ(sink.lastValue[Key(a)], (float)kUpdates);
	EXPECT_EQ(dropped, 0u);
	EXPECT_EQ(submitted, sink.updates + s.coalescedCommands + dropped + stale);
	EXPECT_EQ(accepted, sink.updates + s.coalescedCommands);
	EXPECT_EQ(s.uploads, sink.updates);
	EXPECT_EQ(s.commandsQueued, 0u);
	EXPECT_EQ(s.queuedBytes, 0u);
	EXPECT_EQ(s.layersAllocated, 1u);
}

//------------------------------------------------------------------------------
// Renderer-level: fake GPU factories, null program, drain on this thread, producer on another
//------------------------------------------------------------------------------
namespace
{
	using namespace Dia::Bgfx;

	class FakeGeo : public ICellFieldGpuGeometryFactory
	{
	public:
		bool Create(const CellGridGeometry&, CellFieldGpuHandle& out) override { out = nextHandle++; return true; }
		void Destroy(CellFieldGpuHandle) override {}
		uint32_t nextHandle = 1;
	};

	class FakeTex : public ICellFieldGpuTextureFactory
	{
	public:
		bool Create(uint16_t, uint16_t, CellFieldGpuTextureHandle& out) override { out = nextHandle++; return true; }
		void Update(CellFieldGpuTextureHandle h, const float* v, uint32_t count) override
		{
			++updateCalls;
			lastFirst[h] = count ? v[0] : 0.0f;
			for (uint32_t i = 1; i < count; ++i) { if (v[i] != v[0]) { ++torn; break; } }
		}
		void Destroy(CellFieldGpuTextureHandle) override {}
		uint32_t nextHandle = 1;
		uint64_t updateCalls = 0, torn = 0;
		std::map<CellFieldGpuTextureHandle, float> lastFirst;
	};

	class FakeColourMap : public ICellFieldGpuColourMapFactory
	{
	public:
		bool CreateRgba8(uint32_t, const uint32_t*, uint32_t, bool, CellFieldGpuColourMapHandle& out) override { out = nextHandle++; return true; }
		void UpdateRgba8(CellFieldGpuColourMapHandle, const uint32_t*, uint32_t) override {}
		void Destroy(CellFieldGpuColourMapHandle) override {}
		uint32_t nextHandle = 1;
	};

	struct RendererFixture
	{
		FakeGeo geo; FakeTex tex; FakeColourMap cmap;
		CellFieldRegistry registry;
		Dia::Bgfx::CellFieldRenderer renderer{ 0, nullptr, &registry, geo, tex, cmap };
		HexDescStorage storage;
		CellGridDesc desc;
		RendererFixture()
		{
			Dia::Pathfinding::HexTopology topo(4, 3);
			desc = HexTopologyToCellGridDesc(topo, 1.0f, storage);
		}
	};
}

TEST(CellFieldRendererThreading, ProducerOnOtherThreadFinalTextureEqualsLastUpdate)
{
	RendererFixture f;
	CellFieldLayerHandle a;
	ASSERT_TRUE(f.registry.CreateLayer(f.desc, CellFieldValueKind::Float, a).ok);
	f.renderer.DrainRegistry();

	std::atomic<bool> done{ false };
	uint64_t submitted = 0, notOk = 0;
	std::thread producer([&]
	{
		std::vector<float> v(12);
		for (int i = 1; i <= 10000; ++i)
		{
			std::fill(v.begin(), v.end(), (float)i);
			++submitted;
			if (!f.registry.Update(a, v.data(), 12).ok) { ++notOk; }
		}
		done = true;
	});
	CellFieldFrameData frame;
	while (!done.load()) { f.renderer.DrainRegistry(); f.renderer.Draw(frame); }
	producer.join();
	f.renderer.DrainRegistry();

	EXPECT_EQ(notOk, 0u);
	EXPECT_EQ(f.tex.torn, 0u);
	ASSERT_EQ(f.tex.lastFirst.size(), 1u);
	EXPECT_EQ(f.tex.lastFirst.begin()->second, 10000.0f);
	const CellFieldStats s = f.registry.GetStats();
	EXPECT_EQ(submitted, s.uploads + s.coalescedCommands);
	EXPECT_EQ(f.tex.updateCalls, s.uploads);
}

//------------------------------------------------------------------------------
// (e) Destroy racing a draw
//------------------------------------------------------------------------------
TEST(CellFieldRendererThreading, DestroyRacingDrawIsSkippedAndCounted)
{
	RendererFixture f;
	CellFieldLayerHandle h;
	ASSERT_TRUE(f.registry.CreateLayer(f.desc, CellFieldValueKind::Float, h).ok);
	f.renderer.DrainRegistry();

	CellFieldFrameData frame;
	Dia::Core::CellFieldDrawRequest req;
	req.layer = h;
	frame.RequestDrawCellFieldData(req);

	std::atomic<bool> go{ false };
	std::thread producer([&]
	{
		while (!go.load()) {}
		f.registry.Destroy(h);
	});
	go = true;
	for (int i = 0; i < 2000; ++i) { f.renderer.Draw(frame); }
	producer.join();

	const uint64_t before = f.registry.GetStats().staleHandles;
	f.renderer.Draw(frame);	// destroy is certainly visible now
	EXPECT_GT(f.registry.GetStats().staleHandles, before);
	EXPECT_GE(f.registry.GetStats().staleHandles, 1u);
	EXPECT_EQ(f.registry.GetStats().destroys, 1u);
}
