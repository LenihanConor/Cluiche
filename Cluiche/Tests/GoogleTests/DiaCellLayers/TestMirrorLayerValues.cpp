#include <gtest/gtest.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/Strings/Base64.h>
#include <DiaDebugServer/QueryRegistry.h>
#include "../DiaValueSchema/ValueSchemaTestSupport.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString(); }

	// Test-side base64 decoder (independent of the production encoder).
	bool DecodeBase64(const std::string& text, std::vector<uint8_t>& out)
	{
		out.clear();
		if (text.size() % 4 != 0) { return false; }
		auto val = [](char c) -> int
		{
			if (c >= 'A' && c <= 'Z') { return c - 'A'; }
			if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
			if (c >= '0' && c <= '9') { return c - '0' + 52; }
			if (c == '+') { return 62; }
			if (c == '/') { return 63; }
			return -1;
		};
		for (size_t i = 0; i < text.size(); i += 4)
		{
			int pad = 0;
			uint32_t v = 0;
			for (int k = 0; k < 4; ++k)
			{
				const char c = text[i + k];
				int d = 0;
				if (c == '=') { ++pad; }
				else { d = val(c); if (d < 0 || pad > 0) { return false; } }
				v = (v << 6) | static_cast<uint32_t>(d);
			}
			if (pad > 2) { return false; }
			out.push_back(static_cast<uint8_t>(v >> 16));
			if (pad < 2) { out.push_back(static_cast<uint8_t>(v >> 8)); }
			if (pad < 1) { out.push_back(static_cast<uint8_t>(v)); }
		}
		return true;
	}

	std::vector<float> DecodeFloats(const Json::Value& r)
	{
		std::vector<float> out;
		if (!r.isObject() || !r["data"].isString()) { return out; }
		std::vector<uint8_t> bytes;
		if (!DecodeBase64(r["data"].asString(), bytes) || bytes.size() % 4 != 0) { ADD_FAILURE() << "bad base64 payload"; return out; }
		out.resize(bytes.size() / 4);
		for (size_t i = 0; i < out.size(); ++i)
		{
			const uint32_t u = static_cast<uint32_t>(bytes[i * 4]) | (static_cast<uint32_t>(bytes[i * 4 + 1]) << 8)
				| (static_cast<uint32_t>(bytes[i * 4 + 2]) << 16) | (static_cast<uint32_t>(bytes[i * 4 + 3]) << 24);
			std::memcpy(&out[i], &u, 4);
		}
		return out;
	}

	bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

	Json::Value Args(const char* layer, int64_t offset = -2, int64_t count = -2)
	{
		Json::Value a(Json::objectValue);
		a["layer"] = layer;
		if (offset != -2) { a["offset"] = static_cast<Json::Int64>(offset); }
		if (count != -2) { a["count"] = static_cast<Json::Int64>(count); }
		return a;
	}

	// A single-layer rig of w x h cells.
	struct ValuesRig
	{
		TestGrid grid;
		Dia::Graphics::CellFieldRegistry cellField;
		CountingLayerProvider prov;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };

		ValuesRig(uint16_t w, uint16_t h, const std::vector<float>& values, CellValueKind kind = CellValueKind::Float)
			: grid(w, h)
		{
			FakePuId() = 1;
			prov.values = values;
			EXPECT_EQ(registry.Register(MakeCountingLayer("vals", &grid.desc, prov, kind)), CellRegisterResult::Ok);
			domain.Update(draw);
		}
	};

	std::vector<float> Ramp(uint32_t n)
	{
		std::vector<float> v(n);
		for (uint32_t i = 0; i < n; ++i) { v[i] = static_cast<float>(i) * 0.37f - 123.0f; }
		return v;
	}

	struct LayerValuesTest : ::testing::Test
	{
		static const uint32_t kCells = 104u * 64u;	// 6,656 (Large)
		ValuesRig rig{ 104, 64, Ramp(kCells) };
	};
}

// ---- cells.layer.values ----

TEST_F(LayerValuesTest, SingleChunkReturnsFullLayer)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals"));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["layer"].asString(), "vals");
	EXPECT_EQ(r["encoding"].asString(), "f32le-base64");
	EXPECT_EQ(r["total"].asUInt(), kCells);
	EXPECT_EQ(r["offset"].asUInt(), 0u);
	EXPECT_EQ(r["count"].asUInt(), kCells);
	const std::vector<float> got = DecodeFloats(r);
	ASSERT_EQ(got.size(), kCells);
	const std::vector<float> want = Ramp(kCells);
	if (got.size() == kCells)
	{
		for (uint32_t i = 0; i < kCells; ++i)
		{
			if (!SameBits(got[i], want[i])) { ADD_FAILURE() << "value mismatch at " << i; break; }
		}
	}
}

TEST_F(LayerValuesTest, ValuesEqualWhatLayerReaderReturns)
{
	LayerReader reader(rig.registry);
	const LayerValues lv = reader.Read(StringCRC("vals"));
	ASSERT_TRUE(lv.hasData);
	ASSERT_EQ(lv.count, kCells);
	const std::vector<float> got = DecodeFloats(rig.queries.LayerValues(Args("vals")));
	ASSERT_EQ(got.size(), lv.count);
	if (got.size() == lv.count && lv.values != nullptr)
	{
		EXPECT_EQ(std::memcmp(got.data(), lv.values, kCells * sizeof(float)), 0);
	}
}

TEST_F(LayerValuesTest, GridIdWidthHeightVersion)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals"));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["gridId"].asString(), "grid.0");
	EXPECT_EQ(r["width"].asUInt(), 104u);
	EXPECT_EQ(r["height"].asUInt(), 64u);
	EXPECT_EQ(r["version"].asUInt64(), rig.prov.version);
}

TEST_F(LayerValuesTest, VersionChangesAfterLayerChanges)
{
	const Json::Value before = rig.queries.LayerValues(Args("vals"));
	ASSERT_FALSE(IsError(before));
	rig.prov.values[5] = 999.5f;
	rig.prov.Bump();
	const Json::Value after = rig.queries.LayerValues(Args("vals"));
	ASSERT_FALSE(IsError(after));
	EXPECT_NE(after["version"].asUInt64(), before["version"].asUInt64());
	EXPECT_EQ(after["version"].asUInt64(), rig.prov.version);
	const std::vector<float> got = DecodeFloats(after);
	ASSERT_GT(got.size(), 5u);
	if (got.size() > 5u) { EXPECT_EQ(got[5], 999.5f); }
}

TEST_F(LayerValuesTest, OffsetAndCountReturnASlice)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals", 10, 4));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["offset"].asUInt(), 10u);
	EXPECT_EQ(r["count"].asUInt(), 4u);
	const std::vector<float> got = DecodeFloats(r);
	const std::vector<float> want = Ramp(kCells);
	ASSERT_EQ(got.size(), 4u);
	if (got.size() == 4u) { for (uint32_t i = 0; i < 4; ++i) { EXPECT_TRUE(SameBits(got[i], want[10 + i])) << i; } }
}

TEST_F(LayerValuesTest, OffsetEqualTotalIsEmpty)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals", kCells));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), 0u);
	EXPECT_EQ(r["total"].asUInt(), kCells);
	EXPECT_EQ(r["data"].asString(), "");
}

TEST_F(LayerValuesTest, OffsetBeyondTotalIsError)
{
	EXPECT_TRUE(IsError(rig.queries.LayerValues(Args("vals", kCells + 1))));
}

TEST_F(LayerValuesTest, CountPastTheEndIsTrimmedToRemaining)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals", kCells - 3, 100));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), 3u);
	EXPECT_EQ(DecodeFloats(r).size(), 3u);
}

TEST_F(LayerValuesTest, CountZeroIsEmpty)
{
	const Json::Value r = rig.queries.LayerValues(Args("vals", 0, 0));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), 0u);
	EXPECT_EQ(r["data"].asString(), "");
}

TEST_F(LayerValuesTest, UnknownLayerIsError)
{
	EXPECT_TRUE(IsError(rig.queries.LayerValues(Args("nope"))));
}

TEST_F(LayerValuesTest, MissingOrNonStringLayerIsError)
{
	EXPECT_TRUE(IsError(rig.queries.LayerValues(Json::Value(Json::objectValue))));
	EXPECT_TRUE(IsError(rig.queries.LayerValues(Json::Value(Json::nullValue))));
	Json::Value a(Json::objectValue);
	a["layer"] = 5;
	EXPECT_TRUE(IsError(rig.queries.LayerValues(a)));
}

TEST_F(LayerValuesTest, BadOffsetAndCountAreErrors)
{
	for (const char* key : { "offset", "count" })
	{
		const Json::Value bad[] = { Json::Value("abc"), Json::Value(-1), Json::Value(1.5), Json::Value(true), Json::Value(Json::nullValue) };
		for (const Json::Value& b : bad)
		{
			Json::Value a = Args("vals");
			a[key] = b;
			EXPECT_TRUE(IsError(rig.queries.LayerValues(a))) << key << " = " << b.toStyledString();
		}
	}
}

TEST_F(LayerValuesTest, QueryIsReadOnly)
{
	const uint64_t version = rig.registry.Find(StringCRC("vals")) ? rig.prov.version : 0;
	const uint32_t layersVersion = rig.registry.LayersVersion();
	const std::vector<float> valuesBefore = rig.prov.values;
	rig.queries.LayerValues(Args("vals"));
	rig.queries.LayerValues(Args("vals", 3, 5));
	rig.queries.LayerValues(Args("nope"));
	EXPECT_EQ(rig.prov.version, version);
	EXPECT_EQ(rig.registry.LayersVersion(), layersVersion);
	EXPECT_EQ(rig.prov.values, valuesBefore);
	const ICellSelectionView* view = rig.domain.ActiveSelectionView();
	if (view != nullptr) { EXPECT_EQ(view->Count(), 0u); }
}

TEST_F(LayerValuesTest, RegisteredHandlerAnswersAndUnregisters)
{
	Dia::DebugServer::QueryRegistry qr;
	RegisterMirrorQueries(qr, rig.queries);
	ASSERT_TRUE(qr.Has(StringCRC("cells.layer.values")));
	const Json::Value r = qr.Execute(StringCRC("cells.layer.values"), Args("vals", 0, 2));
	EXPECT_EQ(r["count"].asUInt(), 2u);
	UnregisterMirrorQueries(qr);
	EXPECT_FALSE(qr.Has(StringCRC("cells.layer.values")));
}

TEST_F(LayerValuesTest, CategoryLayerIdsTravelAsExactFloats)
{
	ValuesRig cat(4, 1, { 0.0f, 1.0f, 7.0f, 65535.0f }, CellValueKind::Category);
	const std::vector<float> got = DecodeFloats(cat.queries.LayerValues(Args("vals")));
	ASSERT_EQ(got.size(), 4u);
	if (got.size() == 4u)
	{
		EXPECT_EQ(got[0], 0.0f);
		EXPECT_EQ(got[1], 1.0f);
		EXPECT_EQ(got[2], 7.0f);
		EXPECT_EQ(got[3], 65535.0f);
	}
}

// ---- chunking ----

namespace
{
	struct LayerValuesChunkTest : LayerValuesTest
	{
		static const uint32_t kChunk = 1000;
		void SetUp() override { rig.queries.SetMaxValuesChunk(kChunk); }
	};
}

TEST_F(LayerValuesChunkTest, ChunksConcatenateToFullArray)
{
	const std::vector<float> want = Ramp(kCells);
	std::vector<float> all;
	uint32_t offset = 0;
	uint32_t calls = 0;
	while (offset < kCells && calls < 20)
	{
		const Json::Value r = rig.queries.LayerValues(Args("vals", offset));
		ASSERT_FALSE(IsError(r));
		const uint32_t n = r["count"].asUInt();
		EXPECT_LE(n, kChunk);
		EXPECT_GT(n, 0u);
		const std::vector<float> part = DecodeFloats(r);
		ASSERT_EQ(part.size(), n);
		all.insert(all.end(), part.begin(), part.end());
		offset += n;
		++calls;
		if (n == 0) { break; }
	}
	EXPECT_EQ(calls, 7u);	// 6 x 1000 + 656
	ASSERT_EQ(all.size(), kCells);
	if (all.size() == kCells)
	{
		for (uint32_t i = 0; i < kCells; ++i)
		{
			if (!SameBits(all[i], want[i])) { ADD_FAILURE() << "value mismatch at " << i; break; }
		}
	}
}

TEST_F(LayerValuesChunkTest, BoundariesAtChunkEdges)
{
	Json::Value r = rig.queries.LayerValues(Args("vals", kChunk));	// exactly at an edge
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["offset"].asUInt(), kChunk);
	EXPECT_EQ(r["count"].asUInt(), kChunk);
	r = rig.queries.LayerValues(Args("vals", kChunk - 1));	// straddles an edge: still one full chunk
	EXPECT_EQ(r["count"].asUInt(), kChunk);
	r = rig.queries.LayerValues(Args("vals", 6000));	// last partial chunk
	EXPECT_EQ(r["count"].asUInt(), 656u);
	r = rig.queries.LayerValues(Args("vals", kCells));	// end
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), 0u);
}

TEST_F(LayerValuesChunkTest, CountAboveLimitIsClampedWithWarning)
{
	LogCapture log("CEL");
	const Json::Value r = rig.queries.LayerValues(Args("vals", 0, 5000));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), kChunk);
	EXPECT_EQ(log.Warnings("clamped"), 1u);
}

TEST_F(LayerValuesChunkTest, VersionIsStableAcrossChunksUntilLayerChanges)
{
	const Json::Value a = rig.queries.LayerValues(Args("vals", 0));
	const Json::Value b = rig.queries.LayerValues(Args("vals", kChunk));
	EXPECT_EQ(a["version"].asUInt64(), b["version"].asUInt64());
	rig.prov.Bump();
	const Json::Value c = rig.queries.LayerValues(Args("vals", 2 * kChunk));
	EXPECT_NE(c["version"].asUInt64(), a["version"].asUInt64());
}

TEST(LayerValuesLimits, DefaultChunkFitsOneMebibyteOfBase64)
{
	EXPECT_EQ(MirrorQueries::kMaxValuesChunk, 196608u);
	const uint32_t cells = MirrorQueries::kMaxValuesChunk;
	ValuesRig big(512, 384, std::vector<float>(cells, 1.25f));
	const Json::Value r = big.queries.LayerValues(Args("vals"));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["count"].asUInt(), cells);
	// 786,432 raw bytes -> 1,048,576 base64 characters (1 MiB exactly) plus a few hundred bytes of envelope.
	EXPECT_LE(r["data"].asString().size(), 1048576u);
	EXPECT_LT(r.toStyledString().size(), 1048576u + 512u);
	EXPECT_EQ(DecodeFloats(r).size(), cells);
}

TEST(LayerValuesLimits, ChunkSetterClampsToValidRange)
{
	ValuesRig rig(4, 1, { 1, 2, 3, 4 });
	rig.queries.SetMaxValuesChunk(0);
	EXPECT_EQ(rig.queries.MaxValuesChunk(), 1u);
	rig.queries.SetMaxValuesChunk(0xFFFFFFFFu);
	EXPECT_EQ(rig.queries.MaxValuesChunk(), MirrorQueries::kMaxValuesChunk);
}

TEST(LayerValuesBase64, KnownVectors)
{
	auto enc = [](const char* s) { return Dia::Core::Base64::Encode(reinterpret_cast<const uint8_t*>(s), std::strlen(s)); };
	EXPECT_EQ(enc(""), "");
	EXPECT_EQ(enc("f"), "Zg==");
	EXPECT_EQ(enc("fo"), "Zm8=");
	EXPECT_EQ(enc("foo"), "Zm9v");
	EXPECT_EQ(enc("foob"), "Zm9vYg==");
	EXPECT_EQ(enc("fooba"), "Zm9vYmE=");
	EXPECT_EQ(enc("foobar"), "Zm9vYmFy");
}

// ---- C++ -> TypeScript conformance fixture ----

namespace
{
	struct FixtureCase { const char* name; std::vector<float> values; CellValueKind kind; uint32_t offset; uint32_t count; };

	std::string BuildFixtureText()
	{
		const std::vector<float> awkward = { -1.5f, 0.1f, 1e-40f, 3.4028235e38f, 1.4e-45f, 123456.789f, 1.17549435e-38f, -3.4028235e38f };
		const std::vector<FixtureCase> cases = {
			{ "one_value_pad2", { 1.0f }, CellValueKind::Float, 0, 1 },
			{ "two_values_pad1", { 1.0f, 2.0f }, CellValueKind::Float, 0, 2 },
			{ "three_values_nopad", { 0.0f, 0.5f, -0.5f }, CellValueKind::Float, 0, 3 },
			{ "awkward_floats", awkward, CellValueKind::Float, 0, static_cast<uint32_t>(awkward.size()) },
			{ "awkward_slice", awkward, CellValueKind::Float, 2, 3 },
			{ "category_ids", { 0.0f, 1.0f, 2.0f, 3.0f, 7.0f, 255.0f, 4096.0f, 65535.0f }, CellValueKind::Category, 0, 8 },
			{ "empty_at_end", { 1.0f, 2.0f, 3.0f }, CellValueKind::Float, 3, 0 },
		};

		Json::Value root(Json::objectValue);
		root["description"] = "Produced by the C++ cells.layer.values encoder (float32 little-endian, base64). "
			"values are float32 widened to double (exact); the whole layer is values, data covers cells [offset, offset+count). "
			"Regenerate with DIA_UPDATE_GOLDEN=1 (TestMirrorLayerValues.cpp).";
		Json::Value arr(Json::arrayValue);
		for (const FixtureCase& c : cases)
		{
			ValuesRig rig(static_cast<uint16_t>(c.values.size()), 1, c.values, c.kind);
			Json::Value q(Json::objectValue);
			q["layer"] = "vals";
			q["offset"] = c.offset;
			q["count"] = c.count;
			const Json::Value r = rig.queries.LayerValues(q);
			Json::Value e(Json::objectValue);
			e["name"] = c.name;
			Json::Value vals(Json::arrayValue);
			for (float f : c.values) { vals.append(static_cast<double>(f)); }
			e["values"] = vals;
			e["offset"] = c.offset;
			e["count"] = c.count;
			e["encoding"] = r.isMember("encoding") ? r["encoding"] : Json::Value("");
			e["data"] = r.isMember("data") ? r["data"] : Json::Value("");
			arr.append(e);
		}
		root["cases"] = arr;
		Json::StyledWriter w;
		return w.write(root);
	}
}

TEST(LayerValuesConformance, FixtureMatchesCppEncoder)
{
	const auto selPath = VsTest::FindUpwards(__FILE__, "Dia/DiaCellLayers/Testing/Conformance/selection_hash.json");
	ASSERT_FALSE(selPath.empty()) << "Conformance folder not found";
	const auto file = selPath.parent_path() / "layer_values.json";
	const std::string produced = BuildFixtureText();
	ASSERT_FALSE(produced.empty());

	const char* update = std::getenv("DIA_UPDATE_GOLDEN");
	if (update != nullptr && std::strcmp(update, "1") == 0)
	{
		std::ofstream f(file, std::ios::binary | std::ios::trunc);
		ASSERT_TRUE(f.good()) << "cannot write " << file.string();
		f << produced;
		return;
	}

	std::string expected;
	ASSERT_TRUE(VsTest::ReadFileBytes(file, expected)) << "layer_values.json missing; regenerate with DIA_UPDATE_GOLDEN=1";
	EXPECT_EQ(expected, produced) << "layer_values.json differs from the C++ encoder; regenerate with DIA_UPDATE_GOLDEN=1 and review";
}

TEST(LayerValuesConformance, FixtureCasesDecodeToTheirValues)
{
	const auto selPath = VsTest::FindUpwards(__FILE__, "Dia/DiaCellLayers/Testing/Conformance/selection_hash.json");
	ASSERT_FALSE(selPath.empty());
	std::string text;
	ASSERT_TRUE(VsTest::ReadFileBytes(selPath.parent_path() / "layer_values.json", text)) << "layer_values.json missing";
	Json::Value root;
	Json::Reader reader;
	ASSERT_TRUE(reader.parse(text, root));
	ASSERT_TRUE(root["cases"].isArray());
	ASSERT_GE(root["cases"].size(), 6u);
	for (const Json::Value& c : root["cases"])
	{
		Json::Value fake(Json::objectValue);
		fake["data"] = c["data"];
		const std::vector<float> got = DecodeFloats(fake);
		ASSERT_EQ(got.size(), c["count"].asUInt()) << c["name"].asString();
		if (got.size() != c["count"].asUInt()) { continue; }
		for (uint32_t i = 0; i < got.size(); ++i)
		{
			const float want = static_cast<float>(c["values"][c["offset"].asUInt() + i].asDouble());
			EXPECT_TRUE(SameBits(got[i], want)) << c["name"].asString() << " @" << i;
		}
	}
}

#endif // DIA_DEBUG
