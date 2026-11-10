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
