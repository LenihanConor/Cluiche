#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCore/Json/external/json/json.h>
#include "../DiaValueSchema/ValueSchemaTestSupport.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
namespace Fn = Dia::CellLayers::SelectionFingerprintFn;

namespace
{
	std::string Hex(uint64_t h) { char b[Fn::kHexLength + 1]; Fn::ToHex(h, b); return b; }

	void Fill(CellSelection& sel, const std::vector<uint32_t>& idx)
	{
		sel.BeginEdit();
		for (uint32_t i : idx) { sel.Set(i, true); }
		sel.EndEdit();
	}

	Json::Value LoadVectors()
	{
		Json::Value root;
		const auto path = VsTest::FindUpwards(__FILE__, "Dia/DiaCellLayers/Testing/Conformance/selection_hash.json");
		std::string text;
		if (path.empty() || !VsTest::ReadFileBytes(path, text)) { return root; }
		Json::Reader reader;
		reader.parse(text, root);
		return root;
	}
}

TEST(SelectionFingerprint, ConformanceVectorsHashIndexLists)
{
	const Json::Value root = LoadVectors();
	ASSERT_TRUE(root.isMember("vectors")) << "selection_hash.json not found/parsed";
	ASSERT_GE(root["vectors"].size(), 8u);
	for (const Json::Value& v : root["vectors"])
	{
		std::vector<uint32_t> idx;
		for (const Json::Value& i : v["indices"]) { idx.push_back(i.asUInt()); }
		EXPECT_EQ(Hex(Fn::HashOf(idx.data(), (uint32_t)idx.size())), v["hash"].asString()) << v["name"].asString();
	}
}

TEST(SelectionFingerprint, ConformanceVectorsThroughCellSelection)
{
	const Json::Value root = LoadVectors();
	ASSERT_TRUE(root.isMember("vectors"));
	Dia::Graphics::CellGridDesc grid = MakeGrid(104, 64);	// 6,656 cells
	for (const Json::Value& v : root["vectors"])
	{
		CellSelection sel;
		sel.Reset(&grid);
		std::vector<uint32_t> idx;
		for (const Json::Value& i : v["indices"]) { idx.push_back(i.asUInt()); }
		Fill(sel, idx);
		const SelectionFingerprint fp = Fn::Compute(sel);
		EXPECT_EQ(Hex(fp.mHash), v["hash"].asString()) << v["name"].asString();
		EXPECT_EQ(fp.mCount, sel.Count()) << v["name"].asString();
	}
}

TEST(SelectionFingerprint, OrderIndependent)
{
	std::vector<uint32_t> a;
	for (uint32_t i = 0; i < 200; ++i) { a.push_back(i * 7 % 6656); }
	std::vector<uint32_t> b = a;
	std::mt19937 rng(42);
	std::shuffle(b.begin(), b.end(), rng);
	EXPECT_NE(Fn::HashOf(a.data(), (uint32_t)a.size()), 0u);
	EXPECT_EQ(Fn::HashOf(a.data(), (uint32_t)a.size()), Fn::HashOf(b.data(), (uint32_t)b.size()));
}

TEST(SelectionFingerprint, EmptySelectionIsZeroHex)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(104, 64);
	CellSelection sel;
	sel.Reset(&grid);
	const SelectionFingerprint fp = Fn::Compute(sel);
	EXPECT_EQ(fp.mCount, 0u);
	EXPECT_EQ(fp.mHash, 0u);
	EXPECT_EQ(Hex(fp.mHash), "0000000000000000");
}

TEST(SelectionFingerprint, FullGridSelection)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(104, 64);
	CellSelection sel;
	sel.Reset(&grid);
	std::vector<uint32_t> all;
	for (uint32_t i = 0; i < 6656; ++i) { all.push_back(i); }
	Fill(sel, all);
	const SelectionFingerprint fp = Fn::Compute(sel);
	EXPECT_EQ(fp.mCount, 6656u);
	EXPECT_EQ(Hex(fp.mHash), "7395c2449a51fedb");
}

TEST(SelectionFingerprint, OneCellSelectionAndMix)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(104, 64);
	CellSelection sel;
	sel.Reset(&grid);
	sel.Set(0, true);
	const SelectionFingerprint fp = Fn::Compute(sel);
	EXPECT_EQ(fp.mCount, 1u);
	EXPECT_EQ(Hex(fp.mHash), "e220a8397b1dcdaf");
	EXPECT_EQ(Fn::Mix(0), 0xe220a8397b1dcdafull);
}

TEST(SelectionFingerprint, CarriesCountAndVersion)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(10, 10);
	CellSelection sel;
	sel.Reset(&grid);
	sel.Set(3, true);
	sel.Set(4, true);
	const SelectionFingerprint fp = Fn::Compute(sel);
	EXPECT_EQ(fp.mCount, 2u);
	EXPECT_EQ(fp.mVersion, sel.Version());
	EXPECT_GT(fp.mVersion, 0u);
	sel.Set(5, true);
	EXPECT_NE(Fn::Compute(sel), fp);
	EXPECT_EQ(Fn::Compute(sel).mVersion, sel.Version());
}

TEST(SelectionFingerprint, HexIsLowerCaseSixteenWithLeadingZeros)
{
	EXPECT_EQ(Hex(0x1ull), "0000000000000001");
	EXPECT_EQ(Hex(0xABCDEF0123456789ull), "abcdef0123456789");
	EXPECT_EQ(Hex(~0ull), "ffffffffffffffff");
	EXPECT_EQ(Hex(0).size(), 16u);
}

TEST(SelectionFingerprint, CacheRecomputesOnlyOnVersionChange)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(10, 10);
	CellSelection sel;
	sel.Reset(&grid);
	sel.Set(1, true);
	SelectionFingerprintCache cache;
	const SelectionFingerprint a = cache.Get(sel);
	EXPECT_EQ(a, Fn::Compute(sel));
	EXPECT_EQ(cache.Get(sel), a);
	sel.Set(2, true);
	EXPECT_EQ(cache.Get(sel), Fn::Compute(sel));
	EXPECT_NE(cache.Get(sel), a);
}
