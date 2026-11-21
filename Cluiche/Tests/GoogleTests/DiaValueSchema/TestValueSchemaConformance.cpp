#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValuesFromResolver.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <vector>
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

TEST(DiaValueSchemaConformance, BandSchemaVersionIsReadFromTheFileAndDefaultsToOne)
{
	const LoadResult withField = LoadFixture("good-band-schema-version.schema.json");
	ASSERT_TRUE(withField.mOk) << withField.mFailure.mReason.AsCStr();
	EXPECT_EQ(withField.mBandSchemaVersion, 3u);

	const LoadResult absent = LoadFixture("good-bands.schema.json");
	ASSERT_TRUE(absent.mOk) << absent.mFailure.mReason.AsCStr();
	EXPECT_EQ(absent.mBandSchemaVersion, 1u);
}

TEST(DiaValueSchemaConformance, BandSchemaVersionRejectsOutOfRangeAndNonIntegers)
{
	const char* const bad[] = { "0", "-1", "1.5", "\"2\"", "true", "null", "[]", "2147483648" };
	for (const char* v : bad)
	{
		const std::string text = std::string("{\"format\":\"dia.value_schema\",\"version\":1,\"bandSchemaVersion\":") + v + ",\"for\":\"x\",\"entries\":[]}";
		const LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), "f");
		EXPECT_FALSE(r.mOk) << v;
		EXPECT_NE(std::string(r.mFailure.mReason.AsCStr()).find("bandSchemaVersion"), std::string::npos) << v;
	}
	const std::string max = "{\"format\":\"dia.value_schema\",\"version\":1,\"bandSchemaVersion\":2147483647,\"for\":\"x\",\"entries\":[]}";
	const LoadResult ok = ValueSchemaLoader::LoadFromText(max.c_str(), "f");
	ASSERT_TRUE(ok.mOk);
	EXPECT_EQ(ok.mBandSchemaVersion, 2147483647u);
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

TEST(DiaValueSchemaConformance, GoodEnumMapDefaultKeepsEnumIdsInObjectOrder)
{
	const LoadResult r = LoadFixture("good-map-default-enum.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	const ValueInfo& v = r.mEntries[0];
	EXPECT_EQ(v.mMapValueType, ValueType::Enum);
	EXPECT_TRUE(v.mHasMapDefault);
	ASSERT_EQ(v.mMapDefaults.Size(), 2u);
	EXPECT_STREQ(v.mMapDefaults[0].mMapKey.AsCStr(), "A");
	EXPECT_STREQ(v.mMapDefaults[0].mString.AsCStr(), "Calm");
	EXPECT_STREQ(v.mMapDefaults[1].mMapKey.AsCStr(), "B");
	EXPECT_STREQ(v.mMapDefaults[1].mString.AsCStr(), "Stormy");
}

TEST(DiaValueSchemaConformance, EnumMapDefaultFailuresNameTheEntryAndTheMapKey)
{
	const LoadResult unknown = LoadFixture("bad-map-default-enum-unknown-id.schema.json");
	ASSERT_FALSE(unknown.mOk);
	EXPECT_STREQ(unknown.mFailure.mEntry.AsCStr(), "k");
	const std::string reason = unknown.mFailure.mReason.AsCStr();
	EXPECT_EQ(reason.find("map default 'B'"), 0u) << reason;
	EXPECT_NE(reason.find("Gale"), std::string::npos) << reason;

	const LoadResult number = LoadFixture("bad-map-default-enum-number.schema.json");
	ASSERT_FALSE(number.mOk);
	EXPECT_STREQ(number.mFailure.mEntry.AsCStr(), "k");
	EXPECT_EQ(std::string(number.mFailure.mReason.AsCStr()).find("map default 'B'"), 0u) << number.mFailure.mReason.AsCStr();
}

TEST(DiaValueSchemaConformance, MapDefaultsAreCheckedAgainstTheMapValueTypeWhateverTheKeyOrder)
{
	// `default` may precede `valueType` in the JSON text; the value type still decides how it is read.
	const char* boolMap = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"m","kind":"parameter","label":"L","description":"D","default":{"A":true,"B":false},"type":"map","keyRef":"k","valueType":"bool"}]})";
	const LoadResult b = ValueSchemaLoader::LoadFromText(boolMap, "bool-map.schema.json");
	ASSERT_TRUE(b.mOk) << b.mFailure.mReason.AsCStr();
	EXPECT_DOUBLE_EQ(b.mEntries[0].mMapDefaults[0].mNumber, 1.0);
	EXPECT_DOUBLE_EQ(b.mEntries[0].mMapDefaults[1].mNumber, 0.0);

	const char* intMap = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"m","kind":"parameter","label":"L","description":"D","type":"map","keyRef":"k","valueType":"int","default":{"A":2.5}}]})";
	const LoadResult i = ValueSchemaLoader::LoadFromText(intMap, "int-map.schema.json");
	ASSERT_FALSE(i.mOk);
	EXPECT_NE(std::string(i.mFailure.mReason.AsCStr()).find("whole number"), std::string::npos) << i.mFailure.mReason.AsCStr();

	const char* strMap = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"m","kind":"parameter","label":"L","description":"D","type":"map","keyRef":"k","valueType":"string","default":{"A":"x"}}]})";
	const LoadResult sm = ValueSchemaLoader::LoadFromText(strMap, "string-map.schema.json");
	ASSERT_TRUE(sm.mOk) << sm.mFailure.mReason.AsCStr();
	EXPECT_STREQ(sm.mEntries[0].mMapDefaults[0].mString.AsCStr(), "x");
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

// Close-out gap 2: a `kind: parameter` entry keyed format, version or basedOn can never be set by a pack (the pack
// envelope owns those keys), so the loader rejects it naming the entry. Other kinds are unaffected. Mirrored by
// schema.reserved-key in the CLI and the bad-reserved-key-* fixtures.
TEST(DiaValueSchemaConformance, ReservedParameterKeysAreRejectedNamingTheEntry)
{
	for (const char* key : { "format", "version", "basedOn" })
	{
		SCOPED_TRACE(key);
		const std::string text = std::string(R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":")") + key
			+ R"(","kind":"parameter","label":"L","description":"D","type":"float"}]})";
		const LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), "reserved.schema.json");
		EXPECT_FALSE(r.mOk);
		EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), key);
		EXPECT_NE(std::string(r.mFailure.mReason.AsCStr()).find(key), std::string::npos) << r.mFailure.mReason.AsCStr();
		EXPECT_EQ(r.mEntries.Size(), 0u);
	}
}

TEST(DiaValueSchemaConformance, ReservedKeyIsOnlyReservedForParameterKindAndExactMatch)
{
	for (const char* kind : { "field", "metric", "arg", "command", "component_field" })
	{
		for (const char* key : { "format", "version", "basedOn" })
		{
			SCOPED_TRACE(std::string(kind) + "/" + key);
			const std::string text = std::string(R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":")") + key
				+ R"(","kind":")" + kind + R"(","label":"L","description":"D","type":"float"}]})";
			EXPECT_TRUE(ValueSchemaLoader::LoadFromText(text.c_str(), "reserved-ok.schema.json").mOk);
		}
	}
	for (const char* key : { "Format", "versions", "basedon", "format.x", "x.version", "basedOn2" })
	{
		SCOPED_TRACE(key);
		const std::string text = std::string(R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":")") + key
			+ R"(","kind":"parameter","label":"L","description":"D","type":"float"}]})";
		EXPECT_TRUE(ValueSchemaLoader::LoadFromText(text.c_str(), "near-reserved.schema.json").mOk);
	}
}


// ---- Biome Registry plan task 15: C++ <-> CLI parity over the new data fixtures ------------------------------------
// The CLI verdict is read from each case's `schemaExpected` rows; C++ must reach the same verdict. A case the CLI accepts
// and C++ rejects (or the reverse) fails here, so neither side can drift without a fixture change.

namespace
{
	bool ParseFile(const fs::path& p, Json::Value& out)
	{
		std::string text;
		if (!ReadFile(p, text)) { return false; }
		Json::Reader reader;
		return reader.parse(text, out);
	}

	std::vector<fs::path> DataCases()
	{
		std::vector<fs::path> out;
		const fs::path dir = FindConformanceDir();
		for (const fs::directory_entry& de : fs::directory_iterator(dir / "data"))
		{
			const std::string name = de.path().filename().string();
			const std::string suffix = ".case.json";
			if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) { out.push_back(de.path()); }
		}
		return out;
	}

	bool HasValuesFromEntry(const Json::Value& c)
	{
		for (Json::ArrayIndex i = 0; i < c["schemas"].size(); ++i)
			for (Json::ArrayIndex e = 0; e < c["schemas"][i]["entries"].size(); ++e)
				if (c["schemas"][i]["entries"][e].isMember("valuesFrom")) { return true; }
		return false;
	}

	bool CliFlagsValuesFrom(const Json::Value& c)
	{
		for (Json::ArrayIndex i = 0; i < c["schemaExpected"].size(); ++i)
			if (c["schemaExpected"][i]["rule"].asString().rfind("schema.values-from-", 0) == 0) { return true; }
		return false;
	}
}

TEST(DiaValueSchemaConformance, ValuesFromCasesResolveInCppExactlyWhenTheCliResolves)
{
	int covered = 0;
	int cliBad = 0;
	int cliClean = 0;
	for (const fs::path& casePath : DataCases())
	{
		Json::Value c;
		ASSERT_TRUE(ParseFile(casePath, c)) << casePath;
		if (!HasValuesFromEntry(c)) { continue; }
		const std::string name = casePath.filename().string();
		SCOPED_TRACE(name);
		++covered;

		Dia::ValueSchema::Testing::SchemaTempDirectory dir("ConfValuesFrom");
		std::vector<std::string> candidates;
		for (Json::ArrayIndex f = 0; f < c["files"].size(); ++f)
		{
			Json::FastWriter w;
			candidates.push_back(dir.Write(("d" + std::to_string(f) + ".json").c_str(), w.write(c["files"][f]["content"])));
		}

		const bool cliBadVerdict = CliFlagsValuesFrom(c);
		(cliBadVerdict ? cliBad : cliClean) += 1;
		for (Json::ArrayIndex i = 0; i < c["schemas"].size(); ++i)
		{
			Json::FastWriter w;
			const std::string schemaText = w.write(c["schemas"][i]);
			LoadResult r = ValueSchemaLoader::LoadFromText(schemaText.c_str(), name.c_str());
			ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
			for (unsigned int e = 0; e < r.mEntries.Size(); ++e)
			{
				ValueInfo& entry = r.mEntries[e];
				if (!entry.mHasValuesFrom) { continue; }
				std::string reason;
				const bool cppResolved = ResolveValuesFrom(entry, candidates, reason);
				EXPECT_EQ(cppResolved, !cliBadVerdict) << "C++ " << (cppResolved ? "resolved" : "failed: " + reason) << " but the CLI verdict is " << (cliBadVerdict ? "bad" : "clean");
				if (cppResolved)
				{
					// The ids come out in record order: the same list the CLI checks data values against.
					const Json::Value& recs = c["files"][0u]["content"][entry.mValuesFrom.mRecords.AsCStr()];
					ASSERT_EQ(entry.mEnumValues.Size(), recs.size());
					for (Json::ArrayIndex k = 0; k < recs.size(); ++k)
					{
						EXPECT_STREQ(entry.mEnumValues[k].mId.AsCStr(), recs[k][entry.mValuesFrom.mId.AsCStr()].asCString());
					}
				}
			}
		}
	}
	EXPECT_GE(covered, 6) << "the valuesFrom data fixtures went missing";
	EXPECT_GE(cliBad, 5);
	EXPECT_GE(cliClean, 1);
}

// enum[] and ref[2][] (plan tasks 7a/7b): C++ has no data-side validation for these types (the CLI reports
// data.bad-enum / data.wrong-type / data.unresolved-ref), so the shared verdict is the loader's: every data case
// declaring them loads and reads each entry back with the declared ValueType.
TEST(DiaValueSchemaConformance, EveryDataCaseEntryReadsBackWithItsDeclaredType)
{
	struct TypeRow { const char* text; ValueType type; };
	const TypeRow rows[] = {
		{ "float", ValueType::Float }, { "int", ValueType::Int }, { "bool", ValueType::Bool }, { "enum", ValueType::Enum },
		{ "string", ValueType::String }, { "ref", ValueType::Ref }, { "ref[]", ValueType::RefList }, { "map", ValueType::Map },
		{ "enum[]", ValueType::EnumList }, { "ref[2][]", ValueType::RefPairList } };
	int enumLists = 0;
	int pairLists = 0;
	for (const fs::path& casePath : DataCases())
	{
		Json::Value c;
		ASSERT_TRUE(ParseFile(casePath, c)) << casePath;
		const std::string name = casePath.filename().string();
		SCOPED_TRACE(name);
		for (Json::ArrayIndex i = 0; i < c["schemas"].size(); ++i)
		{
			Json::FastWriter w;
			const std::string schemaText = w.write(c["schemas"][i]);
			const LoadResult r = ValueSchemaLoader::LoadFromText(schemaText.c_str(), name.c_str());
			ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
			for (Json::ArrayIndex e = 0; e < c["schemas"][i]["entries"].size(); ++e)
			{
				const std::string declared = c["schemas"][i]["entries"][e]["type"].asString();
				bool known = false;
				for (const TypeRow& row : rows)
				{
					if (declared != row.text) { continue; }
					known = true;
					EXPECT_EQ(r.mEntries[e].mType, row.type) << declared;
					if (row.type == ValueType::EnumList) { ++enumLists; }
					if (row.type == ValueType::RefPairList) { ++pairLists; }
				}
				EXPECT_TRUE(known) << "type '" << declared << "' is not in the parity table: add it with its ValueType";
			}
		}
	}
	EXPECT_GE(enumLists, 3);
	EXPECT_GE(pairLists, 4);
}
