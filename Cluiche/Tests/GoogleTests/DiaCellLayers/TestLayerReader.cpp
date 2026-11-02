#include <gtest/gtest.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <stdio.h>
#include <string>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct LayerReaderTest : ::testing::Test
	{
		Dia::Graphics::CellGridDesc grid = MakeGrid(2, 2);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider a, b;

		void SetUp() override
		{
			FakePuId() = 1;
			a.values = { 1, 2, 3, 4 };
			b.values = { 10, 20, 30, 40 };
		}
	};
}

TEST_F(LayerReaderTest, FirstReadCallsReadOnceAndReturnsValues)
{
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	const LayerValues v = reader.Read(StringCRC("a"));
	ASSERT_TRUE(v.hasData);
	ASSERT_EQ(4u, v.count);
	EXPECT_EQ(3.0f, v.values[2]);
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(1u, reader.Reads());
}

TEST_F(LayerReaderTest, UnchangedVersionIsCacheHit)
{
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_TRUE(v.hasData);
	EXPECT_EQ(2.0f, v.values[1]);
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(1u, reader.CacheHits());
}

TEST_F(LayerReaderTest, VersionBumpReadsOnceMore)
{
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	a.values = { 9, 9, 9, 9 };
	a.Bump();
	const LayerValues v = reader.Read(StringCRC("a"));
	reader.Read(StringCRC("a"));
	EXPECT_EQ(2u, a.readCalls);
	EXPECT_EQ(9.0f, v.values[0]);
	EXPECT_EQ(a.version, v.version);
}

TEST_F(LayerReaderTest, UnknownIdHasNoData)
{
	LayerReader reader(registry);
	const LayerValues v = reader.Read(StringCRC("nope"));
	EXPECT_FALSE(v.hasData);
	EXPECT_EQ(nullptr, v.values);
	EXPECT_EQ(0u, reader.Reads());
}

TEST_F(LayerReaderTest, TwoLayersCachedIndependently)
{
	registry.Register(MakeCountingLayer("a", &grid, a));
	registry.Register(MakeCountingLayer("b", &grid, b));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	reader.Read(StringCRC("b"));
	const LayerValues va = reader.Read(StringCRC("a"));
	const LayerValues vb = reader.Read(StringCRC("b"));
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(1u, b.readCalls);
	EXPECT_EQ(1.0f, va.values[0]);
	EXPECT_EQ(40.0f, vb.values[3]);
	b.Bump();
	reader.Read(StringCRC("b"));
	reader.Read(StringCRC("a"));
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(2u, b.readCalls);
}

TEST_F(LayerReaderTest, LiveSourceUsesLayerReadNotSource)
{
	StubViewValueSource source;
	source.live = true;
	source.values = std::vector<float>(4, 42.0f);
	registry.SetViewValueSource(&source);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_EQ(1.0f, v.values[0]);
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(0u, source.getValuesCalls);
}

TEST_F(LayerReaderTest, PastViewReturnsSourceValuesWithoutLayerRead)
{
	StubViewValueSource source;
	source.live = false;
	source.values = std::vector<float>(4, 42.0f);
	registry.SetViewValueSource(&source);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	const LayerValues v = reader.Read(StringCRC("a"));
	ASSERT_TRUE(v.hasData);
	EXPECT_EQ(42.0f, v.values[3]);
	EXPECT_EQ(0u, a.readCalls);
	reader.Read(StringCRC("a"));
	EXPECT_EQ(1u, source.getValuesCalls);
}

TEST_F(LayerReaderTest, SourceEpochChangeRereads)
{
	StubViewValueSource source;
	source.live = false;
	source.values = std::vector<float>(4, 1.0f);
	registry.SetViewValueSource(&source);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	source.values = std::vector<float>(4, 2.0f);
	++source.epoch;
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_EQ(2u, source.getValuesCalls);
	EXPECT_EQ(2.0f, v.values[0]);
}

TEST_F(LayerReaderTest, PastViewWithoutDataReportsNoneAndNeverFallsBack)
{
	StubViewValueSource source;
	source.live = false;
	source.hasData = false;
	registry.SetViewValueSource(&source);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_FALSE(v.hasData);
	EXPECT_EQ(nullptr, v.values);
	EXPECT_EQ(0u, a.readCalls);
	reader.Read(StringCRC("a"));
	EXPECT_EQ(1u, source.getValuesCalls);	// the no-data answer is cached too
}

TEST_F(LayerReaderTest, BackToLiveFallsBackToLayerRead)
{
	StubViewValueSource source;
	source.live = false;
	source.values = std::vector<float>(4, 42.0f);
	registry.SetViewValueSource(&source);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	source.live = true;
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_EQ(1u, a.readCalls);
	EXPECT_EQ(1.0f, v.values[0]);
}

TEST_F(LayerReaderTest, ReRegisteredWithDifferentGridIsNotStale)
{
	Dia::Graphics::CellGridDesc big = MakeGrid(3, 3);
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	registry.Unregister(StringCRC("a"));
	registry.Register(MakeCountingLayer("a", &big, a));	// same version, new grid
	const LayerValues v = reader.Read(StringCRC("a"));
	EXPECT_EQ(9u, v.count);
	EXPECT_EQ(2u, a.readCalls);
}

TEST_F(LayerReaderTest, MoreLayersThanSlotsStillCorrect)
{
	Dia::Graphics::CellGridDesc tiny = MakeGrid(1, 1);
	const uint32_t n = CellLayerRegistry::kMaxLayers;
	std::vector<CountingLayerProvider> providers(n);
	std::vector<std::string> names;
	for (uint32_t i = 0; i < n; ++i)
	{
		names.push_back("L" + std::to_string(i));
		providers[i].values = { static_cast<float>(i) };
		registry.Register(MakeCountingLayer(names[i].c_str(), &tiny, providers[i]));
	}
	LayerReader reader(registry, 4);	// far fewer slots than layers
	for (int pass = 0; pass < 2; ++pass)
	{
		for (uint32_t i = 0; i < n; ++i)
		{
			const LayerValues v = reader.Read(StringCRC(names[i].c_str()));
			ASSERT_TRUE(v.hasData);
			EXPECT_EQ(static_cast<float>(i), v.values[0]);
		}
	}
}

TEST_F(LayerReaderTest, InvalidateForcesReread)
{
	registry.Register(MakeCountingLayer("a", &grid, a));
	LayerReader reader(registry);
	reader.Read(StringCRC("a"));
	reader.Invalidate();
	reader.Read(StringCRC("a"));
	EXPECT_EQ(2u, a.readCalls);
}

// ---- Task 10a (Feature 4 pass): a read delegate that re-enters the reader ----

namespace
{
	LayerReader* gReentrantReader = nullptr;
	LayerValues gNestedValues;
	float gNestedCopy[4] = {};
	void ReadThenReenter(float* out, uint32_t n)
	{
		for (uint32_t i = 0; i < n; ++i) { out[i] = 7.0f; }
		gNestedValues = gReentrantReader->Read(StringCRC("b"));	// first read of another layer while this buffer is being filled
		for (uint32_t i = 0; i < 4 && gNestedValues.values != nullptr; ++i) { gNestedCopy[i] = gNestedValues.values[i]; }
	}
}

TEST_F(LayerReaderTest, ReentrantReadOfAnotherLayerDoesNotOverwriteTheOuterBuffer)
{
	CellLayerDesc outer = MakeCountingLayer("a", &grid, a);
	outer.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadThenReenter);
	registry.Register(outer);
	registry.Register(MakeCountingLayer("b", &grid, b));
	LayerReader reader(registry);
	gReentrantReader = &reader;
	const LayerValues v = reader.Read(StringCRC("a"));
	gReentrantReader = nullptr;
	ASSERT_TRUE(v.hasData);
	for (uint32_t i = 0; i < 4; ++i) { EXPECT_EQ(7.0f, v.values[i]) << i; }
	ASSERT_TRUE(gNestedValues.hasData);
	EXPECT_EQ(20.0f, gNestedCopy[1]);
	const LayerValues b2 = reader.Read(StringCRC("b"));	// the nested layer is cached and intact
	EXPECT_EQ(40.0f, b2.values[3]);
	EXPECT_EQ(1u, b.readCalls);
}

TEST_F(LayerReaderTest, ReentrantReadWithASingleSlotReturnsNoDataInsteadOfTouchingTheBuffer)
{
	CellLayerDesc outer = MakeCountingLayer("a", &grid, a);
	outer.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadThenReenter);
	registry.Register(outer);
	registry.Register(MakeCountingLayer("b", &grid, b));
	LayerReader reader(registry, 1);
	gReentrantReader = &reader;
	gNestedValues = LayerValues();
	gNestedValues.hasData = true;
	const LayerValues v = reader.Read(StringCRC("a"));
	gReentrantReader = nullptr;
	ASSERT_TRUE(v.hasData);
	for (uint32_t i = 0; i < 4; ++i) { EXPECT_EQ(7.0f, v.values[i]) << i; }
	EXPECT_FALSE(gNestedValues.hasData);
	EXPECT_EQ(nullptr, gNestedValues.values);
}
