#include "HistoryTestRig.h"
#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

#include <json/json.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	namespace fs = std::filesystem;

	// temp (float 0..100), biome (category, 3 values, ids a b c), 4x3 grid.
	struct Fix
	{
		Rig r;
		FakeSnapshotStore store;
		char err[256] = {};
		SnapshotCellData data;

		void Setup()
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
			r.Start();
			r.Take();
			r.Take();
		}
		bool Build(const char* name = "pre-drought", int64_t unix = 1792510000, const char* world = "coast_range")
		{
			return SnapshotCellFile::BuildFromLive(*r.rec, r.registry, r.schema.service.get(), name, unix, world, data, err, sizeof(err));
		}
		std::string PathFor(const char* name = "pre-drought")
		{
			char p[300];
			EXPECT_TRUE(SnapshotCellFile::Path(store.Folder(), name, p, sizeof(p)));
			return p;
		}
	};

	std::string ReadAll(const std::string& path)
	{
		std::ifstream f(path, std::ios::binary);
		std::stringstream ss; ss << f.rdbuf();
		return ss.str();
	}
	void WriteAll(const std::string& path, const std::string& text) { std::ofstream f(path, std::ios::binary); f << text; }
	Json::Value LoadJson(const std::string& path)
	{
		Json::Value v; Json::Reader rd; rd.parse(ReadAll(path), v); return v;
	}
	void SaveJson(const std::string& path, const Json::Value& v)
	{
		Json::StreamWriterBuilder b; b["indentation"] = "";
		WriteAll(path, Json::writeString(b, v));
	}
	bool HasTmp(const std::string& folder)
	{
		for (const auto& e : fs::directory_iterator(folder)) { if (e.path().extension() == ".tmp") { return true; } }
		return false;
	}
}

TEST(SnapshotCellFile, BuildCapturesHeaderGridAndEveryRecordedLayer)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build()) << f.err;
	EXPECT_STREQ(f.data.mHeader.mName, "pre-drought");
	EXPECT_EQ(f.data.mHeader.mRunId, f.r.rec->RunId());
	EXPECT_EQ(f.data.mHeader.mSample, f.r.rec->SampleCount() - 1);
	EXPECT_STREQ(f.data.mHeader.mLabel, f.r.rec->Header(f.r.rec->SampleCount() - 1).mLabel.AsCStr());
	EXPECT_EQ(f.data.mHeader.mCreatedUnix, 1792510000);
	EXPECT_STREQ(f.data.mHeader.mWorld, "coast_range");
	EXPECT_EQ(f.data.mGrid.mWidth, 4u);
	EXPECT_EQ(f.data.mGrid.mHeight, 3u);
	EXPECT_STREQ(f.data.mGrid.mId, "test.stubgrid");
	ASSERT_EQ(f.data.LayerCount(), 2u);
	const SnapshotLayer* temp = f.data.FindLayer(StringCRC("temp"));
	ASSERT_NE(temp, nullptr);
	EXPECT_EQ(temp->mKind, SnapshotLayerKind::Float);
	EXPECT_FLOAT_EQ(temp->mMin, 0.f);
	EXPECT_FLOAT_EQ(temp->mMax, 100.f);
	const SnapshotLayer* biome = f.data.FindLayer(StringCRC("biome"));
	ASSERT_NE(biome, nullptr);
	EXPECT_EQ(biome->mKind, SnapshotLayerKind::Category);
	ASSERT_EQ(biome->EnumCount(), 3u);
	EXPECT_STREQ(biome->EnumName(0), "a");
	EXPECT_STREQ(biome->EnumName(2), "c");
}

TEST(SnapshotCellFile, BuildReadsLiveValuesNotTheRing)
{
	Fix f; f.Setup();
	StringCRC t("temp");
	CellHistoryRecorder::TrackResult tr;
	ASSERT_TRUE(f.r.rec->Track(&t, 1, tr));
	f.r.Take();
	f.r.grid.SetAll("temp", 40.0f);
	f.r.Take();
	f.r.grid.Set("temp", 5, 77.0f);	// changed after the last sample: only the live read sees it
	ASSERT_TRUE(f.Build());
	float out[12];
	ASSERT_TRUE(SnapshotCellFile::Dequantise(f.data, t, out, 12));
	EXPECT_NEAR(out[5], 77.0f, 100.0f / 65535.0f);
	EXPECT_NEAR(out[0], 40.0f, 100.0f / 65535.0f);
}

TEST(SnapshotCellFile, WriteReadRoundTripWithinQuantisationError)
{
	Fix f; f.Setup();
	for (uint32_t i = 0; i < 12; ++i)
	{
		f.r.grid.Set("temp", i, 90.0f * float(i) / 11.0f + 0.0123f * float(i));
		f.r.grid.Set("biome", i, float(i % 3));
	}
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err))) << f.err;
	EXPECT_FALSE(HasTmp(f.store.Folder()));

	SnapshotCellData back;
	ASSERT_TRUE(SnapshotCellFile::Read(path.c_str(), back, f.err, sizeof(f.err))) << f.err;
	EXPECT_STREQ(back.mHeader.mName, "pre-drought");
	EXPECT_EQ(back.mHeader.mRunId, f.data.mHeader.mRunId);
	EXPECT_EQ(back.mHeader.mSample, f.data.mHeader.mSample);
	EXPECT_STREQ(back.mHeader.mLabel, f.data.mHeader.mLabel);
	EXPECT_EQ(back.mHeader.mCreatedUnix, 1792510000);
	EXPECT_STREQ(back.mHeader.mWorld, "coast_range");
	EXPECT_EQ(back.mGrid.mWidth, 4u);
	EXPECT_EQ(back.mGrid.mHeight, 3u);
	ASSERT_EQ(back.LayerCount(), 2u);

	float live[12], got[12];
	ASSERT_TRUE(f.r.registry.Find(StringCRC("temp"))->read.empty() == false);
	f.r.registry.Find(StringCRC("temp"))->read(live, 12);
	ASSERT_TRUE(SnapshotCellFile::Dequantise(back, StringCRC("temp"), got, 12));
	for (uint32_t i = 0; i < 12; ++i) { EXPECT_NEAR(got[i], live[i], 100.0f / 65535.0f) << i; }
	ASSERT_TRUE(SnapshotCellFile::Dequantise(back, StringCRC("biome"), got, 12));
	for (uint32_t i = 0; i < 12; ++i) { EXPECT_EQ(got[i], float(i % 3)) << i; }
	EXPECT_EQ(back.FindLayer(StringCRC("biome"))->EnumCount(), 3u);
}

TEST(SnapshotCellFile, RunIdIsSixteenHexDigitsInFile)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	const Json::Value v = LoadJson(path);
	EXPECT_EQ(v["format"].asString(), "dia.cell_snapshot");
	EXPECT_EQ(v["version"].asInt(), 1);
	const std::string id = v["header"]["runId"].asString();
	ASSERT_EQ(id.size(), 16u);
	char expect[32];
	snprintf(expect, sizeof(expect), "%016llx", (unsigned long long)f.r.rec->RunId());
	EXPECT_EQ(id, expect);
}

TEST(SnapshotCellFile, NaNAndOutOfRangeCells)
{
	Fix f; f.Setup();
	f.r.grid.Set("temp", 0, std::nanf(""));
	f.r.grid.Set("temp", 1, -50.0f);
	f.r.grid.Set("temp", 2, 500.0f);
	f.r.grid.Set("biome", 0, std::nanf(""));
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	SnapshotCellData back;
	ASSERT_TRUE(SnapshotCellFile::Read(path.c_str(), back, f.err, sizeof(f.err)));
	float got[12];
	ASSERT_TRUE(SnapshotCellFile::Dequantise(back, StringCRC("temp"), got, 12));
	EXPECT_TRUE(std::isnan(got[0]));
	EXPECT_FLOAT_EQ(got[1], 0.0f);
	EXPECT_NEAR(got[2], 100.0f, 100.0f / 65535.0f);
	ASSERT_TRUE(SnapshotCellFile::Dequantise(back, StringCRC("biome"), got, 12));
	EXPECT_EQ(got[0], 255.0f);
}

TEST(SnapshotCellFile, DequantiseUnknownLayerOrWrongSizeFails)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	float out[64];
	EXPECT_FALSE(SnapshotCellFile::Dequantise(f.data, StringCRC("nope"), out, 12));
	EXPECT_FALSE(SnapshotCellFile::Dequantise(f.data, StringCRC("temp"), out, 11));
	EXPECT_FALSE(SnapshotCellFile::Dequantise(f.data, StringCRC("temp"), nullptr, 12));
}

TEST(SnapshotCellFile, BuildFailsWithoutRecordedLayers)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 1, "{\"aggregate\":false}") }));
	r.Start();
	r.Take();
	SnapshotCellData d;
	char err[128];
	EXPECT_FALSE(SnapshotCellFile::BuildFromLive(*r.rec, r.registry, r.schema.service.get(), "x", 1, "", d, err, sizeof(err)));
	EXPECT_NE(err[0], '\0');
}

TEST(SnapshotCellFile, BuildRejectsInvalidName)
{
	Fix f; f.Setup();
	EXPECT_FALSE(f.Build("bad name"));
	EXPECT_NE(f.err[0], '\0');
}

TEST(SnapshotCellFile, AtomicWriteFailureInMissingFolderLeavesNothing)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = std::string(f.store.Folder()) + "/no_such_dir/snapshot_x.cells.json";
	EXPECT_FALSE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	EXPECT_NE(f.err[0], '\0');
	EXPECT_FALSE(fs::exists(path));
	EXPECT_FALSE(fs::exists(path + ".tmp"));
}

TEST(SnapshotCellFile, FailedWriteDoesNotClobberExistingFile_TempBlocked)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	WriteAll(path, "ORIGINAL");
	fs::create_directory(path + ".tmp");	// the temp file cannot be created
	EXPECT_FALSE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	EXPECT_NE(f.err[0], '\0');
	EXPECT_EQ(ReadAll(path), "ORIGINAL");
}

TEST(SnapshotCellFile, RenameFailureRemovesTempFile)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	fs::create_directory(path);	// the target is a non-empty directory: rename cannot replace it
	WriteAll(path + "/keep.txt", "x");
	EXPECT_FALSE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	EXPECT_NE(f.err[0], '\0');
	EXPECT_FALSE(fs::exists(path + ".tmp"));
	EXPECT_TRUE(fs::exists(path + "/keep.txt"));
}

TEST(SnapshotCellFile, SuccessfulWriteReplacesExistingFile)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	WriteAll(path, "OLD");
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	SnapshotCellData back;
	EXPECT_TRUE(SnapshotCellFile::Read(path.c_str(), back, f.err, sizeof(f.err)));
	EXPECT_FALSE(HasTmp(f.store.Folder()));
}

namespace
{
	// Writes a valid file, applies `mutate` to its JSON and expects Read to refuse it with a message containing `expect`.
	template <class Fn>
	void ExpectRejected(const char* expect, Fn mutate)
	{
		Fix f; f.Setup();
		ASSERT_TRUE(f.Build());
		const std::string path = f.PathFor();
		ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
		Json::Value v = LoadJson(path);
		mutate(v);
		SaveJson(path, v);
		SnapshotCellData out;
		char err[256] = {};
		EXPECT_FALSE(SnapshotCellFile::Read(path.c_str(), out, err, sizeof(err)));
		EXPECT_NE(std::strstr(err, expect), nullptr) << "message was: " << err;
		EXPECT_EQ(out.LayerCount(), 0u);
		SnapshotHeader h; SnapshotGridInfo g;
		const bool headerLevel = !std::strcmp(expect, "format") || !std::strcmp(expect, "version") || !std::strcmp(expect, "header")
			|| !std::strcmp(expect, "grid") || !std::strcmp(expect, "runId");
		if (headerLevel)
		{
			EXPECT_FALSE(SnapshotCellFile::ReadHeaderOnly(path.c_str(), h, g, err, sizeof(err)));
		}
	}
}

TEST(SnapshotCellFile, RejectsWrongFormat) { ExpectRejected("format", [](Json::Value& v) { v["format"] = "something.else"; }); }
TEST(SnapshotCellFile, RejectsWrongVersion) { ExpectRejected("version", [](Json::Value& v) { v["version"] = 2; }); }
TEST(SnapshotCellFile, RejectsMissingHeader) { ExpectRejected("header", [](Json::Value& v) { v.removeMember("header"); }); }
TEST(SnapshotCellFile, RejectsMissingGrid) { ExpectRejected("grid", [](Json::Value& v) { v.removeMember("grid"); }); }
TEST(SnapshotCellFile, RejectsMissingLayers) { ExpectRejected("layers", [](Json::Value& v) { v.removeMember("layers"); }); }
TEST(SnapshotCellFile, RejectsBadRunId) { ExpectRejected("runId", [](Json::Value& v) { v["header"]["runId"] = "xyz"; }); }
TEST(SnapshotCellFile, RejectsZeroGrid) { ExpectRejected("grid", [](Json::Value& v) { v["grid"]["width"] = 0; }); }
TEST(SnapshotCellFile, RejectsGridNotMatchingPayload) { ExpectRejected("payload", [](Json::Value& v) { v["grid"]["width"] = 5; }); }
TEST(SnapshotCellFile, RejectsBadBase64) { ExpectRejected("base64", [](Json::Value& v) { v["layers"]["temp"]["data"] = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; }); }
TEST(SnapshotCellFile, RejectsUnknownKind) { ExpectRejected("kind", [](Json::Value& v) { v["layers"]["temp"]["kind"] = "blob"; }); }
TEST(SnapshotCellFile, RejectsFloatLayerWithoutRange) { ExpectRejected("layer", [](Json::Value& v) { v["layers"]["temp"].removeMember("min"); }); }
TEST(SnapshotCellFile, RejectsMoreThan64Layers)
{
	ExpectRejected("layer", [](Json::Value& v)
	{
		const Json::Value proto = v["layers"]["temp"];
		for (int i = 0; i < 65; ++i) { v["layers"][std::string("extra") + std::to_string(i)] = proto; }
	});
}

TEST(SnapshotCellFile, RejectsMissingBadAndTruncatedFiles)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	SnapshotCellData out;
	SnapshotHeader h; SnapshotGridInfo g;
	char err[256] = {};
	EXPECT_FALSE(SnapshotCellFile::Read(path.c_str(), out, err, sizeof(err)));
	EXPECT_NE(err[0], '\0');
	EXPECT_FALSE(SnapshotCellFile::ReadHeaderOnly(path.c_str(), h, g, err, sizeof(err)));

	WriteAll(path, "not json {");
	err[0] = '\0';
	EXPECT_FALSE(SnapshotCellFile::Read(path.c_str(), out, err, sizeof(err)));
	EXPECT_NE(err[0], '\0');

	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	const std::string whole = ReadAll(path);
	WriteAll(path, whole.substr(0, whole.size() / 2));
	err[0] = '\0';
	EXPECT_FALSE(SnapshotCellFile::Read(path.c_str(), out, err, sizeof(err)));
	EXPECT_NE(err[0], '\0');
	EXPECT_EQ(out.LayerCount(), 0u);
}

TEST(SnapshotCellFile, ReadHeaderOnlyReturnsHeaderAndGrid)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build());
	const std::string path = f.PathFor();
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	SnapshotHeader h; SnapshotGridInfo g;
	ASSERT_TRUE(SnapshotCellFile::ReadHeaderOnly(path.c_str(), h, g, f.err, sizeof(f.err))) << f.err;
	EXPECT_STREQ(h.mName, "pre-drought");
	EXPECT_EQ(h.mRunId, f.r.rec->RunId());
	EXPECT_EQ(h.mCreatedUnix, 1792510000);
	EXPECT_STREQ(h.mWorld, "coast_range");
	EXPECT_STREQ(g.mId, "test.stubgrid");
	EXPECT_EQ(g.mWidth, 4u);
	EXPECT_EQ(g.mHeight, 3u);
}

TEST(SnapshotCellFile, EmptyWorldIsAllowed)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Build("n", 5, ""));
	const std::string path = f.PathFor("n");
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(f.data, path.c_str(), f.err, sizeof(f.err)));
	SnapshotCellData back;
	ASSERT_TRUE(SnapshotCellFile::Read(path.c_str(), back, f.err, sizeof(f.err)));
	EXPECT_STREQ(back.mHeader.mWorld, "");
}

TEST(SnapshotCellFile, SixtyFourFloatLayersAtLargeStaysAboutOneMegabyte)
{
	Rig r(104, 64);
	std::vector<std::string> entries;
	for (int i = 0; i < 64; ++i)
	{
		const std::string id = "layer" + std::to_string(i);
		r.grid.AddFloatLayer(id.c_str(), float(i) * 0.5f);
		entries.push_back(FloatEntry(id.c_str(), 0, 100));
	}
	ASSERT_TRUE(r.schema.Load(entries));
	r.cfg.mMaxAggregateLayers = 64;
	r.Start();
	r.Take();
	FakeSnapshotStore store;
	SnapshotCellData d;
	char err[256];
	ASSERT_TRUE(SnapshotCellFile::BuildFromLive(*r.rec, r.registry, r.schema.service.get(), "big", 1, "w", d, err, sizeof(err))) << err;
	EXPECT_EQ(d.LayerCount(), 64u);
	char path[300];
	ASSERT_TRUE(SnapshotCellFile::Path(store.Folder(), "big", path, sizeof(path)));
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(d, path, err, sizeof(err))) << err;
	const uintmax_t bytes = fs::file_size(path);
	RecordProperty("cell_file_bytes_64_layers_large", int(bytes));
	printf("[ INFO ] 64 float layers at 104x64: cell file = %llu bytes\n", (unsigned long long)bytes);
	EXPECT_LT(bytes, 1200000u);	// AC 13 says "about 1 MB"; 64 u16 layers is ~1.14 MB (see task report)
}
