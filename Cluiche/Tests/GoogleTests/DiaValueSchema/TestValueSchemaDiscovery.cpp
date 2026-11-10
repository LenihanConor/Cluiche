#include <gtest/gtest.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/ValueSchemaHealth.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaCore/CRC/StringCRC.h>
#include <string>
#include <string.h>

// Feature 1 task 6: discovery + load of *.schema.json under resolved directories (AC 2, 4, 11).

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
using Dia::Core::StringCRC;

namespace
{
	std::string OneEntry(const char* forFormat, const char* key)
	{
		return SchemaFixtureBuilder().For(forFormat).Entry(key).Build();
	}

	std::string BadFile()
	{
		return SchemaFixtureBuilder().For("bad.format").Entry("k").Set("label", Json::Value(5)).Build();
	}

	bool EndsWith(const char* s, const char* suffix)
	{
		const size_t n = strlen(s), m = strlen(suffix);
		return n >= m && strcmp(s + n - m, suffix) == 0;
	}

	const char* SourceOf(const ValueSchemaRegistry& reg, const char* fullKey)
	{
		const ValueInfo* i = reg.Find(StringCRC(fullKey));
		return i ? i->mSourceFile.AsCStr() : "";
	}
}

TEST(DiaValueSchemaDiscovery, FindsOnlySchemaJsonRecursively)
{
	SchemaTempDirectory dir("FindsOnlySchemaJson");
	dir.Write("a.schema.json", OneEntry("f.a", "x"));
	dir.Write("sub/deeper/b.schema.json", OneEntry("f.b", "y"));
	dir.Write("plain.json", OneEntry("f.c", "z"));
	dir.Write("notes.txt", "hello");
	dir.Write("c.schema.json.bak", OneEntry("f.d", "w"));

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.Discover(), 2u);

	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 2u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_EQ(s.mEntriesLoaded, 2u);
	EXPECT_NE(reg.Find(StringCRC("f.a/x")), nullptr);
	EXPECT_NE(reg.Find(StringCRC("f.b/y")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("f.c/z")), nullptr);
	EXPECT_FALSE(reg.IsPublished());
}

TEST(DiaValueSchemaDiscovery, FileListIsSortedForwardSlashFullPaths)
{
	SchemaTempDirectory dir("SortedPaths");
	dir.Write("b/z.schema.json", OneEntry("f", "1"));
	dir.Write("a/sub/m.schema.json", OneEntry("f", "2"));
	dir.Write("a/z.schema.json", OneEntry("f", "3"));

	SchemaDirectoryLoader loader;
	std::string root = dir.Path();
	for (char& c : root) { if (c == '/') c = '\\'; }	// backslash input still normalises
	loader.AddRoot(root.c_str());
	ASSERT_EQ(loader.Discover(), 3u);
	EXPECT_TRUE(EndsWith(loader.FileAt(0), "/a/sub/m.schema.json")) << loader.FileAt(0);
	EXPECT_TRUE(EndsWith(loader.FileAt(1), "/a/z.schema.json")) << loader.FileAt(1);
	EXPECT_TRUE(EndsWith(loader.FileAt(2), "/b/z.schema.json")) << loader.FileAt(2);
	for (unsigned int i = 0; i < loader.FileCount(); ++i)
	{
		EXPECT_EQ(strchr(loader.FileAt(i), '\\'), nullptr) << loader.FileAt(i);
	}
}

TEST(DiaValueSchemaDiscovery, DuplicateKeyFirstSortedPathWinsRegardlessOfRootOrder)
{
	SchemaTempDirectory dir("DupOrder");
	dir.Write("root_b/k.schema.json", OneEntry("dup.format", "rate"));
	dir.Write("root_a/zz/k.schema.json", OneEntry("dup.format", "rate"));
	const std::string a = dir.Path("root_a"), b = dir.Path("root_b");

	for (int pass = 0; pass < 2; ++pass)
	{
		SchemaDirectoryLoader loader;
		if (pass == 0) { loader.AddRoot(b.c_str()); loader.AddRoot(a.c_str()); }
		else           { loader.AddRoot(a.c_str()); loader.AddRoot(b.c_str()); }

		ValueSchemaRegistry reg;
		const SchemaLoadStats s = loader.LoadInto(reg);
		EXPECT_EQ(s.mFilesLoaded, 2u);
		EXPECT_EQ(s.mEntriesLoaded, 1u);
		EXPECT_EQ(s.mEntriesRejected, 1u);
		EXPECT_EQ(reg.Count(), 1u);
		EXPECT_TRUE(EndsWith(SourceOf(reg, "dup.format/rate"), "/root_a/zz/k.schema.json"))
			<< "pass " << pass << ": " << SourceOf(reg, "dup.format/rate");
	}
}

TEST(DiaValueSchemaDiscovery, SortIsByFullPathNotIterationOrderWithinRoot)
{
	SchemaTempDirectory dir("DupWithinRoot");
	dir.Write("a/z.schema.json", OneEntry("dup2", "k"));
	dir.Write("a/sub/b.schema.json", OneEntry("dup2", "k"));

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	loader.LoadInto(reg);
	EXPECT_TRUE(EndsWith(SourceOf(reg, "dup2/k"), "/a/sub/b.schema.json")) << SourceOf(reg, "dup2/k");
}

TEST(DiaValueSchemaDiscovery, BadFileSkippedOthersLoaded)
{
	SchemaTempDirectory dir("BadSkipped");
	dir.Write("1_good.schema.json", SchemaFixtureBuilder().For("g1").Entry("a").Entry("b").Build());
	dir.Write("2_bad.schema.json", BadFile());
	dir.Write("3_good.schema.json", OneEntry("g2", "c"));
	dir.Write("4_garbage.schema.json", "{ not json");

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 4u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_EQ(s.mFilesFailed, 2u);
	EXPECT_EQ(s.mEntriesLoaded, 3u);
	EXPECT_EQ(reg.Count(), 3u);
	EXPECT_NE(reg.Find(StringCRC("g2/c")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("bad.format/k")), nullptr);
}

TEST(DiaValueSchemaDiscovery, SameFileViaTwoRootsLoadsOnce)
{
	SchemaTempDirectory dir("TwoRoots");
	dir.Write("top.schema.json", OneEntry("t", "a"));
	dir.Write("sub/inner.schema.json", OneEntry("t", "b"));

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	loader.AddRoot(dir.Path("sub").c_str());	// overlapping root
	loader.AddRoot(dir.Path().c_str());			// same root twice
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 2u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_EQ(s.mEntriesRejected, 0u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaDiscovery, MissingRootToleratedWithWarning)
{
	SchemaTempDirectory dir("MissingRoot");
	dir.Write("ok.schema.json", OneEntry("m", "a"));
	const std::string file = dir.Write("not_a_dir.schema.json", OneEntry("m", "b"));

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path("does_not_exist").c_str());
	loader.AddRoot(file.c_str());		// a file, not a directory
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mRootsMissing, 2u);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaDiscovery, NoRootsLoadsNothing)
{
	SchemaDirectoryLoader loader;
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 0u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaHealth, DegradedWhenAnyFileFailed)
{
	SchemaLoadStats s;
	s.mFilesLoaded = 3;
	s.mFilesFailed = 1;
	ValueSchemaHealth h(s);
	EXPECT_EQ(h.GetReporterName(), StringCRC("dia.valueschema"));
	const auto r = h.Report();
	EXPECT_EQ(r.status, Dia::Observation::Health::HealthStatus::kDegraded);
	EXPECT_EQ(r.errors, 1u);
	EXPECT_EQ(r.reason, StringCRC("valueschema.files_failed"));
}

TEST(DiaValueSchemaHealth, OkOnCleanLoad)
{
	SchemaLoadStats s;
	s.mFilesLoaded = 3;
	s.mRootsMissing = 1;
	ValueSchemaHealth h(s);
	const auto r = h.Report();
	EXPECT_EQ(r.status, Dia::Observation::Health::HealthStatus::kOK);
	EXPECT_EQ(r.errors, 0u);
	EXPECT_EQ(r.warnings, 1u);
}
