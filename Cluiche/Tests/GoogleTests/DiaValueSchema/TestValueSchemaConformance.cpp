#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <string>

// Shared conformance fixtures (Feature 1 task 7). The fixture folder is data checked in under
// Dia/DiaValueSchema/Testing/Conformance; it is located by walking up from this source file (__FILE__)
// until Dia/DiaValueSchema/Testing/Conformance/expected.json exists.

using namespace Dia::ValueSchema;
namespace fs = std::filesystem;

namespace
{
	fs::path FindConformanceDir()
	{
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		while (!dir.empty())
		{
			const fs::path candidate = dir / "Dia" / "DiaValueSchema" / "Testing" / "Conformance";
			if (fs::exists(candidate / "expected.json", ec)) { return candidate; }
			const fs::path parent = dir.parent_path();
			if (parent == dir) { break; }
			dir = parent;
		}
		return fs::path();
	}

	bool ReadFile(const fs::path& p, std::string& out)
	{
		std::ifstream in(p, std::ios::binary);
		if (!in) { return false; }
		std::stringstream ss;
		ss << in.rdbuf();
		out = ss.str();
		return true;
	}
}

TEST(DiaValueSchemaConformance, EveryFixtureMatchesExpectedVerdict)
{
	const fs::path dir = FindConformanceDir();
	ASSERT_FALSE(dir.empty()) << "could not locate Dia/DiaValueSchema/Testing/Conformance from " << __FILE__;

	std::string expectedText;
	ASSERT_TRUE(ReadFile(dir / "expected.json", expectedText));
	Json::Value expected;
	Json::Reader reader;
	ASSERT_TRUE(reader.parse(expectedText, expected)) << reader.getFormattedErrorMessages();
	ASSERT_TRUE(expected.isObject());
	EXPECT_EQ(expected["format"].asString(), "dia.value_schema.conformance");
	EXPECT_EQ(expected["version"].asInt(), 1);
	ASSERT_TRUE(expected["cases"].isObject());
	const Json::Value& cases = expected["cases"];

	// Files on disk.
	std::set<std::string> files;
	for (const fs::directory_entry& de : fs::directory_iterator(dir))
	{
		const std::string name = de.path().filename().string();
		const std::string suffix = ".schema.json";
		if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) { files.insert(name); }
	}

	// No drift in either direction.
	for (const std::string& f : files)
	{
		EXPECT_TRUE(cases.isMember(f)) << "fixture " << f << " has no entry in expected.json";
	}
	for (const std::string& name : cases.getMemberNames())
	{
		EXPECT_TRUE(files.count(name) == 1) << "expected.json entry " << name << " has no fixture file";
	}

	int good = 0;
	int bad = 0;
	for (const std::string& name : cases.getMemberNames())
	{
		if (files.count(name) == 0) { continue; }
		SCOPED_TRACE(name);
		const Json::Value& c = cases[name];

		std::string text;
		ASSERT_TRUE(ReadFile(dir / name, text)) << "cannot read " << name;
		const LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), name.c_str());

		ASSERT_TRUE(c.isMember("ok") && c["ok"].isBool());
		if (c["ok"].asBool())
		{
			++good;
			EXPECT_TRUE(r.mOk) << r.mFailure.mEntry.AsCStr() << ": " << r.mFailure.mReason.AsCStr();
			if (c.isMember("entryCount")) { EXPECT_EQ(r.mEntries.Size(), c["entryCount"].asUInt()); }
			if (c.isMember("for")) { EXPECT_STREQ(r.mFor.AsCStr(), c["for"].asCString()); }
			const unsigned expectedWarnings = c.isMember("warnings") ? c["warnings"].asUInt() : 0u;
			EXPECT_EQ(r.mWarnings.Size(), expectedWarnings);
		}
		else
		{
			++bad;
			EXPECT_FALSE(r.mOk);
			EXPECT_EQ(r.mEntries.Size(), 0u);
			EXPECT_STREQ(r.mFailure.mFile.AsCStr(), name.c_str());
			if (c.isMember("failedEntry")) { EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), c["failedEntry"].asCString()); }
			if (c.isMember("reasonContains"))
			{
				EXPECT_NE(std::string(r.mFailure.mReason.AsCStr()).find(c["reasonContains"].asString()), std::string::npos)
					<< "reason was: " << r.mFailure.mReason.AsCStr();
			}
		}
	}

	EXPECT_GE(good, 1) << "need at least one good case";
	EXPECT_GE(bad, 1) << "need at least one bad case";
}

namespace
{
	LoadResult LoadFixture(const char* name)
	{
		const fs::path dir = FindConformanceDir();
		std::string text;
		EXPECT_TRUE(ReadFile(dir / name, text)) << name;
		return ValueSchemaLoader::LoadFromText(text.c_str(), name);
	}
}

TEST(DiaValueSchemaConformance, GoodBandsRoundTripIntoValueInfo)
{
	const LoadResult r = LoadFixture("good-bands.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	ASSERT_EQ(r.mEntries.Size(), 1u);
	const ValueInfo& v = r.mEntries[0];
	ASSERT_EQ(v.mBands.Size(), 2u);
	// jsoncpp orders object members by name, so band order is alphabetical, not file order: look up by name.
	const BandInfo* low = nullptr;
	const BandInfo* high = nullptr;
	for (unsigned i = 0; i < v.mBands.Size(); ++i)
	{
		if (strcmp(v.mBands[i].mName.AsCStr(), "LOW") == 0) { low = &v.mBands[i]; }
		if (strcmp(v.mBands[i].mName.AsCStr(), "HIGH") == 0) { high = &v.mBands[i]; }
	}
	ASSERT_NE(low, nullptr);
	ASSERT_NE(high, nullptr);
	EXPECT_DOUBLE_EQ(low->mLo, 0.0);
	EXPECT_DOUBLE_EQ(low->mHi, 0.3);
	EXPECT_DOUBLE_EQ(high->mLo, 0.7);
	EXPECT_DOUBLE_EQ(high->mHi, 1.0);
}

TEST(DiaValueSchemaConformance, GoodMapDefaultObjectRoundTripsIntoValueInfo)
{
	const LoadResult r = LoadFixture("good-map-default-object.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	const ValueInfo& v = r.mEntries[0];
	EXPECT_TRUE(v.mHasMapDefault);
	ASSERT_EQ(v.mMapDefaults.Size(), 2u);
	EXPECT_STREQ(v.mMapDefaults[0].mMapKey.AsCStr(), "A");
	EXPECT_DOUBLE_EQ(v.mMapDefaults[0].mNumber, 0.5);
	EXPECT_STREQ(v.mMapDefaults[1].mMapKey.AsCStr(), "B");
	EXPECT_DOUBLE_EQ(v.mMapDefaults[1].mNumber, 1.0);
}

TEST(DiaValueSchemaConformance, ScalarDefaultOnMapEntryStillLoads)
{
	const char* text = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"m","kind":"parameter","label":"L","description":"D","type":"map","keyRef":"k","valueType":"float","default":0.25}]})";
	const LoadResult r = ValueSchemaLoader::LoadFromText(text, "scalar-map.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_FALSE(r.mEntries[0].mHasMapDefault);
	EXPECT_TRUE(r.mEntries[0].mHasDefault);
}

TEST(DiaValueSchemaConformance, GoodConstraintsRoundTripInFileOrder)
{
	const LoadResult r = LoadFixture("good-constraints.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	ASSERT_EQ(r.mConstraints.Size(), 2u);
	EXPECT_STREQ(r.mConstraints[0].mA.AsCStr(), "lo");
	EXPECT_EQ(r.mConstraints[0].mOp, ConstraintOp::Le);
	EXPECT_STREQ(r.mConstraints[0].mB.AsCStr(), "hi");
	EXPECT_STREQ(r.mConstraints[1].mA.AsCStr(), "hi");
	EXPECT_EQ(r.mConstraints[1].mOp, ConstraintOp::Ne);
	EXPECT_STREQ(r.mConstraints[1].mB.AsCStr(), "cols.A");
}

TEST(DiaValueSchemaConformance, AllConstraintOpsParse)
{
	const char* ops[] = { "<", "<=", ">", ">=", "==", "!=" };
	const ConstraintOp expect[] = { ConstraintOp::Lt, ConstraintOp::Le, ConstraintOp::Gt, ConstraintOp::Ge, ConstraintOp::Eq, ConstraintOp::Ne };
	for (int i = 0; i < 6; ++i)
	{
		std::string t = std::string(R"({"format":"dia.value_schema","version":1,"for":"t","constraints":[{"a":"x","op":")") + ops[i]
			+ R"(","b":"x"}],"entries":[{"key":"x","kind":"parameter","label":"L","description":"D","type":"float"}]})";
		const LoadResult r = ValueSchemaLoader::LoadFromText(t.c_str(), "ops.schema.json");
		ASSERT_TRUE(r.mOk) << ops[i];
		EXPECT_EQ(r.mConstraints[0].mOp, expect[i]) << ops[i];
	}
}

// Data cases (Conformance/data/*.case.json). Each case carries full dia.value_schema documents under "schemas".
// Loader-level verdict shared with the CLI: every such schema must load (the CLI's schema.* shape rules produce no
// FAIL for them either; pinned in test_validate_schema_parity.py). The data.* findings in `expected` and
// `schemaExpected` (data.out-of-range, data.constraint-violated, data.unresolved-map-key, ...) are implemented by the
// CLI only; the C++ side evaluates them later in AttributeSchema::FromValueSchema / ParameterFile, so they are checked
// on the CLI side only (test_validate_schema_parity.py::test_q1_case_files...). Here the C++ loader must read back
// exactly the structure the case file declares (entry, band, map-default and constraint counts).
TEST(DiaValueSchemaConformance, EveryDataCaseSchemaLoadsAndReadsBackItsDeclaredStructure)
{
	const fs::path dir = FindConformanceDir();
	ASSERT_FALSE(dir.empty());
	const fs::path dataDir = dir / "data";
	int cases = 0;
	int schemas = 0;
	for (const fs::directory_entry& de : fs::directory_iterator(dataDir))
	{
		const std::string name = de.path().filename().string();
		const std::string suffix = ".case.json";
		if (name.size() <= suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) { continue; }
		SCOPED_TRACE(name);
		std::string text;
		ASSERT_TRUE(ReadFile(de.path(), text));
		Json::Value c;
		Json::Reader reader;
		ASSERT_TRUE(reader.parse(text, c)) << reader.getFormattedErrorMessages();
		ASSERT_TRUE(c["schemas"].isArray());
		++cases;
		for (Json::ArrayIndex i = 0; i < c["schemas"].size(); ++i)
		{
			const Json::Value& s = c["schemas"][i];
			Json::FastWriter w;
			const std::string schemaText = w.write(s);
			const LoadResult r = ValueSchemaLoader::LoadFromText(schemaText.c_str(), name.c_str());
			EXPECT_TRUE(r.mOk) << "schemas[" << i << "] " << r.mFailure.mEntry.AsCStr() << ": " << r.mFailure.mReason.AsCStr();
			if (!r.mOk) { continue; }
			++schemas;
			EXPECT_EQ(r.mEntries.Size(), s["entries"].size());
			EXPECT_EQ(r.mConstraints.Size(), s.isMember("constraints") ? s["constraints"].size() : 0u);
			for (Json::ArrayIndex e = 0; e < s["entries"].size(); ++e)
			{
				const Json::Value& je = s["entries"][e];
				const ValueInfo& v = r.mEntries[e];
				EXPECT_STREQ(v.mKey.AsCStr(), je["key"].asCString());
				EXPECT_EQ(v.mBands.Size(), je.isMember("bands") ? je["bands"].size() : 0u);
				const bool objectDefault = je.isMember("default") && je["default"].isObject();
				EXPECT_EQ(v.mHasMapDefault, objectDefault);
				EXPECT_EQ(v.mMapDefaults.Size(), objectDefault ? je["default"].size() : 0u);
			}
		}
	}
	EXPECT_GE(cases, 20);
	EXPECT_GE(schemas, cases);
}

// The CLI is intentionally stricter than the C++ loader on `keyRef`: the loader accepts any string (the key source is
// resolved later), the CLI requires '<format>/<record path>' (schema.ref-malformed). Pinned here and in
// test_validate_schema_parity.py::test_q3_keyref_...; see Conformance/README.md "CLI stricter than the loader".
TEST(DiaValueSchemaConformance, LoaderAcceptsAnyKeyRefStringWhereCliRequiresFormatSlashPath)
{
	const char* text = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"m","kind":"parameter","label":"L","description":"D","type":"map","keyRef":"no-slash-here","valueType":"float"}]})";
	const LoadResult r = ValueSchemaLoader::LoadFromText(text, "keyref.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mEntries[0].mMapKeyRef.AsCStr(), "no-slash-here");
}
