////////////////////////////////////////////////////////////////////////////////
// TestCellFieldGapsRegistry.cpp
// Task 26 gap-audit tests: CellFieldRegistry (handles, cap, colour maps, drain, stats) - DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <algorithm>
#include <atomic>
#include <stdio.h>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string.h>
#include <thread>
#include <vector>

using namespace Dia::Graphics;
using Dia::Core::CellFieldLayerHandle;
using Dia::Core::StringCRC;

namespace
{
	struct Grid
	{
		std::vector<float>   centres;
		std::vector<int32_t> neighbours;
		CellGridDesc         desc;

		Grid(uint16_t w, uint16_t h)
		{
			const uint32_t n = (uint32_t)w * h;
			centres.assign(2 * n, 1.5f);
			neighbours.assign(6 * n, -1);
			desc.width = w;
			desc.height = h;
			desc.cellSize = 2.0f;
			desc.topology = StringCRC("hex.flat.oddq");
			desc.centresXY = centres.data();
			desc.neighbours = neighbours.data();
		}
	};

	struct Rec
	{
		char type = '?';
		CellFieldLayerHandle h;
		CellFieldValueKind kind = CellFieldValueKind::Category;
		CellGridDesc desc;
		const float* centresPtr = nullptr;
		const int32_t* neighboursPtr = nullptr;
		std::vector<float> centres;
		std::vector<int32_t> neighbours;
		std::vector<float> values;
	};

	struct RecSink : ICellFieldCommandSink
	{
		std::vector<Rec> recs;
		void OnCreate(CellFieldLayerHandle h, const CellGridDesc& d, CellFieldValueKind k) override
		{
			Rec r; r.type = 'C'; r.h = h; r.kind = k; r.desc = d;
			r.centresPtr = d.centresXY; r.neighboursPtr = d.neighbours;
			r.centres.assign(d.centresXY, d.centresXY + 2u * d.width * d.height);
			r.neighbours.assign(d.neighbours, d.neighbours + 6u * d.width * d.height);
			recs.push_back(r);
		}
		void OnUpdate(CellFieldLayerHandle h, const float* v, uint32_t c) override
		{
			Rec r; r.type = 'U'; r.h = h; r.values.assign(v, v + c); recs.push_back(r);
		}
		void OnDestroy(CellFieldLayerHandle h) override { Rec r; r.type = 'D'; r.h = h; recs.push_back(r); }
		std::string Order() const { std::string s; for (const Rec& r : recs) { s += r.type; } return s; }
	};

	struct NullSink : ICellFieldCommandSink
	{
		void OnCreate(CellFieldLayerHandle, const CellGridDesc&, CellFieldValueKind) override {}
		void OnUpdate(CellFieldLayerHandle, const float*, uint32_t) override {}
		void OnDestroy(CellFieldLayerHandle) override {}
	};

	// Re-entrant sink: each callback may run a user hook that calls back into the registry.
	struct HookSink : ICellFieldCommandSink
	{
		std::function<void(CellFieldLayerHandle)> onCreate, onUpdate, onDestroy;
		std::string order;
		void OnCreate(CellFieldLayerHandle h, const CellGridDesc&, CellFieldValueKind) override { order += 'C'; if (onCreate) { onCreate(h); } }
		void OnUpdate(CellFieldLayerHandle h, const float*, uint32_t) override { order += 'U'; if (onUpdate) { onUpdate(h); } }
		void OnDestroy(CellFieldLayerHandle h) override { order += 'D'; if (onDestroy) { onDestroy(h); } }
	};

	// Runs fn on a worker thread; true if it finished within the timeout. On timeout the worker is
	// detached (callers keep all state in shared_ptrs) so a deadlock fails instead of hanging the run.
	bool FinishesWithin(std::function<void()> fn, int seconds)
	{
		auto done = std::make_shared<std::atomic<bool>>(false);
		std::thread t([fn, done]() { fn(); done->store(true); });
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
		while (!done->load() && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (done->load()) { t.join(); return true; }
		t.detach();
		return false;
	}

	typedef std::map<std::string, int64_t> DeltaMap;

	DeltaMap Delta(const CellFieldStats& a, const CellFieldStats& b)
	{
		DeltaMap d;
#define DELTA(f) if ((int64_t)b.f - (int64_t)a.f != 0) { d[#f] = (int64_t)b.f - (int64_t)a.f; }
		DELTA(layersAllocated) DELTA(layersLive) DELTA(commandsQueued) DELTA(queuedBytes)
		DELTA(creates) DELTA(updates) DELTA(destroys) DELTA(uploads) DELTA(uploadedBytes)
		DELTA(geometryBuilds) DELTA(droppedCommands) DELTA(staleHandles) DELTA(rejectedRequests)
		DELTA(coalescedCommands) DELTA(capWarningsLogged) DELTA(lifecycleLogsEmitted) DELTA(lifecycleLogsSuppressed)
#undef DELTA
		return d;
	}

	std::string Str(const DeltaMap& m)
	{
		std::string s;
		for (const auto& e : m) { s += e.first + "=" + std::to_string(e.second) + " "; }
		return s;
	}

	CellFieldLayerHandle Make(CellFieldRegistry& reg, uint16_t w = 1, uint16_t h = 1, CellFieldValueKind k = CellFieldValueKind::Float)
	{
		Grid g(w, h);
		CellFieldLayerHandle out;
		reg.CreateLayer(g.desc, k, out);
		return out;
	}

	CellFieldLayerHandle Handle(uint16_t index, uint16_t gen)
	{
		CellFieldLayerHandle h;
		h.index = index;
		h.generation = gen;
		return h;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Garbage / default / out-of-range handles
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, GarbageHandlesAreStaleInEveryApiWithoutSideEffects)
{
	CellFieldRegistry reg;
	Make(reg, 2, 2);	// a live layer at slot 0 so garbage cannot hit an empty-table shortcut
	const uint16_t indices[] = { 0xFFFF, 0xFFFE, 0x8000, 32, 33, 64, 1000 };
	float v[4] = { 1, 2, 3, 4 };
	for (uint16_t idx : indices)
	{
		for (uint16_t gen : { (uint16_t)0, (uint16_t)1, (uint16_t)0xFFFF })
		{
			const CellFieldLayerHandle h = Handle(idx, gen);
			const CellFieldStats before = reg.GetStats();
			EXPECT_FALSE(reg.ResolveHandle(h)) << idx;
			CellFieldResult r = reg.Update(h, v, 4);
			EXPECT_FALSE(r.ok);
			EXPECT_STREQ(CellFieldRegistry::kErrStaleHandle, r.error);
			reg.Update(h, nullptr, 0);
			reg.Update(h, nullptr, 0xFFFFFFFFu);
			reg.Update(h, v, 0xFFFFFFFFu);
			reg.Destroy(h);
			const CellFieldStats after = reg.GetStats();
			EXPECT_EQ(before.staleHandles + 6, after.staleHandles) << idx;
			EXPECT_EQ(before.commandsQueued, after.commandsQueued);
			EXPECT_EQ(before.queuedBytes, after.queuedBytes);
			EXPECT_EQ(before.layersLive, after.layersLive);
			EXPECT_EQ(before.rejectedRequests, after.rejectedRequests);
			EXPECT_EQ(before.destroys, after.destroys);
		}
	}
}

TEST(CellFieldGapsRegistry, DefaultHandleIsInvalidAndNeverResolves)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle def;
	EXPECT_FALSE(def.IsValid());
	EXPECT_EQ(0xFFFF, def.index);
	EXPECT_FALSE(reg.ResolveHandle(def));
	Make(reg);
	EXPECT_FALSE(reg.ResolveHandle(def));
}

TEST(CellFieldGapsRegistry, DestroyOfNeverCreatedSlotZeroOnEmptyRegistryIsStale)
{
	CellFieldRegistry reg;
	reg.Destroy(Handle(0, 0));
	reg.Destroy(Handle(31, 0));
	const CellFieldStats s = reg.GetStats();
	EXPECT_EQ(2u, s.staleHandles);
	EXPECT_EQ(0u, s.destroys);
	EXPECT_EQ(0u, s.commandsQueued);
}

TEST(CellFieldGapsRegistry, DestroyTwiceQueuesExactlyOneDestroy)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg);
	reg.Destroy(h);
	reg.Destroy(h);
	RecSink sink;
	reg.Drain(sink);
	EXPECT_EQ("CD", sink.Order());
	reg.Destroy(h);	// after drain: still just stale
	reg.Drain(sink);
	EXPECT_EQ("CD", sink.Order());
}

TEST(CellFieldGapsRegistry, StaleDestroyDoesNotKillRecycledSlotOccupant)
{
	CellFieldRegistry reg;
	NullSink sink;
	const CellFieldLayerHandle a = Make(reg);
	reg.Destroy(a);
	reg.Drain(sink);
	const CellFieldLayerHandle b = Make(reg);
	ASSERT_EQ(a.index, b.index);
	reg.Destroy(a);	// old generation
	EXPECT_TRUE(reg.ResolveHandle(b));
	EXPECT_EQ(0u, reg.GetStats().destroys - 1u);	// only the first destroy counted
	EXPECT_EQ(1u, reg.GetStats().layersLive);
}

TEST(CellFieldGapsRegistry, AfterDrainUpdateAndDestroyOfDestroyedLayerProduceNoCallbacks)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 2, 2);
	RecSink sink;
	reg.Destroy(h);
	reg.Drain(sink);
	sink.recs.clear();
	float v[4] = {};
	EXPECT_FALSE(reg.Update(h, v, 4).ok);
	reg.Destroy(h);
	reg.Drain(sink);
	EXPECT_TRUE(sink.recs.empty());
	EXPECT_EQ(0u, reg.GetStats().commandsQueued);
}

TEST(CellFieldGapsRegistry, UpdateBeforeCreateDrainedIsAcceptedAndOrdered)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 2, 1);
	const float v[2] = { 3, 4 };
	ASSERT_TRUE(reg.Update(h, v, 2).ok);
	RecSink sink;
	reg.Drain(sink);
	EXPECT_EQ("CU", sink.Order());
	EXPECT_EQ(h.index, sink.recs[1].h.index);
	EXPECT_FLOAT_EQ(4.0f, sink.recs[1].values[1]);
}

////////////////////////////////////////////////////////////////////////////////
// Slot reuse and generations
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, SlotsAllocatedLowestFreeFirst)
{
	CellFieldRegistry reg;
	NullSink sink;
	CellFieldLayerHandle h[6];
	for (int i = 0; i < 6; ++i) { h[i] = Make(reg); EXPECT_EQ(i, h[i].index); }
	reg.Destroy(h[4]);
	reg.Destroy(h[1]);
	reg.Destroy(h[3]);
	reg.Drain(sink);
	EXPECT_EQ(1, Make(reg).index);
	EXPECT_EQ(3, Make(reg).index);
	EXPECT_EQ(4, Make(reg).index);
	EXPECT_EQ(6, Make(reg).index);
}

TEST(CellFieldGapsRegistry, GenerationBumpsOncePerRecycleAndOldHandlesStayDead)
{
	CellFieldRegistry reg;
	NullSink sink;
	std::vector<CellFieldLayerHandle> seen;
	for (int i = 0; i < 10; ++i)
	{
		const CellFieldLayerHandle h = Make(reg);
		EXPECT_EQ(0, h.index);
		EXPECT_EQ(i, h.generation);
		seen.push_back(h);
		reg.Destroy(h);
		reg.Drain(sink);
	}
	for (const CellFieldLayerHandle& old : seen) { EXPECT_FALSE(reg.ResolveHandle(old)); }
}

TEST(CellFieldGapsRegistry, GenerationIsNotBumpedUntilDrain)
{
	CellFieldRegistry reg;
	NullSink sink;
	const CellFieldLayerHandle a = Make(reg);
	reg.Destroy(a);
	const CellFieldLayerHandle b = Make(reg);	// slot 0 still held -> slot 1
	EXPECT_EQ(1, b.index);
	EXPECT_EQ(0, b.generation);
	reg.Drain(sink);
	const CellFieldLayerHandle c = Make(reg);
	EXPECT_EQ(0, c.index);
	EXPECT_EQ(1, c.generation);
}

// Generation is 16-bit: a handle exactly 65,536 recycles of its slot old aliases the current
// occupant. This is a documented limitation (the handle is 4 bytes and trivially copyable; widening
// it would change the draw-request ABI). Handles from any closer lap must stay dead. This test pins
// both halves of that contract so a future change to either is deliberate.
TEST(CellFieldGapsRegistry, GenerationWrapAfter65536RecyclesAliasesOnlyTheExactAncientHandle)
{
	CellFieldRegistry reg;
	NullSink sink;
	const CellFieldLayerHandle ancient = Make(reg);
	ASSERT_EQ(0, ancient.generation);
	reg.Destroy(ancient);
	reg.Drain(sink);

	CellFieldLayerHandle previousLap;
	for (uint32_t i = 1; i < 65536u; ++i)
	{
		const CellFieldLayerHandle h = Make(reg);
		ASSERT_EQ(0, h.index);
		ASSERT_EQ((uint16_t)i, h.generation);
		if (i == 65535u) { previousLap = h; }
		reg.Destroy(h);
		reg.Drain(sink);
	}
	// 65,536 recycles done: the slot is back at generation 0.
	const CellFieldLayerHandle wrapped = Make(reg);
	EXPECT_EQ(0, wrapped.index);
	EXPECT_EQ(0, wrapped.generation);
	EXPECT_TRUE(reg.ResolveHandle(wrapped));
	EXPECT_FALSE(reg.ResolveHandle(previousLap));	// one lap-step old: dead
	EXPECT_FALSE(reg.ResolveHandle(Handle(0, 1)));
	EXPECT_FALSE(reg.ResolveHandle(Handle(0, 32768)));
	EXPECT_TRUE(reg.ResolveHandle(ancient));		// documented alias after a full 2^16 wrap
}

////////////////////////////////////////////////////////////////////////////////
// Create
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, CreateCopiesBothArraysAndDeliversOwnedDescription)
{
	CellFieldRegistry reg;
	Grid g(3, 2);
	for (size_t i = 0; i < g.centres.size(); ++i) { g.centres[i] = (float)i; }
	for (size_t i = 0; i < g.neighbours.size(); ++i) { g.neighbours[i] = (int32_t)(i % 6) - 1; }
	const std::vector<float> wantCentres = g.centres;
	const std::vector<int32_t> wantNeighbours = g.neighbours;
	const float* origCentres = g.centres.data();
	const int32_t* origNeighbours = g.neighbours.data();
	CellFieldLayerHandle h;
	ASSERT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Category, h).ok);

	// Caller overwrites then frees its memory after the call.
	std::fill(g.centres.begin(), g.centres.end(), -1.0f);
	std::fill(g.neighbours.begin(), g.neighbours.end(), 12345);
	g.centres.clear(); g.centres.shrink_to_fit();
	g.neighbours.clear(); g.neighbours.shrink_to_fit();

	RecSink sink;
	reg.Drain(sink);
	ASSERT_EQ(1u, sink.recs.size());
	const Rec& r = sink.recs[0];
	EXPECT_EQ(wantCentres, r.centres);
	EXPECT_EQ(wantNeighbours, r.neighbours);
	EXPECT_NE(origCentres, r.centresPtr);
	EXPECT_NE(origNeighbours, r.neighboursPtr);
	EXPECT_EQ(3, r.desc.width);
	EXPECT_EQ(2, r.desc.height);
	EXPECT_FLOAT_EQ(2.0f, r.desc.cellSize);
	EXPECT_TRUE(r.desc.topology == StringCRC("hex.flat.oddq"));
	EXPECT_EQ(CellFieldValueKind::Category, r.kind);
	EXPECT_EQ(h.index, r.h.index);
	EXPECT_EQ(h.generation, r.h.generation);
}

TEST(CellFieldGapsRegistry, RejectedCreateConsumesNoSlotAndInvalidatesOutHandle)
{
	CellFieldRegistry reg;
	CellFieldLayerHandle h = Make(reg);
	ASSERT_TRUE(h.IsValid());
	Grid bad(0, 4);
	EXPECT_FALSE(reg.CreateLayer(bad.desc, CellFieldValueKind::Float, h).ok);
	EXPECT_FALSE(h.IsValid());	// out reset even though it held a valid handle
	Grid wide(65535, 1);
	EXPECT_FALSE(reg.CreateLayer(wide.desc, CellFieldValueKind::Float, h).ok);
	Grid ok(1, 1);
	CellFieldLayerHandle next;
	ASSERT_TRUE(reg.CreateLayer(ok.desc, CellFieldValueKind::Float, next).ok);
	EXPECT_EQ(1, next.index);	// slot 0 is the first layer; rejected creates took nothing
	EXPECT_EQ(2u, reg.GetStats().layersAllocated);
}

TEST(CellFieldGapsRegistry, AllShapesFromOneByOneToMaxAreAccepted)
{
	CellFieldRegistry reg;
	NullSink sink;
	const uint16_t dims[][2] = { {1,1}, {1,128}, {128,1}, {2,2}, {127,128}, {128,128}, {3,5}, {64,64} };
	for (const auto& d : dims)
	{
		Grid g(d[0], d[1]);
		CellFieldLayerHandle h;
		EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok) << d[0] << "x" << d[1];
		reg.Destroy(h);
		reg.Drain(sink);
	}
}

TEST(CellFieldGapsRegistry, RegisterSupportedTopologyIsIdempotentAndUnlimitedEnough)
{
	CellFieldRegistry reg;
	Grid g(2, 2);
	g.desc.topology = StringCRC("square.4");
	CellFieldLayerHandle h;
	EXPECT_FALSE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	for (int i = 0; i < 5; ++i) { reg.RegisterSupportedTopology(StringCRC("square.4")); }
	EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	// default topology still supported, re-registering it is harmless
	reg.RegisterSupportedTopology(StringCRC("hex.flat.oddq"));
	Grid hex(2, 2);
	EXPECT_TRUE(reg.CreateLayer(hex.desc, CellFieldValueKind::Float, h).ok);
	// many distinct topologies do not break lookups
	char name[32];
	for (int i = 0; i < 200; ++i) { snprintf(name, sizeof(name), "topo%d", i); reg.RegisterSupportedTopology(StringCRC(name)); }
	g.desc.topology = StringCRC("topo199");
	EXPECT_TRUE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
	g.desc.topology = StringCRC("topo200");
	EXPECT_FALSE(reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok);
}

TEST(CellFieldGapsRegistry, UnsupportedTopologyErrorNotCountedAsQueued)
{
	CellFieldRegistry reg;
	Grid g(2, 2);
	g.desc.topology = StringCRC("nope");
	CellFieldLayerHandle h;
	CellFieldResult r = reg.CreateLayer(g.desc, CellFieldValueKind::Float, h);
	EXPECT_STREQ(CellFieldRegistry::kErrUnsupportedTopology, r.error);
	EXPECT_EQ(0u, reg.GetStats().commandsQueued);
	EXPECT_EQ(0u, reg.GetStats().layersAllocated);
}

////////////////////////////////////////////////////////////////////////////////
// Update
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, UpdateExactly128x128AcceptedAndDelivered)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 128, 128);
	std::vector<float> v(128u * 128u);
	for (size_t i = 0; i < v.size(); ++i) { v[i] = (float)i; }
	ASSERT_TRUE(reg.Update(h, v.data(), (uint32_t)v.size()).ok);
	EXPECT_FALSE(reg.Update(h, v.data(), (uint32_t)v.size() - 1).ok);
	EXPECT_FALSE(reg.Update(h, v.data(), (uint32_t)v.size() + 1).ok);
	RecSink sink;
	reg.Drain(sink);
	ASSERT_EQ(2u, sink.recs.size());
	EXPECT_EQ(v, sink.recs[1].values);
	EXPECT_EQ(128u * 128u * 4u, reg.GetStats().uploadedBytes);
}

TEST(CellFieldGapsRegistry, UpdateNaNAndInfAreCopiedVerbatim)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 4, 1);
	uint32_t bits[4] = { 0x7FC00001u, 0x7F800000u, 0xFF800000u, 0x80000000u };	// NaN payload, +inf, -inf, -0
	float v[4];
	memcpy(v, bits, sizeof(v));
	ASSERT_TRUE(reg.Update(h, v, 4).ok);
	RecSink sink;
	reg.Drain(sink);
	ASSERT_EQ(2u, sink.recs.size());
	ASSERT_EQ(4u, sink.recs[1].values.size());
	EXPECT_EQ(0, memcmp(bits, sink.recs[1].values.data(), sizeof(bits)));
}

TEST(CellFieldGapsRegistry, UpdateAcceptedForCategoryAndFloatLayersAlike)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle c = Make(reg, 2, 1, CellFieldValueKind::Category);
	const CellFieldLayerHandle f = Make(reg, 2, 1, CellFieldValueKind::Float);
	const float v[2] = { 1, 2 };
	EXPECT_TRUE(reg.Update(c, v, 2).ok);
	EXPECT_TRUE(reg.Update(f, v, 2).ok);
	RecSink sink;
	reg.Drain(sink);
	EXPECT_EQ("CCUU", sink.Order());
	EXPECT_EQ(CellFieldValueKind::Category, sink.recs[0].kind);
	EXPECT_EQ(CellFieldValueKind::Float, sink.recs[1].kind);
}

TEST(CellFieldGapsRegistry, UpdateCopiesCallerValuesAtSubmit)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 3, 1);
	std::vector<float> v = { 1, 2, 3 };
	ASSERT_TRUE(reg.Update(h, v.data(), 3).ok);
	std::fill(v.begin(), v.end(), 99.0f);
	RecSink sink;
	reg.Drain(sink);
	EXPECT_EQ(std::vector<float>({ 1, 2, 3 }), sink.recs[1].values);
}

TEST(CellFieldGapsRegistry, UpdateHugeCountOnValidHandleIsCountMismatchNotCrash)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle h = Make(reg, 2, 2);
	float v[4] = {};
	for (uint32_t count : { 0u, 5u, 16385u, 0x7FFFFFFFu, 0xFFFFFFFFu })
	{
		CellFieldResult r = reg.Update(h, v, count);
		EXPECT_FALSE(r.ok) << count;
		EXPECT_STREQ(CellFieldRegistry::kErrCountMismatch, r.error);
	}
	EXPECT_EQ(5u, reg.GetStats().rejectedRequests);
	EXPECT_EQ(0u, reg.GetStats().updates);
}

////////////////////////////////////////////////////////////////////////////////
// Queue cap, to the exact byte
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, QueueCapBoundaryExactlyAtCapAcceptedNextByteRejected)
{
	CellFieldRegistry reg;
	const uint64_t cap = CellFieldRegistry::kMaxQueueBytes;
	ASSERT_EQ(8ull * 1024 * 1024, cap);

	// Build the queue to exactly cap-4 bytes: 15 full 128x128 Creates (524,288 B each),
	// a 1x1 A, a 128x127 G, a 3x37 H (+ its 111-float Update), and two more 1x1 Creates.
	Grid big(128, 128);
	for (int i = 0; i < 15; ++i)
	{
		CellFieldLayerHandle t;
		ASSERT_TRUE(reg.CreateLayer(big.desc, CellFieldValueKind::Float, t).ok) << i;
	}
	const CellFieldLayerHandle a = Make(reg, 1, 1);
	ASSERT_TRUE(a.IsValid());
	ASSERT_TRUE(Make(reg, 128, 127).IsValid());
	const CellFieldLayerHandle hh = Make(reg, 3, 37);
	ASSERT_TRUE(hh.IsValid());
	std::vector<float> hv(3u * 37u, 1.0f);
	ASSERT_TRUE(reg.Update(hh, hv.data(), (uint32_t)hv.size()).ok);
	const CellFieldLayerHandle i1 = Make(reg, 1, 1);
	const CellFieldLayerHandle j1 = Make(reg, 1, 1);
	ASSERT_TRUE(i1.IsValid());
	ASSERT_TRUE(j1.IsValid());
	ASSERT_EQ(cap - 4, reg.GetStats().queuedBytes);
	EXPECT_EQ(0u, reg.GetStats().droppedCommands);

	// cap - 4 + 4 == cap: accepted
	const float one = 1.0f;
	EXPECT_TRUE(reg.Update(a, &one, 1).ok);
	EXPECT_EQ(cap, reg.GetStats().queuedBytes);
	EXPECT_EQ(0u, reg.GetStats().droppedCommands);

	// cap + 4: rejected, counted, nothing queued, no slot consumed
	const uint32_t allocated = reg.GetStats().layersAllocated;
	CellFieldResult r = reg.Update(i1, &one, 1);
	EXPECT_FALSE(r.ok);
	EXPECT_STREQ(CellFieldRegistry::kErrQueueFull, r.error);
	EXPECT_EQ(cap, reg.GetStats().queuedBytes);
	EXPECT_EQ(1u, reg.GetStats().droppedCommands);
	CellFieldLayerHandle extra;
	Grid tiny(1, 1);
	EXPECT_FALSE(reg.CreateLayer(tiny.desc, CellFieldValueKind::Float, extra).ok);
	EXPECT_FALSE(extra.IsValid());
	EXPECT_EQ(allocated, reg.GetStats().layersAllocated);
	EXPECT_EQ(2u, reg.GetStats().droppedCommands);

	// Coalescing and Destroy are never rejected at the cap.
	EXPECT_TRUE(reg.Update(a, &one, 1).ok);
	EXPECT_TRUE(reg.Update(hh, hv.data(), (uint32_t)hv.size()).ok);
	EXPECT_EQ(cap, reg.GetStats().queuedBytes);
	reg.Destroy(j1);
	EXPECT_EQ(1u, reg.GetStats().destroys);
	EXPECT_EQ(2u, reg.GetStats().droppedCommands);
	EXPECT_EQ(cap, reg.GetStats().queuedBytes);

	// After a drain the queue accepts again and the first rejected update now succeeds.
	NullSink sink;
	reg.Drain(sink);
	EXPECT_EQ(0u, reg.GetStats().queuedBytes);
	EXPECT_TRUE(reg.Update(i1, &one, 1).ok);
}

TEST(CellFieldGapsRegistry, QueueCapExactFitCreateAcceptedNextCreateRejected)
{
	// Fill to cap-32 with 1x1 mixes, then one more 1x1 Create (32 B) fits exactly.
	CellFieldRegistry reg;
	Grid big(128, 128);
	for (int i = 0; i < 15; ++i)
	{
		CellFieldLayerHandle t;
		ASSERT_TRUE(reg.CreateLayer(big.desc, CellFieldValueKind::Float, t).ok);
	}
	// 524,288 left = 128*128*32: a 128x127 Create (520,192) leaves 4,096 = 128 1x1 Creates, but the
	// layer limit allows 16 more; use a 128x1 (4,096 B) Create to land exactly.
	ASSERT_TRUE(Make(reg, 128, 127).IsValid());
	ASSERT_EQ(CellFieldRegistry::kMaxQueueBytes - 4096, reg.GetStats().queuedBytes);
	Grid one(1, 1);
	CellFieldLayerHandle t;
	Grid row(128, 1);
	ASSERT_TRUE(reg.CreateLayer(row.desc, CellFieldValueKind::Float, t).ok);	// exactly cap
	EXPECT_EQ(CellFieldRegistry::kMaxQueueBytes, reg.GetStats().queuedBytes);
	EXPECT_FALSE(reg.CreateLayer(one.desc, CellFieldValueKind::Float, t).ok);	// cap + 32
	EXPECT_EQ(CellFieldRegistry::kMaxQueueBytes, reg.GetStats().queuedBytes);
}

////////////////////////////////////////////////////////////////////////////////
// Drain: re-entrancy, empty, ordering
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsRegistry, DrainOnEmptyQueueIsNoOp)
{
	CellFieldRegistry reg;
	RecSink sink;
	const CellFieldStats before = reg.GetStats();
	reg.Drain(sink);
	reg.Drain(sink);
	EXPECT_TRUE(sink.recs.empty());
	EXPECT_TRUE(Delta(before, reg.GetStats()).empty());
}

TEST(CellFieldGapsRegistry, SinkMayUpdateInsideOnCreateWithoutDeadlock)
{
	auto reg = std::make_shared<CellFieldRegistry>();
	auto sink = std::make_shared<HookSink>();
	auto updateOk = std::make_shared<std::atomic<int>>(-1);
	auto resolved = std::make_shared<std::atomic<int>>(-1);
	sink->onCreate = [reg, updateOk, resolved](CellFieldLayerHandle h)
	{
		const float v[2] = { 7, 8 };
		updateOk->store(reg->Update(h, v, 2).ok ? 1 : 0);
		resolved->store(reg->ResolveHandle(h) ? 1 : 0);
		reg->GetStats();
	};
	const CellFieldLayerHandle h = Make(*reg, 2, 1);
	const bool finished = FinishesWithin([reg, sink]() { reg->Drain(*sink); }, 5);
	ASSERT_TRUE(finished) << "Drain deadlocked when the sink re-entered the registry";
	EXPECT_EQ(1, updateOk->load());
	EXPECT_EQ(1, resolved->load());
	EXPECT_EQ("C", sink->order);

	// The re-entrant Update was queued for the NEXT drain.
	EXPECT_EQ(1u, reg->GetStats().commandsQueued);
	sink->onCreate = nullptr;
	reg->Drain(*sink);
	EXPECT_EQ("CU", sink->order);
	(void)h;
}

TEST(CellFieldGapsRegistry, SinkMayDestroyInsideOnUpdateAndLayerIsFreedNextDrain)
{
	auto reg = std::make_shared<CellFieldRegistry>();
	auto sink = std::make_shared<HookSink>();
	sink->onUpdate = [reg](CellFieldLayerHandle h) { reg->Destroy(h); };
	const CellFieldLayerHandle h = Make(*reg, 1, 1);
	const float v = 1.0f;
	ASSERT_TRUE(reg->Update(h, &v, 1).ok);
	ASSERT_TRUE(FinishesWithin([reg, sink]() { reg->Drain(*sink); }, 5)) << "deadlock";
	EXPECT_EQ("CU", sink->order);
	EXPECT_FALSE(reg->ResolveHandle(h));
	EXPECT_EQ(1u, reg->GetStats().layersAllocated);	// destroy not drained yet
	reg->Drain(*sink);
	EXPECT_EQ("CUD", sink->order);
	EXPECT_EQ(0u, reg->GetStats().layersAllocated);
}

TEST(CellFieldGapsRegistry, SinkMayCreateInsideOnDestroyAndGetsADifferentSlot)
{
	auto reg = std::make_shared<CellFieldRegistry>();
	auto sink = std::make_shared<HookSink>();
	auto created = std::make_shared<CellFieldLayerHandle>();
	sink->onDestroy = [reg, created](CellFieldLayerHandle)
	{
		Grid g(1, 1);
		reg->CreateLayer(g.desc, CellFieldValueKind::Float, *created);
	};
	const CellFieldLayerHandle h = Make(*reg, 1, 1);
	reg->Destroy(h);
	ASSERT_TRUE(FinishesWithin([reg, sink]() { reg->Drain(*sink); }, 5)) << "deadlock";
	EXPECT_EQ("CD", sink->order);
	ASSERT_TRUE(created->IsValid());
	EXPECT_NE(h.index, created->index);	// the destroyed slot is only freed after OnDestroy returns
	EXPECT_TRUE(reg->ResolveHandle(*created));
	sink->onDestroy = nullptr;
	reg->Drain(*sink);
	EXPECT_EQ("CDC", sink->order);
	EXPECT_EQ(1u, reg->GetStats().layersAllocated);
}

TEST(CellFieldGapsRegistry, SinkMayRegisterColourMapsInsideCallbacks)
{
	auto reg = std::make_shared<CellFieldRegistry>();
	auto sink = std::make_shared<HookSink>();
	sink->onCreate = [reg](CellFieldLayerHandle)
	{
		const uint32_t c = 0xFFu;
		reg->RegisterPalette(StringCRC("inside"), &c, 1, 0);
		CellFieldPaletteData d;
		reg->GetPalette(StringCRC("inside").Value(), d);
	};
	Make(*reg);
	ASSERT_TRUE(FinishesWithin([reg, sink]() { reg->Drain(*sink); }, 5)) << "deadlock";
	CellFieldPaletteData d;
	EXPECT_TRUE(reg->GetPalette(StringCRC("inside").Value(), d));
}

TEST(CellFieldGapsRegistry, DrainInterleavedAcrossLayersKeepsSubmitOrderAndCoalescesInPlace)
{
	CellFieldRegistry reg;
	const CellFieldLayerHandle a = Make(reg, 1, 1);
	const CellFieldLayerHandle b = Make(reg, 1, 1);
	float v1 = 1, v2 = 2, v3 = 3;
	ASSERT_TRUE(reg.Update(b, &v1, 1).ok);
	ASSERT_TRUE(reg.Update(a, &v2, 1).ok);
	ASSERT_TRUE(reg.Update(b, &v3, 1).ok);	// merges into b's earlier slot
	reg.Destroy(a);
	RecSink sink;
	reg.Drain(sink);
	EXPECT_EQ("CCUUD", sink.Order());
	EXPECT_EQ(b.index, sink.recs[2].h.index);
	EXPECT_FLOAT_EQ(3.0f, sink.recs[2].values[0]);
	EXPECT_EQ(a.index, sink.recs[3].h.index);
	EXPECT_FLOAT_EQ(2.0f, sink.recs[3].values[0]);
	EXPECT_EQ(a.index, sink.recs[4].h.index);
}

TEST(CellFieldGapsRegistry, RecreateInRecycledSlotAfterPendingUpdateDeliversOnlyNewValues)
{
	CellFieldRegistry reg;
	NullSink null;
	RecSink sink;
	const CellFieldLayerHandle a = Make(reg, 2, 1);
	float va[2] = { 1, 1 };
	ASSERT_TRUE(reg.Update(a, va, 2).ok);
	reg.Destroy(a);
	reg.Drain(sink);
	EXPECT_EQ("CUD", sink.Order());
	sink.recs.clear();

	const CellFieldLayerHandle b = Make(reg, 2, 1);
	ASSERT_EQ(a.index, b.index);
	float vb[2] = { 9, 9 };
	ASSERT_TRUE(reg.Update(b, vb, 2).ok);
	reg.Drain(sink);
	EXPECT_EQ("CU", sink.Order());
	EXPECT_FLOAT_EQ(9.0f, sink.recs[1].values[0]);
	EXPECT_EQ(b.generation, sink.recs[1].h.generation);
	EXPECT_EQ(0u, reg.GetStats().coalescedCommands);
}

////////////////////////////////////////////////////////////////////////////////
// Colour maps
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsColourMaps, PaletteCountOneAndMaxRoundTrip)
{
	CellFieldRegistry reg;
	uint32_t cols[64];
	for (uint32_t i = 0; i < 64; ++i) { cols[i] = 0x01010101u * (i + 1); }
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("one"), cols, 1, 0xAAu).ok);
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("max"), cols, 64, 0xBBu).ok);
	CellFieldPaletteData d;
	ASSERT_TRUE(reg.GetPalette(StringCRC("one").Value(), d));
	EXPECT_EQ(1u, d.count);
	EXPECT_EQ(cols[0], d.colours[0]);
	EXPECT_EQ(0xAAu, d.fallback);
	ASSERT_TRUE(reg.GetPalette(StringCRC("max").Value(), d));
	EXPECT_EQ(64u, d.count);
	EXPECT_EQ(cols[63], d.colours[63]);
	EXPECT_EQ(0xBBu, d.fallback);
}

TEST(CellFieldGapsColourMaps, PaletteCopiesCallerArray)
{
	CellFieldRegistry reg;
	std::vector<uint32_t> cols = { 1, 2, 3 };
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), cols.data(), 3, 0).ok);
	std::fill(cols.begin(), cols.end(), 0xDEADu);
	cols.clear(); cols.shrink_to_fit();
	CellFieldPaletteData d;
	ASSERT_TRUE(reg.GetPalette(StringCRC("p").Value(), d));
	EXPECT_EQ(1u, d.colours[0]);
	EXPECT_EQ(3u, d.colours[2]);
}

TEST(CellFieldGapsColourMaps, PaletteErrorStringAndPartialReplaceLeavesTail)
{
	CellFieldRegistry reg;
	EXPECT_STREQ("Invalid palette", reg.RegisterPalette(StringCRC("p"), nullptr, 1, 0).error);
	EXPECT_STREQ(CellFieldRegistry::kErrInvalidPalette, reg.RegisterPalette(StringCRC("p"), nullptr, 1, 0).error);
	const uint32_t big[4] = { 1, 2, 3, 4 };
	const uint32_t small[1] = { 9 };
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), big, 4, 0).ok);
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), small, 1, 0).ok);
	CellFieldPaletteData d;
	ASSERT_TRUE(reg.GetPalette(StringCRC("p").Value(), d));
	EXPECT_EQ(1u, d.count);	// count is authoritative; colours beyond it are not meaningful
	EXPECT_EQ(9u, d.colours[0]);
}

TEST(CellFieldGapsColourMaps, RampCountOneAndMaxRoundTrip)
{
	CellFieldRegistry reg;
	CellFieldRampStop s[16];
	for (int i = 0; i < 16; ++i) { s[i].position = i / 15.0f; s[i].rgba8 = 0x10000000u * (uint32_t)(i % 16) + 1u; }
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("one"), s, 1).ok);
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("max"), s, 16).ok);
	CellFieldRampData d;
	ASSERT_TRUE(reg.GetRamp(StringCRC("one").Value(), d));
	EXPECT_EQ(1u, d.count);
	ASSERT_TRUE(reg.GetRamp(StringCRC("max").Value(), d));
	EXPECT_EQ(16u, d.count);
	for (int i = 0; i < 16; ++i)
	{
		EXPECT_EQ(s[i].position, d.stops[i].position);
		EXPECT_EQ(s[i].rgba8, d.stops[i].rgba8);
	}
}

TEST(CellFieldGapsColourMaps, RampPositionEdgeCases)
{
	CellFieldRegistry reg;
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();

	const CellFieldRampStop exactEnds[2] = { { 0.0f, 1 }, { 1.0f, 2 } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("ends"), exactEnds, 2).ok);
	const CellFieldRampStop negZero[2] = { { -0.0f, 1 }, { 1.0f, 2 } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("negzero"), negZero, 2).ok);
	const CellFieldRampStop allEqual[3] = { { 0.5f, 1 }, { 0.5f, 2 }, { 0.5f, 3 } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("alleq"), allEqual, 3).ok);
	const CellFieldRampStop atZero[2] = { { 0.0f, 1 }, { 0.0f, 2 } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("zeros"), atZero, 2).ok);
	const CellFieldRampStop atOne[2] = { { 1.0f, 1 }, { 1.0f, 2 } };
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("ones"), atOne, 2).ok);

	const uint64_t rejectedBefore = reg.GetStats().rejectedRequests;
	const CellFieldRampStop tinyDescending[2] = { { 0.5f, 1 }, { std::nextafter(0.5f, 0.0f), 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("desc"), tinyDescending, 2).ok);
	const CellFieldRampStop justOver[2] = { { 0.0f, 1 }, { std::nextafter(1.0f, 2.0f), 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("over"), justOver, 2).ok);
	const CellFieldRampStop justUnder[2] = { { std::nextafter(0.0f, -1.0f), 1 }, { 1.0f, 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("under"), justUnder, 2).ok);
	const CellFieldRampStop nanFirst[2] = { { nan, 1 }, { 1.0f, 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("nan1"), nanFirst, 2).ok);
	const CellFieldRampStop nanMid[3] = { { 0.0f, 1 }, { nan, 2 }, { 1.0f, 3 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("nan2"), nanMid, 3).ok);
	const CellFieldRampStop nanLast[2] = { { 0.0f, 1 }, { nan, 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("nan3"), nanLast, 2).ok);
	const CellFieldRampStop infStop[2] = { { 0.0f, 1 }, { inf, 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("inf"), infStop, 2).ok);
	const CellFieldRampStop negInf[2] = { { -inf, 1 }, { 0.5f, 2 } };
	EXPECT_FALSE(reg.RegisterRamp(StringCRC("ninf"), negInf, 2).ok);
	EXPECT_EQ(rejectedBefore + 8, reg.GetStats().rejectedRequests);
	EXPECT_STREQ(CellFieldRegistry::kErrInvalidRamp, reg.RegisterRamp(StringCRC("x"), nanFirst, 2).error);
	EXPECT_STREQ("Invalid ramp", CellFieldRegistry::kErrInvalidRamp);
	CellFieldRampData d;
	EXPECT_FALSE(reg.GetRamp(StringCRC("nan2").Value(), d));
}

TEST(CellFieldGapsColourMaps, RampCopiesCallerStops)
{
	CellFieldRegistry reg;
	std::vector<CellFieldRampStop> s = { { 0.0f, 5 }, { 1.0f, 6 } };
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("r"), s.data(), 2).ok);
	s[0].rgba8 = 0; s[1].position = 0.2f;
	CellFieldRampData d;
	ASSERT_TRUE(reg.GetRamp(StringCRC("r").Value(), d));
	EXPECT_EQ(5u, d.stops[0].rgba8);
	EXPECT_FLOAT_EQ(1.0f, d.stops[1].position);
}

TEST(CellFieldGapsColourMaps, TableFullIsPerTableAndCountsEachRejection)
{
	CellFieldRegistry reg;
	const uint32_t col = 1;
	const CellFieldRampStop st = { 0.0f, 1 };
	char name[32];
	for (uint32_t i = 0; i < 127; ++i)
	{
		snprintf(name, sizeof(name), "p%u", i);
		ASSERT_TRUE(reg.RegisterPalette(StringCRC(name), &col, 1, 0).ok);
	}
	EXPECT_TRUE(reg.RegisterPalette(StringCRC("p127"), &col, 1, 0).ok);	// 128th distinct id
	const uint64_t before = reg.GetStats().rejectedRequests;
	CellFieldResult r = reg.RegisterPalette(StringCRC("p128"), &col, 1, 0);	// 129th
	EXPECT_FALSE(r.ok);
	EXPECT_STREQ(CellFieldRegistry::kErrColourMapTableFull, r.error);
	EXPECT_STREQ("Colour map table full", r.error);
	EXPECT_EQ(before + 1, reg.GetStats().rejectedRequests);
	EXPECT_EQ(before + 2, (reg.RegisterPalette(StringCRC("p129"), &col, 1, 0), reg.GetStats().rejectedRequests));
	CellFieldPaletteData d;
	EXPECT_FALSE(reg.GetPalette(StringCRC("p128").Value(), d));

	// The ramp table is independent: it is still empty and accepts.
	EXPECT_TRUE(reg.RegisterRamp(StringCRC("p0"), &st, 1).ok);
	// Re-registering an existing palette when full works and bumps the revision.
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("p3"), &col, 1, 7).ok);
	ASSERT_TRUE(reg.GetPalette(StringCRC("p3").Value(), d));
	EXPECT_EQ(2u, d.revision);
	EXPECT_EQ(7u, d.fallback);
	// An invalid payload at full capacity reports the validation error, not table-full.
	EXPECT_STREQ(CellFieldRegistry::kErrInvalidPalette, reg.RegisterPalette(StringCRC("p200"), nullptr, 1, 0).error);

	for (uint32_t i = 0; i < 127; ++i)
	{
		snprintf(name, sizeof(name), "r%u", i);
		ASSERT_TRUE(reg.RegisterRamp(StringCRC(name), &st, 1).ok);
	}
	// 1 ("p0") + 127 = 128 ramps: the next distinct id is rejected.
	EXPECT_STREQ(CellFieldRegistry::kErrColourMapTableFull, reg.RegisterRamp(StringCRC("rX"), &st, 1).error);
}

TEST(CellFieldGapsColourMaps, SameIdInPaletteAndRampTablesIsIndependent)
{
	CellFieldRegistry reg;
	const uint32_t col = 0x11u;
	const CellFieldRampStop st[2] = { { 0.0f, 0x22u }, { 1.0f, 0x33u } };
	ASSERT_TRUE(reg.RegisterPalette(StringCRC("shared"), &col, 1, 0).ok);
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("shared"), st, 2).ok);
	ASSERT_TRUE(reg.RegisterRamp(StringCRC("shared"), st, 2).ok);
	CellFieldPaletteData p;
	CellFieldRampData r;
	ASSERT_TRUE(reg.GetPalette(StringCRC("shared").Value(), p));
	ASSERT_TRUE(reg.GetRamp(StringCRC("shared").Value(), r));
	EXPECT_EQ(1u, p.revision);
	EXPECT_EQ(2u, r.revision);
	EXPECT_EQ(1u, p.count);
	EXPECT_EQ(2u, r.count);
}

TEST(CellFieldGapsColourMaps, RevisionIsMonotonicAndFailuresDoNotBumpIt)
{
	CellFieldRegistry reg;
	const uint32_t col = 1;
	const CellFieldRampStop st = { 0.0f, 1 };
	uint32_t lastP = 0, lastR = 0;
	for (int i = 0; i < 100; ++i)
	{
		ASSERT_TRUE(reg.RegisterPalette(StringCRC("p"), &col, 1, 0).ok);
		ASSERT_TRUE(reg.RegisterRamp(StringCRC("r"), &st, 1).ok);
		EXPECT_FALSE(reg.RegisterPalette(StringCRC("p"), nullptr, 1, 0).ok);
		EXPECT_FALSE(reg.RegisterPalette(StringCRC("p"), &col, 65, 0).ok);
		EXPECT_FALSE(reg.RegisterRamp(StringCRC("r"), &st, 0).ok);
		CellFieldPaletteData p; CellFieldRampData r;
		ASSERT_TRUE(reg.GetPalette(StringCRC("p").Value(), p));
		ASSERT_TRUE(reg.GetRamp(StringCRC("r").Value(), r));
		EXPECT_EQ(lastP + 1, p.revision);
		EXPECT_EQ(lastR + 1, r.revision);
		lastP = p.revision; lastR = r.revision;
	}
	EXPECT_EQ(100u, lastP);
}

TEST(CellFieldGapsColourMaps, GetUnknownLeavesOutputUntouched)
{
	CellFieldRegistry reg;
	CellFieldPaletteData p;
	p.count = 77;
	CellFieldRampData r;
	r.count = 66;
	EXPECT_FALSE(reg.GetPalette(1, p));
	EXPECT_FALSE(reg.GetRamp(1, r));
	EXPECT_EQ(77u, p.count);
	EXPECT_EQ(66u, r.count);
}

////////////////////////////////////////////////////////////////////////////////
// Bake
////////////////////////////////////////////////////////////////////////////////
TEST(CellFieldGapsBake, FullRangeRampIsExactIdentityPerChannel)
{
	const CellFieldRampStop up[2] = { { 0.0f, 0x00000000u }, { 1.0f, 0xFFFFFFFFu } };
	uint32_t out[256];
	BakeRamp(up, 2, out);
	for (uint32_t i = 0; i < 256; ++i)
	{
		ASSERT_EQ(0x01010101u * i, out[i]) << i;
	}
	const CellFieldRampStop down[2] = { { 0.0f, 0xFFFFFFFFu }, { 1.0f, 0x00000000u } };
	BakeRamp(down, 2, out);
	for (uint32_t i = 0; i < 256; ++i)
	{
		ASSERT_EQ(0x01010101u * (255 - i), out[i]) << i;
	}
}

TEST(CellFieldGapsBake, EqualAdjacentStopsMakeAHardStepWithoutNaN)
{
	const CellFieldRampStop s[2] = { { 0.5f, 0xFF0000FFu }, { 0.5f, 0x0000FFFFu } };
	uint32_t out[256];
	BakeRamp(s, 2, out);
	for (uint32_t i = 0; i < 128; ++i) { ASSERT_EQ(0xFF0000FFu, out[i]) << i; }
	for (uint32_t i = 128; i < 256; ++i) { ASSERT_EQ(0x0000FFFFu, out[i]) << i; }

	// equal pair in the middle of a longer ramp
	const CellFieldRampStop m[4] = { { 0.0f, 0x000000FFu }, { 0.5f, 0x808080FFu }, { 0.5f, 0xFFFFFFFFu }, { 1.0f, 0xFFFFFFFFu } };
	BakeRamp(m, 4, out);
	EXPECT_EQ(0x000000FFu, out[0]);
	EXPECT_EQ(0xFFFFFFFFu, out[255]);
	for (uint32_t i = 0; i < 255; ++i)
	{
		// non-decreasing in the red channel (no overshoot from a zero-width span)
		ASSERT_LE((out[i] >> 24) & 0xFF, (out[i + 1] >> 24) & 0xFF) << i;
	}
}

TEST(CellFieldGapsBake, SingleStopAtEitherEndIsConstant)
{
	uint32_t out[256];
	for (float p : { 0.0f, 1.0f })
	{
		const CellFieldRampStop one[1] = { { p, 0x12345678u } };
		BakeRamp(one, 1, out);
		for (int i = 0; i < 256; ++i) { ASSERT_EQ(0x12345678u, out[i]) << p << " " << i; }
	}
}

TEST(CellFieldGapsBake, SixteenStopRampHitsEveryStopColourWithinOneStep)
{
	CellFieldRampStop s[16];
	for (int i = 0; i < 16; ++i) { s[i].position = i / 15.0f; s[i].rgba8 = ((uint32_t)(i * 17) << 24) | 0xFFu; }
	uint32_t out[256];
	BakeRamp(s, 16, out);
	EXPECT_EQ(s[0].rgba8, out[0]);
	EXPECT_EQ(s[15].rgba8, out[255]);
	for (int i = 0; i < 16; ++i)
	{
		const int texel = (int)std::lround(s[i].position * 255.0f);
		EXPECT_NEAR((int)(s[i].rgba8 >> 24), (int)(out[texel] >> 24), 1) << i;
	}
	for (int i = 0; i < 255; ++i) { ASSERT_LE(out[i] >> 24, out[i + 1] >> 24); }
}

TEST(CellFieldGapsBake, AlphaAndColourChannelsInterpolateIndependently)
{
	const CellFieldRampStop s[2] = { { 0.0f, 0xFF000000u }, { 1.0f, 0x00FF00FFu } };
	uint32_t out[256];
	BakeRamp(s, 2, out);
	EXPECT_EQ(0xFF000000u, out[0]);
	EXPECT_EQ(0x00FF00FFu, out[255]);
	const uint32_t mid = out[128];
	EXPECT_NEAR((int)(mid >> 24), 127, 2);
	EXPECT_NEAR((int)((mid >> 16) & 0xFF), 128, 2);
	EXPECT_EQ(0, (int)((mid >> 8) & 0xFF));
	EXPECT_NEAR((int)(mid & 0xFF), 128, 2);
}

TEST(CellFieldGapsBake, PaletteCountEdgeCases)
{
	uint32_t out[64];
	BakePalette(nullptr, 0, 0x55u, out);	// count 0 never reads the source
	for (int i = 0; i < 64; ++i) { ASSERT_EQ(0x55u, out[i]); }

	std::vector<uint32_t> cols(100);
	for (uint32_t i = 0; i < 100; ++i) { cols[i] = i + 1000; }
	BakePalette(cols.data(), 100, 0x66u, out);	// more than 64 clamps to the 64 texels
	for (uint32_t i = 0; i < 64; ++i) { ASSERT_EQ(i + 1000, out[i]); }

	BakePalette(cols.data(), 1, 0x77u, out);
	EXPECT_EQ(1000u, out[0]);
	for (int i = 1; i < 64; ++i) { ASSERT_EQ(0x77u, out[i]); }
}

////////////////////////////////////////////////////////////////////////////////
// Stats: every event moves exactly the expected counters, by exactly the expected amount
////////////////////////////////////////////////////////////////////////////////
namespace
{
	struct StatRow
	{
		const char* name;
		std::function<void(CellFieldRegistry&, CellFieldLayerHandle&)> setup;
		std::function<void(CellFieldRegistry&, CellFieldLayerHandle&)> act;
		DeltaMap expect;
	};
}

TEST(CellFieldGapsStats, EveryEventMovesExactlyItsCounters)
{
	const float v4[4] = { 1, 2, 3, 4 };
	const uint32_t col = 1;
	const CellFieldRampStop st[2] = { { 0.0f, 1 }, { 1.0f, 2 } };
	auto none = [](CellFieldRegistry&, CellFieldLayerHandle&) {};
	auto make22 = [](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); };
	auto make22Updated = [&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); r.Update(h, v4, 4); };
	auto make22Destroyed = [](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); r.Destroy(h); };

	std::vector<StatRow> rows;
	rows.push_back({ "create", none, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); },
		{ {"layersAllocated",1}, {"layersLive",1}, {"commandsQueued",1}, {"queuedBytes",128}, {"creates",1}, {"lifecycleLogsEmitted",1} } });
	rows.push_back({ "update", make22, [&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Update(h, v4, 4); },
		{ {"commandsQueued",1}, {"queuedBytes",16}, {"updates",1} } });
	rows.push_back({ "update coalesced", make22Updated, [&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Update(h, v4, 4); },
		{ {"updates",1}, {"coalescedCommands",1} } });
	rows.push_back({ "update count mismatch", make22, [&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Update(h, v4, 3); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "update null values", make22, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Update(h, nullptr, 4); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "update stale", make22Destroyed, [&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Update(h, v4, 4); },
		{ {"staleHandles",1} } });
	rows.push_back({ "destroy live", make22, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Destroy(h); },
		{ {"layersLive",-1}, {"commandsQueued",1}, {"destroys",1}, {"lifecycleLogsEmitted",1} } });
	rows.push_back({ "destroy stale", make22Destroyed, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.Destroy(h); },
		{ {"staleHandles",1} } });
	rows.push_back({ "resolve live", make22, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.ResolveHandle(h); }, {} });
	rows.push_back({ "resolve stale", make22Destroyed, [](CellFieldRegistry& r, CellFieldLayerHandle& h) { r.ResolveHandle(h); },
		{ {"staleHandles",1} } });
	rows.push_back({ "create grid too large", none, [](CellFieldRegistry& r, CellFieldLayerHandle&) { Make(r, 129, 1); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "create zero size", none, [](CellFieldRegistry& r, CellFieldLayerHandle&) { Make(r, 0, 1); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "create unsupported topology", none, [](CellFieldRegistry& r, CellFieldLayerHandle&)
		{ Grid g(1, 1); g.desc.topology = StringCRC("zz"); CellFieldLayerHandle o; r.CreateLayer(g.desc, CellFieldValueKind::Float, o); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "create max layers",
		[](CellFieldRegistry& r, CellFieldLayerHandle&) { for (int i = 0; i < 32; ++i) { Make(r); } },
		[](CellFieldRegistry& r, CellFieldLayerHandle&) { Make(r); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "palette ok", none, [&col](CellFieldRegistry& r, CellFieldLayerHandle&) { r.RegisterPalette(StringCRC("p"), &col, 1, 0); },
		{ {"lifecycleLogsEmitted",1} } });
	rows.push_back({ "ramp ok", none, [&st](CellFieldRegistry& r, CellFieldLayerHandle&) { r.RegisterRamp(StringCRC("r"), st, 2); },
		{ {"lifecycleLogsEmitted",1} } });
	rows.push_back({ "palette invalid", none, [&col](CellFieldRegistry& r, CellFieldLayerHandle&) { r.RegisterPalette(StringCRC("p"), &col, 0, 0); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "ramp invalid", none, [&st](CellFieldRegistry& r, CellFieldLayerHandle&) { r.RegisterRamp(StringCRC("r"), st, 0); },
		{ {"rejectedRequests",1} } });
	rows.push_back({ "palette get/ramp get", none, [](CellFieldRegistry& r, CellFieldLayerHandle&)
		{ CellFieldPaletteData p; CellFieldRampData d; r.GetPalette(5, p); r.GetRamp(5, d); }, {} });
	rows.push_back({ "drain create+update+destroy",
		[&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); r.Update(h, v4, 4); r.Destroy(h); },
		[](CellFieldRegistry& r, CellFieldLayerHandle&) { NullSink s; r.Drain(s); },
		{ {"layersAllocated",-1}, {"commandsQueued",-3}, {"queuedBytes",-144}, {"uploads",1}, {"uploadedBytes",16}, {"geometryBuilds",1} } });
	rows.push_back({ "drain two coalesced updates",
		[&v4](CellFieldRegistry& r, CellFieldLayerHandle& h) { h = Make(r, 2, 2); r.Update(h, v4, 4); r.Update(h, v4, 4); },
		[](CellFieldRegistry& r, CellFieldLayerHandle&) { NullSink s; r.Drain(s); },
		{ {"commandsQueued",-2}, {"queuedBytes",-144}, {"uploads",1}, {"uploadedBytes",16}, {"geometryBuilds",1} } });

	for (const StatRow& row : rows)
	{
		CellFieldRegistry reg;
		CellFieldLayerHandle h;
		row.setup(reg, h);
		const CellFieldStats before = reg.GetStats();
		row.act(reg, h);
		const DeltaMap got = Delta(before, reg.GetStats());
		EXPECT_EQ(row.expect, got) << row.name << ": got " << Str(got) << " want " << Str(row.expect);
	}
}

TEST(CellFieldGapsStats, CapDropsCountEachDropButWarnOncePerDrainCycle)
{
	CellFieldRegistry reg;
	Grid big(128, 128);
	for (int i = 0; i < 16; ++i) { CellFieldLayerHandle t; reg.CreateLayer(big.desc, CellFieldValueKind::Float, t); }
	Grid tiny(1, 1);
	CellFieldLayerHandle t;
	CellFieldStats b = reg.GetStats();
	reg.CreateLayer(tiny.desc, CellFieldValueKind::Float, t);
	DeltaMap d = Delta(b, reg.GetStats());
	EXPECT_EQ((DeltaMap{ {"droppedCommands", 1}, {"capWarningsLogged", 1} }), d) << Str(d);
	b = reg.GetStats();
	reg.CreateLayer(tiny.desc, CellFieldValueKind::Float, t);
	d = Delta(b, reg.GetStats());
	EXPECT_EQ((DeltaMap{ {"droppedCommands", 1} }), d) << Str(d);
	NullSink sink;
	reg.Drain(sink);
	b = reg.GetStats();
	for (int i = 0; i < 16; ++i) { CellFieldLayerHandle q; reg.CreateLayer(big.desc, CellFieldValueKind::Float, q); }
	reg.CreateLayer(tiny.desc, CellFieldValueKind::Float, t);
	EXPECT_EQ(2u, reg.GetStats().capWarningsLogged);
}

TEST(CellFieldGapsStats, LifecycleLogBudgetIs16PerDrainCycleAndResets)
{
	CellFieldRegistry reg;
	const uint32_t col = 1;
	char name[16];
	const uint32_t budget = CellFieldRegistry::kLifecycleLogBudget;
	ASSERT_EQ(16u, budget);
	for (uint32_t i = 0; i < budget + 4; ++i)
	{
		snprintf(name, sizeof(name), "p%u", i);
		reg.RegisterPalette(StringCRC(name), &col, 1, 0);
	}
	CellFieldStats s = reg.GetStats();
	EXPECT_EQ(budget, s.lifecycleLogsEmitted);
	EXPECT_EQ(4u, s.lifecycleLogsSuppressed);

	NullSink sink;
	reg.Drain(sink);
	reg.RegisterPalette(StringCRC("after"), &col, 1, 0);
	s = reg.GetStats();
	EXPECT_EQ(budget + 1, s.lifecycleLogsEmitted);
	EXPECT_EQ(4u, s.lifecycleLogsSuppressed);

	// Layer create/destroy draw from the same shared budget.
	CellFieldRegistry reg2;
	for (uint32_t i = 0; i < 10; ++i) { Make(reg2); }
	for (uint32_t i = 0; i < 10; ++i) { reg2.Destroy(Handle((uint16_t)i, 0)); }
	s = reg2.GetStats();
	EXPECT_EQ(budget, s.lifecycleLogsEmitted);
	EXPECT_EQ(4u, s.lifecycleLogsSuppressed);
}

TEST(CellFieldGapsStats, LayerCountsTrackAllocatedVersusLive)
{
	CellFieldRegistry reg;
	NullSink sink;
	CellFieldLayerHandle h[3];
	for (int i = 0; i < 3; ++i) { h[i] = Make(reg); }
	EXPECT_EQ(3u, reg.GetStats().layersAllocated);
	EXPECT_EQ(3u, reg.GetStats().layersLive);
	reg.Destroy(h[1]);
	EXPECT_EQ(3u, reg.GetStats().layersAllocated);
	EXPECT_EQ(2u, reg.GetStats().layersLive);
	reg.Drain(sink);
	EXPECT_EQ(2u, reg.GetStats().layersAllocated);
	EXPECT_EQ(2u, reg.GetStats().layersLive);
}

////////////////////////////////////////////////////////////////////////////////
// Multiple producers, one consumer
////////////////////////////////////////////////////////////////////////////////
namespace
{
	uint32_t KeyOf(CellFieldLayerHandle h) { return ((uint32_t)h.index << 16) | h.generation; }

	struct LifecycleSink : ICellFieldCommandSink
	{
		std::set<uint32_t> live;
		uint64_t creates = 0, destroys = 0, updates = 0, violations = 0;
		void OnCreate(CellFieldLayerHandle h, const CellGridDesc&, CellFieldValueKind) override
		{
			++creates;
			if (!live.insert(KeyOf(h)).second) { ++violations; }
		}
		void OnUpdate(CellFieldLayerHandle h, const float* v, uint32_t c) override
		{
			++updates;
			if (live.find(KeyOf(h)) == live.end()) { ++violations; }
			for (uint32_t i = 1; i < c; ++i) { if (v[i] != v[0]) { ++violations; break; } }
		}
		void OnDestroy(CellFieldLayerHandle h) override
		{
			++destroys;
			if (live.erase(KeyOf(h)) == 0) { ++violations; }
		}
	};

	void RunProducers(uint32_t producerCount, uint32_t iterations)
	{
		CellFieldRegistry reg;
		LifecycleSink sink;
		std::atomic<bool> stop{ false };
		std::atomic<uint64_t> okCreates{ 0 };

		std::thread consumer([&]()
		{
			while (!stop.load()) { reg.Drain(sink); std::this_thread::yield(); }
		});
		std::vector<std::thread> producers;
		for (uint32_t p = 0; p < producerCount; ++p)
		{
			producers.emplace_back([&, p]()
			{
				Grid g(2, 2);
				for (uint32_t i = 0; i < iterations; ++i)
				{
					CellFieldLayerHandle h;
					while (!reg.CreateLayer(g.desc, CellFieldValueKind::Float, h).ok) { std::this_thread::yield(); }
					okCreates.fetch_add(1);
					const float v[4] = { (float)p, (float)p, (float)p, (float)p };
					reg.Update(h, v, 4);
					reg.Update(h, v, 4);
					reg.Destroy(h);
					reg.Destroy(h);	// stale: harmless
				}
			});
		}
		for (auto& t : producers) { t.join(); }
		stop.store(true);
		consumer.join();
		reg.Drain(sink);

		EXPECT_EQ(0u, sink.violations);
		EXPECT_EQ(okCreates.load(), sink.creates);
		EXPECT_EQ(sink.creates, sink.destroys);
		EXPECT_TRUE(sink.live.empty());
		const CellFieldStats s = reg.GetStats();
		EXPECT_EQ(okCreates.load(), s.creates);
		EXPECT_EQ(okCreates.load(), s.destroys);
		EXPECT_EQ(0u, s.layersAllocated);
		EXPECT_EQ(0u, s.layersLive);
		EXPECT_EQ(0u, s.commandsQueued);
		EXPECT_EQ(0u, s.queuedBytes);
		EXPECT_EQ(sink.updates, s.uploads);
	}
}

TEST(CellFieldGapsThreading, TwoProducersCreateUpdateDestroyWhileConsumerDrains)
{
	RunProducers(2, 3000);
}

TEST(CellFieldGapsThreading, FourProducersCreateUpdateDestroyWhileConsumerDrains)
{
	RunProducers(4, 2000);
}

TEST(CellFieldGapsThreading, ConcurrentColourMapRegistrationAndLookup)
{
	CellFieldRegistry reg;
	std::atomic<bool> bad{ false };
	std::vector<std::thread> ts;
	for (int t = 0; t < 4; ++t)
	{
		ts.emplace_back([&, t]()
		{
			char name[16];
			for (int i = 0; i < 400; ++i)
			{
				snprintf(name, sizeof(name), "m%d", (t * 7 + i) % 40);
				const uint32_t cols[2] = { (uint32_t)t, (uint32_t)t };
				reg.RegisterPalette(StringCRC(name), cols, 2, 0);
				CellFieldPaletteData d;
				if (reg.GetPalette(StringCRC(name).Value(), d) && (d.count != 2 || d.colours[0] != d.colours[1])) { bad.store(true); }
			}
		});
	}
	for (auto& t : ts) { t.join(); }
	EXPECT_FALSE(bad.load());
}
