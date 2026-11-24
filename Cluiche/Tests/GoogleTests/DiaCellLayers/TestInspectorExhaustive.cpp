#include <gtest/gtest.h>
#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/SelectionStats.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaAPI/DiaAPI.h>

#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>

// Selection Inspector (Feature 4) exhaustive test pass: boundaries, error paths, determinism, reentrancy, fingerprint change
// detection, detail-provider registry behaviour and 128-layer limits. Complements the per-class files.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();
	const float kInf = std::numeric_limits<float>::infinity();

	// Pure-function fixture: values buffer + selection over a 4x3 grid.
	struct InspectorStatsPure : ::testing::Test
	{
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		CellSelection sel;
		std::vector<float> data;

		void SetUp() override { sel.Reset(&grid); data.assign(12, 0.0f); }
		void Select(std::initializer_list<uint32_t> cells) { for (uint32_t c : cells) { sel.Set(c, true); } }
		void SelectAll() { for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); } }
		LayerValues Values() const
		{
			LayerValues v;
			v.values = data.data();
			v.count = static_cast<uint32_t>(data.size());
			v.hasData = true;
			return v;
		}
	};

	std::string LongText(size_t n) { return std::string(n, 'x'); }

	// Registry-level fixture used by the state / model / json tests.
	struct InspectorExhaustiveTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CellSelection sel;
		InspectorState state;
		std::unique_ptr<LayerReader> mReader;
		std::vector<std::unique_ptr<CountingLayerProvider>> providers;

		void SetUp() override { FakePuId() = 1; sel.Reset(&grid); }

		LayerReader& Reader()
		{
			if (!mReader) { mReader = std::make_unique<LayerReader>(registry); }
			return *mReader;
		}
		CountingLayerProvider& AddLayer(const char* id, std::vector<float> values, const char* group = "G",
			CellValueKind kind = CellValueKind::Float, const char* schemaKey = nullptr)
		{
			providers.push_back(std::make_unique<CountingLayerProvider>());
			CountingLayerProvider& p = *providers.back();
			p.values = values;
			CellLayerDesc d = MakeCountingLayer(id, &grid, p, kind);
			Dia::ValueSchema::SetText(d.group, group);	// truncating: a plain Append asserts when the name overflows String32
			if (schemaKey != nullptr) { d.schemaKey = StringCRC(schemaKey); }
			EXPECT_EQ(CellRegisterResult::Ok, registry.Register(d)) << id;
			return p;
		}
		void SelectAll() { for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); } }
		bool Tick() { return state.Tick(registry, Reader(), service, &sel); }
		void Settle() { for (int i = 0; i < 4; ++i) { Tick(); } }
		bool BuildJson(InspectRequest::Mode mode, Json::Value& out)
		{
			InspectRequest req;
			req.mode = mode;
			return InspectorModel::BuildStatsJson(registry, Reader(), service, state, &sel, req, out);
		}
	};

	const Json::Value* FindJsonLayer(const Json::Value& data, const char* layer)
	{
		for (const Json::Value& l : data["layers"]) { if (l["layer"].asString() == layer) { return &l; } }
		return nullptr;
	}
}

// ---------------------------------------------------------------- SelectionStats: float boundaries

TEST_F(InspectorStatsPure, PositiveInfinityAloneGivesInfiniteMaxAndMean)
{
	data[2] = kInf; data[3] = 1.0f;
	Select({ 2, 3 });
	LayerStats s;
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(1.0f, s.min);
	EXPECT_TRUE(std::isinf(s.max));
	EXPECT_TRUE(std::isinf(s.mean));
}

TEST_F(InspectorStatsPure, OpposingInfinitiesGiveFullRangeAndNaNMean)
{
	data[0] = -kInf; data[1] = kInf;
	Select({ 0, 1 });
	LayerStats s;
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_TRUE(s.hasRange);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(-kInf, s.min);
	EXPECT_EQ(kInf, s.max);
	EXPECT_TRUE(std::isnan(s.mean));	// inf + -inf: documented behaviour pin, count and range stay valid
}

TEST_F(InspectorStatsPure, NaNAndInfinityMixCountsEachSeparately)
{
	data[0] = kNaN; data[1] = kInf; data[2] = kNaN; data[3] = 5.0f;
	Select({ 0, 1, 2, 3 });
	LayerStats s;
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(2u, s.nanCount);
	EXPECT_EQ(5.0f, s.min);
	EXPECT_TRUE(std::isinf(s.max));
}

TEST_F(InspectorStatsPure, HugeFloatsDoNotOverflowTheDoubleMean)
{
	const float big = std::numeric_limits<float>::max();
	data[0] = big; data[1] = big; data[2] = big;
	Select({ 0, 1, 2 });
	LayerStats s;
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_EQ(static_cast<double>(big), s.mean);
	EXPECT_EQ(big, s.max);
}

TEST_F(InspectorStatsPure, DenormalsAreCountedAndKeepAnExactMin)
{
	const float tiny = std::numeric_limits<float>::denorm_min();
	data[0] = tiny; data[1] = 0.0f;
	Select({ 0, 1 });
	LayerStats s;
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(0.0f, s.min);
	EXPECT_EQ(tiny, s.max);
}

TEST_F(InspectorStatsPure, SelectionInsertionOrderDoesNotChangeTheStats)
{
	for (uint32_t i = 0; i < 12; ++i) { data[i] = 0.1f * static_cast<float>(i * i); }
	LayerStats a, b;
	Select({ 11, 3, 7, 0, 5 });
	SelectionStats::ComputeFloat(sel, Values(), a);
	CellSelection other;
	other.Reset(&grid);
	for (uint32_t c : { 0u, 3u, 5u, 7u, 11u }) { other.Set(c, true); }
	SelectionStats::ComputeFloat(other, Values(), b);
	EXPECT_EQ(a.count, b.count);
	EXPECT_EQ(a.min, b.min);
	EXPECT_EQ(a.max, b.max);
	EXPECT_EQ(a.mean, b.mean);	// bit-identical: the accumulation order is by cell index
}

TEST_F(InspectorStatsPure, RepeatedComputeIsDeterministicAndResetsStaleFields)
{
	data[1] = 4.0f; data[2] = 8.0f;
	Select({ 1, 2 });
	LayerStats s;
	s.nanCount = 99; s.min = -1; s.max = 1; s.mean = 7; s.hasRange = true;	// stale content must be overwritten
	SelectionStats::ComputeFloat(sel, Values(), s);
	SelectionStats::ComputeFloat(sel, Values(), s);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_EQ(4.0f, s.min);
	EXPECT_EQ(8.0f, s.max);
	EXPECT_DOUBLE_EQ(6.0, s.mean);
}

// ---------------------------------------------------------------- SelectionStats: category boundaries

TEST_F(InspectorStatsPure, CategoryValuesAtAndBeyondTheEnumEdgeAreOther)
{
	// enum of 3: ids 0..2. 2 is in range; 3 (== count), -1, 1.5 and +/-inf are other; NaN is nanCount.
	data = { 2, 3, -1, 1.5f, kInf, -kInf, kNaN, 0, 0, 0, 0, 0 };
	Select({ 0, 1, 2, 3, 4, 5, 6 });
	LayerStats s;
	SelectionStats::ComputeCategory(sel, Values(), 3, s);
	EXPECT_EQ(1u, s.categoryCounts[2]);
	EXPECT_EQ(5u, s.otherCount);
	EXPECT_EQ(1u, s.nanCount);
	EXPECT_EQ(6u, s.count);
	EXPECT_EQ(s.count, s.categoryCounts[0] + s.categoryCounts[1] + s.categoryCounts[2] + s.otherCount);
}

TEST_F(InspectorStatsPure, CategoryAllNaNCountsNoneAndNoOther)
{
	data.assign(12, kNaN);
	SelectAll();
	LayerStats s;
	SelectionStats::ComputeCategory(sel, Values(), 4, s);
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(0u, s.count);
	EXPECT_EQ(12u, s.nanCount);
	EXPECT_EQ(0u, s.otherCount);
	EXPECT_FALSE(s.hasRange);
}

TEST_F(InspectorStatsPure, CategoryWithZeroEnumValuesPutsEverythingInOther)
{
	data.assign(12, 0.0f);
	SelectAll();
	LayerStats s;
	SelectionStats::ComputeCategory(sel, Values(), 0, s);
	EXPECT_EQ(0u, s.categoryCount);
	EXPECT_EQ(12u, s.otherCount);
	EXPECT_EQ(12u, s.count);
}

TEST_F(InspectorStatsPure, CategoryEnumExactlyAtCapacityIsNotTruncatedButOneMoreIs)
{
	data = { 63, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	Select({ 0, 1 });
	LayerStats exact;
	SelectionStats::ComputeCategory(sel, Values(), LayerStats::kMaxCategories, exact);
	EXPECT_FALSE(exact.categoryTruncated);
	EXPECT_EQ(LayerStats::kMaxCategories, exact.categoryCount);
	EXPECT_EQ(1u, exact.categoryCounts[63]);
	EXPECT_EQ(1u, exact.otherCount);	// 64 is outside the enum
	LayerStats over;
	SelectionStats::ComputeCategory(sel, Values(), LayerStats::kMaxCategories + 1, over);
	EXPECT_TRUE(over.categoryTruncated);
	EXPECT_EQ(1u, over.categoryCounts[63]);
	EXPECT_EQ(1u, over.otherCount);	// 64 is an enum value past the capacity: not itemised, counted as other
}

// ---------------------------------------------------------------- SelectionStats: histogram boundaries

TEST_F(InspectorStatsPure, HistogramRangeEndpointsLandInFirstAndLastBin)
{
	data[0] = 0.0f; data[1] = 16.0f; data[2] = 15.999f; data[3] = 1.0f;
	Select({ 0, 1, 2, 3 });
	Histogram h;
	SelectionStats::ComputeHistogram(sel, Values(), 0.0f, 16.0f, h);
	EXPECT_TRUE(h.valid);
	EXPECT_EQ(1u, h.bins[0]);
	EXPECT_EQ(1u, h.bins[1]);	// exactly on the 1.0 edge goes up
	EXPECT_EQ(2u, h.bins[15]);	// max and just below max
	EXPECT_EQ(0u, h.outOfRangeLow);
	EXPECT_EQ(0u, h.outOfRangeHigh);
	EXPECT_EQ(4u, h.count);
}

TEST_F(InspectorStatsPure, HistogramInfinitiesAreOutOfRangeInTheEndBins)
{
	data[0] = -kInf; data[1] = kInf; data[2] = kNaN; data[3] = 8.0f;
	Select({ 0, 1, 2, 3 });
	Histogram h;
	SelectionStats::ComputeHistogram(sel, Values(), 0.0f, 16.0f, h);
	EXPECT_EQ(1u, h.outOfRangeLow);
	EXPECT_EQ(1u, h.outOfRangeHigh);
	EXPECT_EQ(1u, h.nanCount);
	EXPECT_EQ(3u, h.count);
	EXPECT_EQ(1u, h.bins[0]);
	EXPECT_EQ(1u, h.bins[15]);
	EXPECT_EQ(1u, h.bins[8]);
}

TEST_F(InspectorStatsPure, HistogramBinsAlwaysSumToCountForMixedData)
{
	for (uint32_t i = 0; i < 12; ++i) { data[i] = static_cast<float>(i) * 3.0f - 10.0f; }
	data[4] = kNaN;
	SelectAll();
	Histogram h;
	SelectionStats::ComputeHistogram(sel, Values(), 0.0f, 20.0f, h);
	uint32_t sum = 0;
	for (uint32_t b = 0; b < Histogram::kBins; ++b) { sum += h.bins[b]; }
	EXPECT_EQ(sum, h.count);
	EXPECT_EQ(11u, h.count);
	EXPECT_EQ(1u, h.nanCount);
	EXPECT_GT(h.outOfRangeLow, 0u);
	EXPECT_GT(h.outOfRangeHigh, 0u);
}

TEST_F(InspectorStatsPure, HistogramNonFiniteRangeIsInvalid)
{
	data[0] = 1.0f;
	Select({ 0 });
	Histogram h;
	SelectionStats::ComputeHistogram(sel, Values(), 0.0f, kInf, h);
	EXPECT_FALSE(h.valid);
	EXPECT_EQ(1u, h.bins[0]);
	SelectionStats::ComputeHistogram(sel, Values(), kNaN, 1.0f, h);
	EXPECT_FALSE(h.valid);
	EXPECT_EQ(0u, h.outOfRangeLow + h.outOfRangeHigh);
}

// ---------------------------------------------------------------- Compute through the registry

TEST_F(InspectorExhaustiveTest, ComputeOnACategoryLayerHistogramIsRefused)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	service = &fx.Publish();
	AddLayer("biome", { 0, 1, 2 }, "G", CellValueKind::Category, "cells/biome");
	sel.Set(0, true);
	Histogram h;
	h.count = 77;
	EXPECT_FALSE(SelectionStats::ComputeHistogram(registry, Reader(), service, sel, StringCRC("biome"), h));
	EXPECT_EQ(0u, h.count);
	EXPECT_FALSE(h.hasData);
}

TEST_F(InspectorExhaustiveTest, ComputeAcrossAllRegistryCapacityLayersReadsEachOnce)
{
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		AddLayer(id, std::vector<float>(12, static_cast<float>(i)), i % 2 ? "Odd" : "Even");
	}
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, registry.LayerCountForGrid(&grid));
	SelectAll();
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		LayerStats s;
		ASSERT_TRUE(SelectionStats::Compute(registry, Reader(), service, sel, StringCRC(id), s)) << id;
		EXPECT_EQ(12u, s.count);
		EXPECT_EQ(static_cast<float>(i), s.min);
		EXPECT_EQ(static_cast<double>(i), s.mean);
	}
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, Reader().Reads());
	// A second sweep is served from the reader cache: no further reads.
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		LayerStats s;
		SelectionStats::Compute(registry, Reader(), service, sel, StringCRC(id), s);
	}
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, Reader().Reads());
}

TEST_F(InspectorExhaustiveTest, LongSchemaLabelAndUnitAreTruncatedNotOverrun)
{
	auto e = UploaderSchemaFixture::FloatEntry("long", 0, 10);
	Dia::ValueSchema::SetText(e.mLabel, LongText(400).c_str());
	Dia::ValueSchema::SetText(e.mUnit, LongText(200).c_str());
	fx.Add(e);
	service = &fx.Publish();
	AddLayer("long", { 1, 2 }, "G", CellValueKind::Float, "cells/long");
	sel.Set(0, true);
	LayerStats s;
	ASSERT_TRUE(SelectionStats::Compute(registry, Reader(), service, sel, StringCRC("long"), s));
	EXPECT_LT(strlen(s.label.AsCStr()), 400u);
	EXPECT_LT(strlen(s.unit.AsCStr()), 200u);
	EXPECT_GT(strlen(s.label.AsCStr()), 0u);
	EXPECT_EQ(0, strncmp(s.label.AsCStr(), "xxx", 3));
	state.SetActive(registry, &grid, StringCRC("long"));
	Settle();
	Json::Value out;
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::All, out)) << out.toStyledString();
	const Json::Value* l = FindJsonLayer(out, "long");
	ASSERT_NE(nullptr, l);
	EXPECT_LT((*l)["label"].asString().size(), 400u);
	EXPECT_GT((*l)["label"].asString().size(), 0u);
}

// ---------------------------------------------------------------- fingerprint: change detection and stability

TEST(InspectorFingerprintExhaustive, EverySingleCellHasADistinctNonZeroHash)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(64, 64);
	std::set<uint64_t> seen;
	for (uint32_t i = 0; i < 64u * 64u; ++i)
	{
		const uint64_t h = SelectionFingerprintFn::Mix(i);
		EXPECT_NE(0u, h) << i;
		EXPECT_TRUE(seen.insert(h).second) << "collision at " << i;
	}
}

TEST(InspectorFingerprintExhaustive, OneCellDifferenceChangesHashAndFingerprint)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(8, 8);
	CellSelection a, b;
	a.Reset(&grid); b.Reset(&grid);
	for (uint32_t c : { 1u, 5u, 9u, 30u }) { a.Set(c, true); b.Set(c, true); }
	const SelectionFingerprint fa = SelectionFingerprintFn::Compute(a);
	b.Set(31, true);
	const SelectionFingerprint fb = SelectionFingerprintFn::Compute(b);
	EXPECT_NE(fa.mHash, fb.mHash);
	EXPECT_NE(fa.mCount, fb.mCount);
	EXPECT_TRUE(fa != fb);
	// same count, one cell swapped: the hash must still differ
	CellSelection c2;
	c2.Reset(&grid);
	for (uint32_t c : { 1u, 5u, 9u, 31u }) { c2.Set(c, true); }
	const SelectionFingerprint fc = SelectionFingerprintFn::Compute(c2);
	EXPECT_EQ(fa.mCount, fc.mCount);
	EXPECT_NE(fa.mHash, fc.mHash);
}

TEST(InspectorFingerprintExhaustive, EqualSetsOnTwoSelectionsHashEqualRegardlessOfEditHistory)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(8, 8);
	CellSelection a, b;
	a.Reset(&grid); b.Reset(&grid);
	for (uint32_t c : { 3u, 10u, 40u }) { a.Set(c, true); }
	for (uint32_t c : { 40u, 7u, 10u, 3u }) { b.Set(c, true); }
	b.Set(7, false);
	const SelectionFingerprint fa = SelectionFingerprintFn::Compute(a);
	const SelectionFingerprint fb = SelectionFingerprintFn::Compute(b);
	EXPECT_EQ(fa.mHash, fb.mHash);
	EXPECT_EQ(fa.mCount, fb.mCount);
	EXPECT_NE(fa.mVersion, fb.mVersion);	// version tracks edits, not content
	EXPECT_EQ(fa.mHash, SelectionFingerprintFn::Compute(a).mHash);	// stable on repeat
}

TEST(InspectorFingerprintExhaustive, ResetToEmptyChangesTheFingerprintAndTheCacheFollows)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(8, 8);
	CellSelection sel;
	sel.Reset(&grid);
	sel.Set(9, true);
	SelectionFingerprintCache cache;
	const SelectionFingerprint before = cache.Get(sel);
	EXPECT_EQ(1u, before.mCount);
	EXPECT_NE(0u, before.mHash);
	sel.Clear();
	const SelectionFingerprint cleared = cache.Get(sel);
	EXPECT_EQ(0u, cleared.mCount);
	EXPECT_EQ(0u, cleared.mHash);
	cache.Invalidate();
	EXPECT_EQ(cleared, cache.Get(sel));
}

TEST(InspectorFingerprintExhaustive, ToHexOfExtremesIsExact)
{
	char buf[SelectionFingerprintFn::kHexLength + 1];
	SelectionFingerprintFn::ToHex(0xFFFFFFFFFFFFFFFFull, buf);
	EXPECT_STREQ("ffffffffffffffff", buf);
	SelectionFingerprintFn::ToHex(0x8000000000000001ull, buf);
	EXPECT_STREQ("8000000000000001", buf);
}

// ---------------------------------------------------------------- CellDetailRegistry

namespace
{
	struct InspectorDetailExhaustive : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc gridA = MakeGrid(4, 3);
		Dia::Graphics::CellGridDesc gridB = MakeGrid(2, 2);
		CellDetailRegistry reg{ &FakePuIdFn };
		CellDetailResult out;

		void SetUp() override
		{
			FakePuId() = 1;
			fx.Add(UploaderSchemaFixture::FloatEntry("temp", -50, 50));
			service = &fx.Publish();
		}
		void TearDown() override { FakePuId() = 1; }
	};

	// A provider that mutates the registry from inside its own callback.
	struct SelfUnregisteringProvider
	{
		CellDetailRegistry* reg = nullptr;
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		uint32_t calls = 0;
		void Provide(uint32_t, CellDetailBuilder& b)
		{
			++calls;
			b.Row("self", "x");
			reg->Unregister(grid, "Self");
		}
	};

	struct RegisteringProvider
	{
		CellDetailRegistry* reg = nullptr;
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		StubDetailProvider* added = nullptr;
		uint32_t calls = 0;
		CellDetailRegisterResult result = CellDetailRegisterResult::Ok;
		void Provide(uint32_t, CellDetailBuilder& b)
		{
			++calls;
			b.Row("outer", "x");
			if (calls == 1) { result = reg->Register(grid, "Late", added->Delegate()); }
		}
	};

	struct NestedBuildProvider
	{
		CellDetailRegistry* reg = nullptr;
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		const Dia::ValueSchema::ValueSchemaService* schema = nullptr;
		CellDetailResult inner;
		uint32_t innerCalled = 0;
		uint32_t depth = 0;
		void Provide(uint32_t cell, CellDetailBuilder& b)
		{
			b.Row("outer", "o");
			if (depth++ == 0) { innerCalled = reg->Build(grid, cell + 1, schema, inner); }
			--depth;
		}
	};
}

// Provider names are stored truncated to a String32 (31 chars). Register and Unregister compare in that same truncated domain, so a
// longer name can be unregistered and a repeat registration is a duplicate (names sharing their first 31 chars count as the same).
TEST_F(InspectorDetailExhaustive, ProviderNameLongerThanTheStoredCapacityCanStillBeUnregistered)
{
	// Name storage is a String32 (31 chars). A 40-char name must round-trip through Register / Unregister.
	const std::string name = LongText(40);
	StubDetailProvider p;
	ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, name.c_str(), p.Delegate()));
	EXPECT_TRUE(reg.Unregister(&gridA, name.c_str()));
	EXPECT_EQ(0u, reg.ProviderCount());
}

TEST_F(InspectorDetailExhaustive, SameLongProviderNameTwiceIsADuplicate)
{
	const std::string name = LongText(40);
	StubDetailProvider p;
	ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, name.c_str(), p.Delegate()));
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(&gridA, name.c_str(), p.Delegate()));
	EXPECT_EQ(1u, reg.ProviderCount());
}

TEST_F(InspectorDetailExhaustive, LongProviderNameSectionTitleIsTruncatedSafely)
{
	StubDetailProvider p; p.rowsToEmit = 1;
	const std::string name = LongText(200);
	reg.Register(&gridA, name.c_str(), p.Delegate());
	reg.Build(&gridA, 0, service, out);
	ASSERT_EQ(2u, out.Count());
	EXPECT_EQ(CellDetailRowKind::Section, out.At(0).kind);
	EXPECT_LT(strlen(out.At(0).label.AsCStr()), 200u);
	EXPECT_GT(strlen(out.At(0).label.AsCStr()), 0u);
}

TEST_F(InspectorDetailExhaustive, UnregisterAndUnregisterAllFromAnotherPuAreRejectedAndKeepTheProviders)
{
	LogCapture log;
	StubDetailProvider p;
	reg.Register(&gridA, "Eco", p.Delegate());
	FakePuId() = 2;
	EXPECT_FALSE(reg.Unregister(&gridA, "Eco"));
	EXPECT_EQ(0u, reg.UnregisterAll(&gridA));
	FakePuId() = 1;
	EXPECT_EQ(1u, reg.ProviderCount());
	EXPECT_EQ(2u, log.Count(Dia::Observation::Log::LogLevel::kError, "rejected"));
	EXPECT_TRUE(reg.Unregister(&gridA, "Eco"));
}

TEST_F(InspectorDetailExhaustive, InvalidRegistrationArgumentsAreRejectedWithAnError)
{
	LogCapture log;
	StubDetailProvider p;
	EXPECT_NE(CellDetailRegisterResult::Ok, reg.Register(nullptr, "Eco", p.Delegate()));
	EXPECT_NE(CellDetailRegisterResult::Ok, reg.Register(&gridA, nullptr, p.Delegate()));
	EXPECT_NE(CellDetailRegisterResult::Ok, reg.Register(&gridA, "", p.Delegate()));
	EXPECT_NE(CellDetailRegisterResult::Ok, reg.Register(&gridA, "Eco", CellDetailProviderFn()));
	EXPECT_EQ(0u, reg.ProviderCount());
	EXPECT_EQ(4u, log.Count(Dia::Observation::Log::LogLevel::kError));
	EXPECT_FALSE(reg.Unregister(&gridA, nullptr));
}

TEST_F(InspectorDetailExhaustive, BuildForAGridWithoutProvidersClearsTheResultAndCallsNothing)
{
	StubDetailProvider p; p.rowsToEmit = 3;
	reg.Register(&gridA, "A", p.Delegate());
	EXPECT_EQ(1u, reg.Build(&gridA, 0, service, out));
	ASSERT_GT(out.Count(), 0u);
	EXPECT_EQ(0u, reg.Build(&gridB, 0, service, out));
	EXPECT_EQ(0u, out.Count());
	EXPECT_FALSE(out.Truncated());
	EXPECT_EQ(0u, reg.Build(nullptr, 0, service, out));
	EXPECT_EQ(1u, p.calls);
}

TEST_F(InspectorDetailExhaustive, TotalResultIsCappedAtMaxRowsAcrossProviders)
{
	std::vector<std::unique_ptr<StubDetailProvider>> ps;
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i)
	{
		ps.push_back(std::make_unique<StubDetailProvider>());
		ps.back()->rowsToEmit = CellDetailBuilder::kMaxRowsPerProvider;
		char n[16]; snprintf(n, sizeof(n), "p%u", i);
		ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, n, ps.back()->Delegate()));
	}
	EXPECT_EQ(CellDetailRegistry::kMaxProviders, reg.Build(&gridA, 0, service, out));	// all called even once the result is full
	EXPECT_EQ(CellDetailResult::kMaxRows, out.Count());
	EXPECT_TRUE(out.Truncated());
	for (auto& p : ps) { EXPECT_EQ(1u, p->calls); }
}

TEST_F(InspectorDetailExhaustive, SectionRowsCountTowardTheProviderCap)
{
	CellDetailResult r;
	CellDetailBuilder b(r, service);
	for (int i = 0; i < 70; ++i) { b.Section("s"); }
	EXPECT_EQ(CellDetailBuilder::kMaxRowsPerProvider, b.Added());
	EXPECT_EQ(6u, b.Dropped());
	b.RowValue(StringCRC("cells/temp"), 1.0f);
	EXPECT_EQ(7u, b.Dropped());
}

TEST_F(InspectorDetailExhaustive, BuilderToleratesNullTextAndKeepsNonFiniteValues)
{
	CellDetailResult r;
	CellDetailBuilder b(r, nullptr);	// no schema
	b.Section(nullptr);
	b.Row(nullptr, nullptr);
	b.RowValue(StringCRC("cells/temp"), kNaN);
	b.RowValue(StringCRC("cells/temp"), kInf);
	ASSERT_EQ(4u, r.Count());
	EXPECT_STREQ("", r.At(0).label.AsCStr());
	EXPECT_STREQ("", r.At(1).label.AsCStr());
	EXPECT_STREQ("", r.At(1).text.AsCStr());
	EXPECT_FALSE(r.At(2).resolved);
	EXPECT_TRUE(std::isnan(r.At(2).value));
	EXPECT_TRUE(std::isinf(r.At(3).value));
	EXPECT_STREQ("", r.At(2).label.AsCStr());
}

TEST_F(InspectorDetailExhaustive, LongRowTextIsTruncatedWithoutOverrun)
{
	CellDetailResult r;
	CellDetailBuilder b(r, service);
	b.Row(LongText(500).c_str(), LongText(500).c_str());
	ASSERT_EQ(1u, r.Count());
	EXPECT_LT(strlen(r.At(0).label.AsCStr()), 500u);
	EXPECT_LT(strlen(r.At(0).text.AsCStr()), 500u);
	EXPECT_GT(strlen(r.At(0).text.AsCStr()), 0u);
}

TEST_F(InspectorDetailExhaustive, UnregisterKeepsTheOrderOfTheRestAndReRegisterGoesLast)
{
	StubDetailProvider a, b, c;
	a.sectionTitle = "A"; b.sectionTitle = "B"; c.sectionTitle = "C";
	reg.Register(&gridA, "A", a.Delegate());
	reg.Register(&gridA, "B", b.Delegate());
	reg.Register(&gridA, "C", c.Delegate());
	ASSERT_TRUE(reg.Unregister(&gridA, "A"));
	reg.Register(&gridA, "A", a.Delegate());
	reg.Build(&gridA, 0, service, out);
	// per provider: its provider Section row, then the provider's own Section(title) row
	ASSERT_EQ(6u, out.Count());
	EXPECT_STREQ("B", out.At(0).label.AsCStr());
	EXPECT_STREQ("C", out.At(2).label.AsCStr());
	EXPECT_STREQ("A", out.At(4).label.AsCStr());
}

TEST_F(InspectorDetailExhaustive, ReRegisteringAnOverflowingProviderWarnsAgain)
{
	LogCapture log;
	StubDetailProvider big; big.rowsToEmit = 100;
	reg.Register(&gridA, "Big", big.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(1u, log.Warnings());
	reg.Unregister(&gridA, "Big");
	reg.Register(&gridA, "Big", big.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(2u, log.Warnings());
}

TEST_F(InspectorDetailExhaustive, UnregisterAllIsPerGridAndKeepsOtherGridsInOrder)
{
	StubDetailProvider a, b, c;
	reg.Register(&gridA, "A", a.Delegate());
	reg.Register(&gridB, "B", b.Delegate());
	reg.Register(&gridA, "A2", c.Delegate());
	EXPECT_EQ(2u, reg.UnregisterAll(&gridA));
	EXPECT_EQ(0u, reg.UnregisterAll(&gridA));
	EXPECT_EQ(1u, reg.ProviderCount());
	EXPECT_EQ(0u, reg.ProviderCountForGrid(&gridA));
	EXPECT_EQ(1u, reg.ProviderCountForGrid(&gridB));
	EXPECT_EQ(1u, reg.Build(&gridB, 0, service, out));
	EXPECT_EQ(1u, b.calls);
}

TEST_F(InspectorDetailExhaustive, ProviderMayUnregisterItselfDuringTheCallAndOthersStillRun)
{
	SelfUnregisteringProvider self; self.reg = &reg; self.grid = &gridA;
	StubDetailProvider other; other.rowsToEmit = 1;
	reg.Register(&gridA, "Self", fastdelegate::MakeDelegate(&self, &SelfUnregisteringProvider::Provide));
	reg.Register(&gridA, "Other", other.Delegate());
	EXPECT_EQ(2u, reg.Build(&gridA, 0, service, out));
	EXPECT_EQ(1u, self.calls);
	EXPECT_EQ(1u, other.calls);
	EXPECT_EQ(1u, reg.ProviderCount());
	EXPECT_EQ(1u, reg.Build(&gridA, 0, service, out));
	EXPECT_EQ(1u, self.calls);
}

TEST_F(InspectorDetailExhaustive, ProviderRegisteredDuringBuildRunsFromTheNextBuild)
{
	StubDetailProvider added; added.rowsToEmit = 1;
	RegisteringProvider outer; outer.reg = &reg; outer.grid = &gridA; outer.added = &added;
	reg.Register(&gridA, "Outer", fastdelegate::MakeDelegate(&outer, &RegisteringProvider::Provide));
	EXPECT_EQ(1u, reg.Build(&gridA, 0, service, out));
	EXPECT_EQ(CellDetailRegisterResult::Ok, outer.result);
	EXPECT_EQ(0u, added.calls);
	EXPECT_EQ(2u, reg.Build(&gridA, 0, service, out));
	EXPECT_EQ(1u, added.calls);
}

TEST_F(InspectorDetailExhaustive, NestedBuildInsideAProviderLeavesTheOuterResultIntact)
{
	NestedBuildProvider p; p.reg = &reg; p.grid = &gridA; p.schema = service;
	reg.Register(&gridA, "Nest", fastdelegate::MakeDelegate(&p, &NestedBuildProvider::Provide));
	EXPECT_EQ(1u, reg.Build(&gridA, 4, service, out));
	EXPECT_EQ(1u, p.innerCalled);
	ASSERT_EQ(2u, out.Count());
	EXPECT_STREQ("Nest", out.At(0).label.AsCStr());
	EXPECT_STREQ("outer", out.At(1).label.AsCStr());
	ASSERT_EQ(2u, p.inner.Count());
}

// ---------------------------------------------------------------- InspectorState: limits and lifecycle

TEST_F(InspectorExhaustiveTest, HundredTwentyEightLayersCapTheShownSetAtMaxShownBaseFirst)
{
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		AddLayer(id, std::vector<float>(12, static_cast<float>(i)), "G");
	}
	state.SetActive(registry, &grid, StringCRC("l100"));
	SelectAll();
	Settle();
	EXPECT_EQ(InspectorState::kMaxShown, state.ShownLayerCount());
	EXPECT_EQ(StringCRC("l100"), state.ShownLayerAt(0));
	EXPECT_EQ(StringCRC("l0"), state.ShownLayerAt(1));
	EXPECT_EQ(InspectorState::kMaxShown, state.LastPassLayerCount());
	const LayerStats* base = state.StatsFor(StringCRC("l100"));
	ASSERT_NE(nullptr, base);
	EXPECT_EQ(100.0, base->mean);
	EXPECT_EQ(nullptr, state.StatsFor(StringCRC("l127")));
	uint32_t reads = 0;
	for (auto& p : providers) { reads += p->readCalls; }
	EXPECT_EQ(InspectorState::kMaxShown, reads);	// the other 96 layers were never read
}

TEST_F(InspectorExhaustiveTest, FreezeCoversAllHundredTwentyEightLayersAndTwiceDoesNotDoubleThem)
{
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		AddLayer(id, std::vector<float>(12, static_cast<float>(i)), i % 4 == 0 ? "A" : "B");
	}
	state.SetActive(registry, &grid, StringCRC("l0"));
	SelectAll();
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), service, &grid, &sel));
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, state.FrozenLayerCount());
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("l127")));
	EXPECT_EQ(127.0, state.StatsForA(StringCRC("l127"))->mean);
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), service, &grid, &sel));
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, state.FrozenLayerCount());
	state.Unpin();
	EXPECT_EQ(0u, state.FrozenLayerCount());
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("l0")));
}

TEST_F(InspectorExhaustiveTest, StatsJsonAllWithHundredTwentyEightLayersReportsEachOnce)
{
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxLayers; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "l%u", i);
		AddLayer(id, std::vector<float>(12, static_cast<float>(i)), "G");
	}
	state.SetActive(registry, &grid, StringCRC("l0"));
	SelectAll();
	Settle();
	Json::Value out;
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::All, out)) << out.toStyledString();
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, out["layers"].size());
	std::set<std::string> ids;
	for (const Json::Value& l : out["layers"]) { ids.insert(l["layer"].asString()); }
	EXPECT_EQ(CellLayerRegistry::kMaxLayers, ids.size());
	InspectRequest req;
	req.mode = InspectRequest::Mode::Layers;
	for (uint32_t i = 0; i < InspectRequest::kMaxLayers; ++i) { EXPECT_TRUE(req.AddLayer(StringCRC("l5"))); }
	EXPECT_FALSE(req.AddLayer(StringCRC("l5")));	// request capacity is the registry capacity
}

TEST_F(InspectorExhaustiveTest, LongGroupNamesStillShowTheBaseGroupAndCanBeCollapsed)
{
	const std::string longGroup = LongText(40);
	AddLayer("a", { 1, 2, 3 }, longGroup.c_str());
	AddLayer("b", { 4, 5, 6 }, longGroup.c_str());
	AddLayer("c", { 7, 8, 9 }, "Other");
	state.SetActive(registry, &grid, StringCRC("a"));
	sel.Set(0, true);
	Settle();
	EXPECT_EQ(2u, state.ShownLayerCount());	// a and b share the (truncated) group
	EXPECT_NE(nullptr, state.StatsFor(StringCRC("b")));
	EXPECT_EQ(nullptr, state.StatsFor(StringCRC("c")));
}

TEST_F(InspectorExhaustiveTest, ExpandedSetEmptyGroupNameIsIgnored)
{
	AddLayer("a", { 1, 2, 3 }, "G");
	state.SetActive(registry, &grid, StringCRC("a"));
	sel.Set(0, true);
	Settle();
	const uint32_t groups = state.ExpandedGroupCount();
	state.SetGroupExpanded("", true);
	state.SetGroupExpanded(nullptr, true);
	Settle();
	EXPECT_EQ(1u, state.ShownLayerCount());
	EXPECT_LE(state.ExpandedGroupCount(), groups + 1);
	EXPECT_TRUE(state.StatsFor(StringCRC("a")) != nullptr);
}

TEST_F(InspectorExhaustiveTest, UnregisteringAShownLayerWhilePinnedLiveDropsItsAAndKeepsThePin)
{
	AddLayer("a", { 1, 2, 3, 4 }, "G");
	AddLayer("b", { 5, 6, 7, 8 }, "G");
	state.SetActive(registry, &grid, StringCRC("a"));
	sel.Set(0, true); sel.Set(1, true);
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	Settle();
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("b")));
	ASSERT_TRUE(registry.Unregister(StringCRC("b")));
	Settle();
	EXPECT_TRUE(state.IsPinned());
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("b")));
	EXPECT_EQ(nullptr, state.StatsFor(StringCRC("b")));
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_EQ(2u, state.StatsForA(StringCRC("a"))->count);
}

TEST_F(InspectorExhaustiveTest, UnregisteringALayerAfterFreezeKeepsItsFrozenStatsAndCardJsonSurvives)
{
	AddLayer("a", { 1, 2, 3, 4 }, "G");
	AddLayer("b", { 5, 6, 7, 8 }, "G");
	state.SetActive(registry, &grid, StringCRC("a"));
	sel.Set(0, true); sel.Set(1, true);
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), service, &grid, &sel));
	ASSERT_TRUE(registry.Unregister(StringCRC("b")));
	Settle();
	EXPECT_TRUE(state.IsFrozen());
	EXPECT_TRUE(state.FrozenAvailable(StringCRC("b")));
	Json::Value card;
	InspectorModel::BuildCardJson(registry, Reader(), service, state, &sel, card);
	EXPECT_TRUE(card["hasSelection"].asBool());
	bool sawB = false;
	for (const Json::Value& l : card["layers"]) { sawB = sawB || l["id"].asString() == "b"; }
	EXPECT_FALSE(sawB);
}

TEST_F(InspectorExhaustiveTest, ReRegisteringALayerWithTheSameIdAfterFreezeIsStillFrozen)
{
	AddLayer("a", { 1, 2, 3, 4 }, "G");
	state.SetActive(registry, &grid, StringCRC("a"));
	sel.Set(0, true);
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), service, &grid, &sel));
	ASSERT_TRUE(registry.Unregister(StringCRC("a")));
	CountingLayerProvider replacement;
	replacement.values = { 100, 200, 300, 400 };
	CellLayerDesc d = MakeCountingLayer("a", &grid, replacement);
	ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
	Settle();
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_EQ(1.0, state.StatsForA(StringCRC("a"))->mean);	// frozen values are not recomputed
}

TEST_F(InspectorExhaustiveTest, SelectionWithAllNaNLayerAndInfLayerGivesConsistentJson)
{
	AddLayer("nan", std::vector<float>(12, kNaN), "G");
	AddLayer("inf", std::vector<float>(12, kInf), "G");
	AddLayer("ok", std::vector<float>(12, 2.0f), "G");
	state.SetActive(registry, &grid, StringCRC("ok"));
	SelectAll();
	Settle();
	Json::Value out;
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::All, out)) << out.toStyledString();
	const Json::Value* n = FindJsonLayer(out, "nan");
	ASSERT_NE(nullptr, n);
	EXPECT_TRUE((*n)["hasData"].asBool());
	EXPECT_EQ(0u, (*n)["count"].asUInt());
	EXPECT_EQ(12u, (*n)["nanCount"].asUInt());
	EXPECT_FALSE(n->isMember("min"));
	EXPECT_FALSE(n->isMember("mean"));
	EXPECT_FALSE(n->isMember("max"));
	const Json::Value* i = FindJsonLayer(out, "inf");
	ASSERT_NE(nullptr, i);
	EXPECT_EQ(12u, (*i)["count"].asUInt());
	EXPECT_TRUE(i->isMember("min"));
	EXPECT_TRUE(std::isinf((*i)["max"].asDouble()));
	const Json::Value* ok = FindJsonLayer(out, "ok");
	ASSERT_NE(nullptr, ok);
	EXPECT_EQ(2.0, (*ok)["mean"].asDouble());
}

TEST_F(InspectorExhaustiveTest, LayerWithNoDataReportsHasDataFalseAndNoStats)
{
	AddLayer("empty", {}, "G");	// provider fills zeros; replaced below by a null-value layer
	CellLayerDesc d = MakeLayer("nodata", &grid);
	d.group.Clear(); d.group.Append("G");
	d.read = fastdelegate::FastDelegate2<float*, uint32_t>();	// no read delegate: the reader reports no data
	registry.Unregister(StringCRC("empty"));
	const CellRegisterResult r = registry.Register(d);
	if (r != CellRegisterResult::Ok) { GTEST_SKIP() << "registry refuses a layer without a read delegate"; }
	state.SetActive(registry, &grid, StringCRC("nodata"));
	sel.Set(0, true);
	Settle();
	Json::Value out;
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::All, out));
	const Json::Value* l = FindJsonLayer(out, "nodata");
	ASSERT_NE(nullptr, l);
	EXPECT_FALSE((*l)["hasData"].asBool());
	EXPECT_FALSE(l->isMember("mean"));
}

TEST_F(InspectorExhaustiveTest, SingleCellRowForNonFiniteAndOutOfEnumValues)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 2));
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
	service = &fx.Publish();
	AddLayer("temp", { kNaN, kInf, -kInf }, "G", CellValueKind::Float, "cells/temp");
	AddLayer("biome", { -1, 1.5f, 1 }, "G", CellValueKind::Category, "cells/biome");
	state.SetActive(registry, &grid, StringCRC("temp"));
	SingleCellTable table;
	for (uint32_t cell = 0; cell < 3; ++cell)
	{
		sel.Clear();
		sel.Set(cell, true);
		Settle();
		ASSERT_TRUE(InspectorModel::BuildSingleCell(registry, Reader(), service, state, &sel, true, table));
		const SingleCellRow* temp = nullptr; const SingleCellRow* biome = nullptr;
		for (uint32_t i = 0; i < table.RowCount(); ++i)
		{
			if (table.Row(i).layerId == StringCRC("temp")) { temp = &table.Row(i); }
			if (table.Row(i).layerId == StringCRC("biome")) { biome = &table.Row(i); }
		}
		ASSERT_NE(nullptr, temp); ASSERT_NE(nullptr, biome);
		EXPECT_EQ(cell == 0, temp->isNaN);
		EXPECT_TRUE(temp->hasData);
		EXPECT_EQ(cell != 2, biome->otherValue);	// -1 and 1.5 are outside the enum, 1 is v1
		if (cell == 2) { EXPECT_STREQ("v1", biome->enumId.AsCStr()); }
		else { EXPECT_STREQ("", biome->enumId.AsCStr()); }
	}
}

TEST_F(InspectorExhaustiveTest, TwoIndependentStatesFedTheSameInputsGiveIdenticalJson)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 100));
	service = &fx.Publish();
	std::vector<float> v;
	for (int i = 0; i < 12; ++i) { v.push_back(static_cast<float>((i * 37) % 11) * 1.37f); }
	AddLayer("temp", v, "G", CellValueKind::Float, "cells/temp");
	AddLayer("rain", v, "G");
	sel.Set(2, true); sel.Set(3, true); sel.Set(9, true);
	state.SetActive(registry, &grid, StringCRC("temp"));
	Settle();
	InspectorState second;
	LayerReader reader2(registry);
	second.SetActive(registry, &grid, StringCRC("temp"));
	for (int i = 0; i < 4; ++i) { second.Tick(registry, reader2, service, &sel); }
	InspectRequest req;
	Json::Value a, b, ca, cb;
	ASSERT_TRUE(InspectorModel::BuildStatsJson(registry, Reader(), service, state, &sel, req, a));
	ASSERT_TRUE(InspectorModel::BuildStatsJson(registry, reader2, service, second, &sel, req, b));
	EXPECT_EQ(a.toStyledString(), b.toStyledString());
	InspectorModel::BuildCardJson(registry, Reader(), service, state, &sel, ca);
	InspectorModel::BuildCardJson(registry, reader2, service, second, &sel, cb);
	EXPECT_EQ(ca.toStyledString(), cb.toStyledString());
}

TEST_F(InspectorExhaustiveTest, WholeGridSelectionIsOneCellMoreThanAnyBoxAndCountsEveryCell)
{
	AddLayer("a", { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, "G");
	state.SetActive(registry, &grid, StringCRC("a"));
	SelectAll();
	Settle();
	Json::Value out;
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::Shown, out));
	EXPECT_EQ(12u, out["selected"].asUInt());
	EXPECT_EQ(12u, out["layers"][0u]["count"].asUInt());
	EXPECT_EQ(6.5, out["layers"][0u]["mean"].asDouble());
	EXPECT_FALSE(out.isMember("cell"));	// not a single-cell selection
	sel.Clear();
	sel.Set(11, true);
	Settle();
	ASSERT_TRUE(BuildJson(InspectRequest::Mode::Shown, out));
	EXPECT_TRUE(out.isMember("cell"));
	EXPECT_EQ(11u, out["cell"]["index"].asUInt());
	EXPECT_EQ(3u, out["cell"]["q"].asUInt());
	EXPECT_EQ(2u, out["cell"]["r"].asUInt());
}

TEST(InspectorDeltaExhaustive, DeltaIsNegativeForCountAndInvalidWithoutBothRanges)
{
	LayerStats b, a;
	b.count = 0; a.count = 5; a.hasRange = true; a.min = 1; a.max = 2; a.mean = 1.5;
	LayerDelta d;
	ComputeDelta(b, a, d);
	EXPECT_EQ(-5, d.countDelta);
	EXPECT_FALSE(d.hasRange);
	b.count = 3; b.hasRange = true; b.min = 4; b.max = 8; b.mean = 6;
	ComputeDelta(b, a, d);
	EXPECT_EQ(-2, d.countDelta);
	EXPECT_TRUE(d.hasRange);
	EXPECT_DOUBLE_EQ(3.0, d.minDelta);
	EXPECT_DOUBLE_EQ(4.5, d.meanDelta);
	EXPECT_DOUBLE_EQ(6.0, d.maxDelta);
	b.count = 0xFFFFFFFFu; a.count = 0;
	ComputeDelta(b, a, d);
	EXPECT_EQ(4294967295ll, d.countDelta);	// no wrap-around at the uint32 limit
}

// ---------------------------------------------------------------- domain level: detail-provider reentrancy and sim-hosted commands

#ifdef DIA_DEBUG
namespace
{
	struct DomainRig
	{
		TestGrid grid;
		Dia::Graphics::CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider temp, rain;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 200));
			f.Add(UploaderSchemaFixture::FloatEntry("rain", 0, 100));
			return &f.Publish();
		}

		DomainRig()
			: grid(10, 10)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			temp.values.resize(100); rain.values.assign(100, 4.0f);
			for (size_t i = 0; i < 100; ++i) { temp.values[i] = static_cast<float>(i); }
			CellLayerDesc t = temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp");
			t.group.Clear(); t.group.Append("T");
			EXPECT_EQ(CellRegisterResult::Ok, registry.Register(t));
			CellLayerDesc r = rain.MakeDesc("rain", &grid.desc, CellValueKind::Float, "cells/rain");
			r.group.Clear(); r.group.Append("T");
			EXPECT_EQ(CellRegisterResult::Ok, registry.Register(r));
			domain.Update(draw);
		}
		void Settle() { for (int i = 0; i < 4; ++i) { domain.Update(draw); } }
	};

	Json::Value ParseJson(const char* text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::istringstream in(text);
		std::string errs;
		Json::parseFromStream(b, in, &v, &errs);
		return v;
	}
	Json::Value RunCmd(const char* cmd, const char* params) { return ViewerCommands::Exec(cmd, ParseJson(params)); }

	// A detail provider that calls back into the domain (commands and card state) while the domain is building the card or the
	// stats JSON. The viewer mutex is gone; this must simply work.
	struct CallbackProvider
	{
		CellLayerViewerDomain* domain = nullptr;
		bool inspect = false, cardState = false, unregisterSelf = false;
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		CellDetailRegistry* reg = nullptr;
		uint32_t calls = 0;
		uint32_t depth = 0;
		Json::Value nestedInspect;
		bool nestedCardHasInspector = false;

		void Provide(uint32_t cell, CellDetailBuilder& b)
		{
			++calls;
			b.Row("cell", "x");
			if (depth > 0) { return; }	// never recurse more than one level
			++depth;
			if (inspect) { nestedInspect = RunCmd("cells.inspect", "{\"all\":true}"); }
			if (cardState) { Json::Value s(Json::objectValue); domain->GetJSONState(s); nestedCardHasInspector = s.isMember("inspector"); }
			if (unregisterSelf) { reg->Unregister(grid, "Cb"); }
			--depth;
			(void)cell;
		}
	};

	struct InspectorDomainExhaustive : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(InspectorDomainExhaustive, DetailProviderCallingCellsInspectWhileTheCardIsBuiltWorks)
{
	DomainRig rig;
	CallbackProvider p; p.domain = &rig.domain; p.inspect = true;
	ASSERT_EQ(CellDetailRegisterResult::Ok, rig.registry.Details().Register(&rig.grid.desc, "Cb",
		fastdelegate::MakeDelegate(&p, &CallbackProvider::Provide)));
	ASSERT_TRUE(RunCmd("cells.select.cell", "{\"q\":3,\"r\":2}")["success"].asBool());
	rig.Settle();
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	ASSERT_GE(p.calls, 2u);	// outer card build + the nested cells.inspect build
	ASSERT_TRUE(p.nestedInspect["success"].asBool()) << p.nestedInspect.toStyledString();
	EXPECT_EQ(1u, p.nestedInspect["data"]["selected"].asUInt());
	const Json::Value& details = state["inspector"]["cell"]["details"];
	ASSERT_TRUE(details.isArray());
	EXPECT_GE(details.size(), 2u);	// section + row, intact after the nested call
	EXPECT_EQ(23u, state["inspector"]["cell"]["index"].asUInt());
}

TEST_F(InspectorDomainExhaustive, DetailProviderCallingCardStateRecursionIsBoundedAndWorks)
{
	DomainRig rig;
	CallbackProvider p; p.domain = &rig.domain; p.cardState = true;
	rig.registry.Details().Register(&rig.grid.desc, "Cb", fastdelegate::MakeDelegate(&p, &CallbackProvider::Provide));
	RunCmd("cells.select.cell", "{\"q\":1,\"r\":1}");
	rig.Settle();
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	EXPECT_TRUE(p.nestedCardHasInspector);
	EXPECT_TRUE(state["inspector"]["cell"]["details"].isArray());
	EXPECT_GE(p.calls, 2u);
}

TEST_F(InspectorDomainExhaustive, DetailProviderUnregisteringItselfDuringTheCardBuildIsSafe)
{
	DomainRig rig;
	CallbackProvider p; p.domain = &rig.domain; p.unregisterSelf = true; p.grid = &rig.grid.desc; p.reg = &rig.registry.Details();
	rig.registry.Details().Register(&rig.grid.desc, "Cb", fastdelegate::MakeDelegate(&p, &CallbackProvider::Provide));
	RunCmd("cells.select.cell", "{\"q\":0,\"r\":0}");
	rig.Settle();
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	EXPECT_EQ(0u, rig.registry.Details().ProviderCount());
	EXPECT_TRUE(state["inspector"]["cell"]["details"].isArray());
	Json::Value again(Json::objectValue);
	rig.domain.GetJSONState(again);	// second build: provider gone, the card still renders
	EXPECT_EQ(0u, again["inspector"]["cell"]["details"].size());
	EXPECT_EQ(1u, p.calls);
}

TEST_F(InspectorDomainExhaustive, ProviderIsNotCalledByInspectForMultiCellSelectionsOrAfterClear)
{
	DomainRig rig;
	StubDetailProvider sp; sp.rowsToEmit = 1;
	rig.registry.Details().Register(&rig.grid.desc, "Stub", sp.Delegate());
	RunCmd("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":1}");
	rig.Settle();
	RunCmd("cells.inspect", "{}");
	Json::Value s(Json::objectValue);
	rig.domain.GetJSONState(s);
	EXPECT_EQ(0u, sp.calls);
	RunCmd("cells.select.clear", "{}");
	rig.Settle();
	RunCmd("cells.inspect", "{}");
	EXPECT_EQ(0u, sp.calls);
}

TEST_F(InspectorDomainExhaustive, InspectWithEmptySelectionAndPinnedAStillReportsThePin)
{
	DomainRig rig;
	RunCmd("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	rig.Settle();
	ASSERT_TRUE(RunCmd("cells.select.pin", "{}")["success"].asBool());
	RunCmd("cells.select.clear", "{}");
	rig.Settle();
	const Json::Value r = RunCmd("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(0u, r["data"]["selected"].asUInt());
	EXPECT_EQ(0u, r["data"]["layers"].size());
	EXPECT_TRUE(r["data"].isMember("pinned"));
	EXPECT_EQ(9u, r["data"]["pinned"]["count"].asUInt());
}

TEST_F(InspectorDomainExhaustive, PinWithNothingSelectedIsAnErrorAndRepeatedUnpinIsHarmless)
{
	DomainRig rig;
	rig.Settle();
	const Json::Value pin = RunCmd("cells.select.pin", "{}");
	EXPECT_FALSE(pin["success"].asBool());
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
	const Json::Value freeze = RunCmd("cells.select.pin", "{\"freeze\":true}");
	EXPECT_FALSE(freeze["success"].asBool());
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
	EXPECT_NO_FATAL_FAILURE(RunCmd("cells.select.unpin", "{}"));
	EXPECT_NO_FATAL_FAILURE(RunCmd("cells.select.unpin", "{}"));
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
}

TEST_F(InspectorDomainExhaustive, InspectResultsAreDeterministicAcrossRepeatedCalls)
{
	DomainRig rig;
	RunCmd("cells.select.box", "{\"q0\":1,\"r0\":1,\"q1\":4,\"r1\":3}");
	rig.Settle();
	const std::string first = RunCmd("cells.inspect", "{\"all\":true}").toStyledString();
	const std::string second = RunCmd("cells.inspect", "{\"all\":true}").toStyledString();
	EXPECT_EQ(first, second);
	rig.Settle();
	EXPECT_EQ(first, RunCmd("cells.inspect", "{\"all\":true}").toStyledString());
}

TEST_F(InspectorDomainExhaustive, SelectionAfterLayerVersionBumpIsServedFreshByInspect)
{
	DomainRig rig;
	RunCmd("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":0}");
	rig.Settle();
	const Json::Value before = RunCmd("cells.inspect", "{}");
	ASSERT_TRUE(before["success"].asBool());
	rig.rain.values.assign(100, 9.0f);
	rig.rain.versionValue++;
	const Json::Value after = RunCmd("cells.inspect", "{\"layers\":[\"rain\"]}");	// no Update in between
	ASSERT_TRUE(after["success"].asBool()) << after.toStyledString();
	EXPECT_EQ(9.0, after["data"]["layers"][0u]["mean"].asDouble());
}
#endif // DIA_DEBUG
