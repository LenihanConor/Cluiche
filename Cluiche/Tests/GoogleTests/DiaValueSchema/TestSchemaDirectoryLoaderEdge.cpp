#include <gtest/gtest.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/ValueSchemaHealth.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaCore/CRC/StringCRC.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <share.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>

// Exhaustive edge tests for SchemaDirectoryLoader (discovery + load) and ValueSchemaHealth.
// PIN tests document current behaviour on purpose; FIX tests were red before the matching source change.

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::SchemaFixtureBuilder;
using Dia::ValueSchema::Testing::SchemaTempDirectory;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace
{
	std::string OneEntry(const char* forFormat, const char* key)
	{
		return SchemaFixtureBuilder().For(forFormat).Entry(key).Build();
	}

	bool EndsWith(const char* s, const char* suffix)
	{
		const size_t n = strlen(s), m = strlen(suffix);
		return n >= m && strcmp(s + n - m, suffix) == 0;
	}

	std::string ReplaceAll(std::string s, const std::string& from, const std::string& to)
	{
		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
		return s;
	}

	std::vector<std::string> FileList(const SchemaDirectoryLoader& l)
	{
		std::vector<std::string> v;
		for (unsigned int i = 0; i < l.FileCount(); ++i) { v.push_back(l.FileAt(i)); }
		return v;
	}

	const char* SourceOf(const ValueSchemaRegistry& reg, const char* fullKey)
	{
		const ValueInfo* i = reg.Find(StringCRC(fullKey));
		return i ? i->mSourceFile.AsCStr() : "";
	}

	void WriteWide(const fs::path& p, const std::string& text)
	{
		fs::create_directories(p.parent_path());
		std::ofstream out(p, std::ios::binary | std::ios::trunc);
		out << text;
	}

	void ExpectInvariant(const SchemaLoadStats& s)
	{
		EXPECT_EQ(s.mFilesFound, s.mFilesLoaded + s.mFilesFailed);
	}
}

// ---- file name rules -------------------------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, EmptyAndWhitespaceOnlyFilesCountFailedOthersLoad)
{
	SchemaTempDirectory dir("DirEdge_Empty");
	dir.Write("a_empty.schema.json", "");
	dir.Write("b_ws.schema.json", "  \r\n\t \n ");
	dir.Write("c_good.schema.json", OneEntry("e.fmt", "k"));

	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 3u);
	EXPECT_EQ(s.mFilesFailed, 2u);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(reg.Count(), 1u);
	ExpectInvariant(s);
}

TEST(DiaValueSchemaDirEdge, NameExactlyTheSuffixIsNotDiscoveredPin)
{
	// PIN: HasSuffix needs at least one character before the suffix.
	SchemaTempDirectory dir("DirEdge_ExactSuffix");
	dir.Write(".schema.json", OneEntry("x.fmt", "k"));
	dir.Write("ok.schema.json", OneEntry("y.fmt", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	EXPECT_TRUE(EndsWith(loader.FileAt(0), "/ok.schema.json"));
}

TEST(DiaValueSchemaDirEdge, SuffixMatchIsCaseSensitivePin)
{
	// PIN: "x.SCHEMA.JSON" is not discovered even though the Windows file system is case-insensitive.
	SchemaTempDirectory dir("DirEdge_Case");
	dir.Write("x.SCHEMA.JSON", OneEntry("x.fmt", "k"));
	dir.Write("y.Schema.Json", OneEntry("y.fmt", "k"));
	dir.Write("z.schema.json", OneEntry("z.fmt", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.Discover(), 1u);
}

TEST(DiaValueSchemaDirEdge, BackupAndLookalikeNamesAreIgnored)
{
	SchemaTempDirectory dir("DirEdge_Lookalike");
	dir.Write("x.schema.json.bak", OneEntry("a", "k"));
	dir.Write("x.schema.jsonx", OneEntry("b", "k"));
	dir.Write("x_schema.json", OneEntry("c", "k"));
	dir.Write("xschema.json", OneEntry("d", "k"));
	dir.Write("x.schema.json ", OneEntry("e", "k"));	// trailing space: Windows strips it, still not a match or still a match - must not crash
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_LE(loader.Discover(), 1u);
}

TEST(DiaValueSchemaDirEdge, DirectoryNamedLikeASchemaFileIsNotAFile)
{
	SchemaTempDirectory dir("DirEdge_DirNamed");
	dir.Write("x.schema.json/inner.schema.json", OneEntry("in.fmt", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	EXPECT_TRUE(EndsWith(loader.FileAt(0), "/x.schema.json/inner.schema.json"));
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaDirEdge, CustomSuffixDiscoversOnlyThatSuffix)
{
	SchemaTempDirectory dir("DirEdge_CustomSuffix");
	dir.Write("a.ramps.json", "{}");
	dir.Write("sub/b.ramps.json", "{}");
	dir.Write("c.schema.json", OneEntry("c", "k"));
	dir.Write("ramps.json", "{}");
	SchemaDirectoryLoader loader(".ramps.json");
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.Discover(), 2u);
}

TEST(DiaValueSchemaDirEdge, NullSuffixFallsBackToSchemaJson)
{
	SchemaTempDirectory dir("DirEdge_NullSuffix");
	dir.Write("a.schema.json", OneEntry("a", "k"));
	dir.Write("a.ramps.json", "{}");
	SchemaDirectoryLoader loader(nullptr);
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	EXPECT_TRUE(EndsWith(loader.FileAt(0), "/a.schema.json"));
}

TEST(DiaValueSchemaDirEdge, EmptySuffixMatchesEveryFilePin)
{
	// PIN: an empty suffix is not special-cased; every regular file matches.
	SchemaTempDirectory dir("DirEdge_EmptySuffix");
	dir.Write("a.txt", "x");
	dir.Write("b", "y");
	SchemaDirectoryLoader loader("");
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.Discover(), 2u);
}

// ---- roots -----------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, EmptyStringAndNullRootsAreMissing)
{
	SchemaDirectoryLoader loader;
	loader.AddRoot("");
	loader.AddRoot(nullptr);
	EXPECT_EQ(loader.RootCount(), 2u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mRootsMissing, 2u);
	EXPECT_EQ(s.mFilesFound, 0u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaDirEdge, FilePathAsRootIsMissingNotLoaded)
{
	SchemaTempDirectory dir("DirEdge_FileRoot");
	const std::string file = dir.Write("only.schema.json", OneEntry("f", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(file.c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mRootsMissing, 1u);
	EXPECT_EQ(s.mFilesFound, 0u);
}

TEST(DiaValueSchemaDirEdge, SameRootTwiceAndNestedRootsLoadEachFileOnce)
{
	SchemaTempDirectory dir("DirEdge_Nested");
	dir.Write("top.schema.json", OneEntry("n", "a"));
	dir.Write("l1/mid.schema.json", OneEntry("n", "b"));
	dir.Write("l1/l2/deep.schema.json", OneEntry("n", "c"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	loader.AddRoot(dir.Path().c_str());
	loader.AddRoot(dir.Path("l1").c_str());
	loader.AddRoot(dir.Path("l1/l2").c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 3u);
	EXPECT_EQ(s.mEntriesLoaded, 3u);
	EXPECT_EQ(s.mEntriesRejected, 0u);
	EXPECT_EQ(s.mRootsMissing, 0u);
}

TEST(DiaValueSchemaDirEdge, TrailingSlashesAndDotSegmentsNormalise)
{
	SchemaTempDirectory dir("DirEdge_Slash");
	dir.Write("sub/a.schema.json", OneEntry("s", "a"));
	SchemaDirectoryLoader plain, slash, backslash, dotted;
	plain.AddRoot(dir.Path().c_str());
	slash.AddRoot((dir.Path() + "/").c_str());
	std::string bs = dir.Path() + "/";
	for (char& c : bs) { if (c == '/') { c = '\\'; } }
	backslash.AddRoot(bs.c_str());
	dotted.AddRoot((dir.Path() + "/sub/../sub/.").c_str());
	ASSERT_EQ(plain.Discover(), 1u);
	ASSERT_EQ(slash.Discover(), 1u);
	ASSERT_EQ(backslash.Discover(), 1u);
	ASSERT_EQ(dotted.Discover(), 1u);
	EXPECT_STREQ(plain.FileAt(0), slash.FileAt(0));
	EXPECT_STREQ(plain.FileAt(0), backslash.FileAt(0));
	EXPECT_STREQ(plain.FileAt(0), dotted.FileAt(0));
}

TEST(DiaValueSchemaDirEdge, RelativeRootResolvesAgainstCurrentDirectory)
{
	SchemaTempDirectory dir("DirEdge_Relative");
	dir.Write("r.schema.json", OneEntry("rel", "k"));
	std::error_code ec;
	const fs::path rel = fs::relative(fs::path(dir.Path()), fs::current_path(), ec);
	if (ec || rel.empty() || rel.is_absolute()) { GTEST_SKIP() << "temp directory not expressible relative to the cwd"; }
	SchemaDirectoryLoader loader;
	const std::string relStr = rel.generic_string();
	loader.AddRoot(relStr.c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	EXPECT_TRUE(fs::path(loader.FileAt(0)).is_absolute());
	SchemaDirectoryLoader abs;
	abs.AddRoot(dir.Path().c_str());
	abs.Discover();
	EXPECT_STREQ(loader.FileAt(0), abs.FileAt(0));
}

TEST(DiaValueSchemaDirEdge, DotRootIsTheCurrentDirectory)
{
	SchemaDirectoryLoader loader;
	loader.AddRoot(".");
	EXPECT_EQ(loader.RootCount(), 1u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mRootsMissing, 0u);	// exists; contents irrelevant
	ExpectInvariant(s);
}

// ---- paths with unusual characters ------------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, PrintfDirectivesInDirectoryAndFileNames)
{
	SchemaTempDirectory dir("DirEdge_%s%d%n");
	const std::string good = dir.Write("a%s%d%n.schema.json", OneEntry("pct", "good"));
	dir.Write("100%/b%n%n.schema.json", "{ broken %s %n");
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 2u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mFilesFailed, 1u);
	EXPECT_STREQ(SourceOf(reg, "pct/good"), good.c_str());
	// A duplicate across '%'-named files must log without formatting the names.
	SchemaTempDirectory dup("DirEdge_%s_dup");
	dup.Write("p%s.schema.json", OneEntry("pd", "k"));
	dup.Write("q%d.schema.json", OneEntry("pd", "k"));
	SchemaDirectoryLoader l2;
	l2.AddRoot(dup.Path().c_str());
	ValueSchemaRegistry r2;
	const SchemaLoadStats s2 = l2.LoadInto(r2);
	EXPECT_EQ(s2.mEntriesLoaded, 1u);
	EXPECT_EQ(s2.mEntriesRejected, 1u);
}

TEST(DiaValueSchemaDirEdge, SpacesInPathsAndNames)
{
	SchemaTempDirectory dir("DirEdge with spaces");
	dir.Write("my folder/my file.schema.json", OneEntry("sp", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_STREQ(SourceOf(reg, "sp/k"), loader.FileAt(0));
}

TEST(DiaValueSchemaDirEdge, LatinSupplementCharactersInPath)
{
	SchemaTempDirectory dir("DirEdge_Latin");
	const fs::path p = fs::path(dir.Path()) / fs::path(std::wstring(L"caf") + wchar_t(0xE9)) / fs::path(std::wstring(L"na") + wchar_t(0xEF) + L"ve.schema.json");
	WriteWide(p, OneEntry("latin", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);	// representable in the ANSI code page
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mFilesFailed, 0u);
}

TEST(DiaValueSchemaDirEdge, NonRepresentableUnicodeNamesNeverCrashPin)
{
	// PIN: a CJK file name cannot be carried in the narrow API; it is skipped silently (or counted failed) but
	// never crashes and never stops sibling files from loading.
	SchemaTempDirectory dir("DirEdge_Cjk");
	dir.Write("ascii.schema.json", OneEntry("cjk", "ascii"));
	WriteWide(fs::path(dir.Path()) / fs::path(std::wstring(2, wchar_t(0x65E5))) / fs::path(std::wstring(2, wchar_t(0x6587)) + L".schema.json"), OneEntry("cjk", "wide"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	const unsigned int n = loader.Discover();
	EXPECT_GE(n, 1u);
	EXPECT_LE(n, 2u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_NE(reg.Find(StringCRC("cjk/ascii")), nullptr);
	ExpectInvariant(s);
}

TEST(DiaValueSchemaDirEdge, PathLongerThan260CharactersNeverCrashesPin)
{
	// PIN: a file below a >260 char path is created through the extended-length prefix. Whether the narrow-path
	// ReadText can open it depends on the OS long-path setting; either way discovery finds it and the loader counts
	// it loaded or failed, never crashing, and the stats invariant holds.
	SchemaTempDirectory dir("DirEdge_Long");
	fs::path p = fs::path(dir.Path());
	while (p.generic_string().size() < 300) { p /= std::string(40, 'd'); }
	std::error_code ec;
	const fs::path ext = fs::path(std::wstring(L"\\\\?\\") + fs::absolute(p).make_preferred().wstring());
	fs::create_directories(ext, ec);
	if (ec) { GTEST_SKIP() << "cannot create a >260 char directory here: " << ec.message(); }
	{
		std::ofstream out(ext / "long.schema.json", std::ios::binary);
		if (!out) { GTEST_SKIP() << "cannot create a >260 char file here"; }
		out << OneEntry("longp", "k");
	}
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_LE(loader.Discover(), 1u);	// 0 when the process is not long-path aware (observed: the walk stops, a Warning is logged)
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_LE(s.mFilesFound, 1u);
	ExpectInvariant(s);
	RecordProperty("long_path_loaded", static_cast<int>(s.mFilesLoaded));
	std::error_code rec;
	fs::remove_all(fs::path(std::wstring(L"\\\\?\\") + fs::absolute(fs::path(dir.Path())).make_preferred().wstring()), rec);
}

// ---- accessors / re-entrancy ----------------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, FileAtOutOfRangeIsEmptyAndFileCountZeroBeforeDiscover)
{
	SchemaTempDirectory dir("DirEdge_Accessors");
	dir.Write("a.schema.json", OneEntry("acc", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.FileCount(), 0u);
	EXPECT_STREQ(loader.FileAt(0), "");
	ASSERT_EQ(loader.Discover(), 1u);
	EXPECT_STRNE(loader.FileAt(0), "");
	EXPECT_STREQ(loader.FileAt(1), "");
	EXPECT_STREQ(loader.FileAt(0xFFFFFFFFu), "");
	EXPECT_NE(loader.FileAt(1), nullptr);
}

TEST(DiaValueSchemaDirEdge, ReversedRootOrderGivesTheSameFileList)
{
	SchemaTempDirectory dir("DirEdge_Reverse");
	dir.Write("a/1.schema.json", OneEntry("r", "1"));
	dir.Write("b/2.schema.json", OneEntry("r", "2"));
	dir.Write("c/3.schema.json", OneEntry("r", "3"));
	SchemaDirectoryLoader fwd, rev;
	for (const char* d : { "a", "b", "c" }) { fwd.AddRoot(dir.Path(d).c_str()); }
	for (const char* d : { "c", "b", "a" }) { rev.AddRoot(dir.Path(d).c_str()); }
	fwd.Discover();
	rev.Discover();
	EXPECT_EQ(FileList(fwd), FileList(rev));
	EXPECT_EQ(fwd.FileCount(), 3u);
}

TEST(DiaValueSchemaDirEdge, DiscoverTwiceResetsInsteadOfAppending)
{
	SchemaTempDirectory dir("DirEdge_Rediscover");
	dir.Write("a.schema.json", OneEntry("rd", "a"));
	dir.Write("b.schema.json", OneEntry("rd", "b"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	loader.AddRoot(dir.Path("missing").c_str());
	EXPECT_EQ(loader.Discover(), 2u);
	EXPECT_EQ(loader.Discover(), 2u);
	EXPECT_EQ(loader.FileCount(), 2u);
	fs::remove(fs::path(dir.Path("b.schema.json")));
	EXPECT_EQ(loader.Discover(), 1u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);	// LoadInto rediscovers again; missing-root count does not accumulate
	EXPECT_EQ(s.mFilesFound, 1u);
	EXPECT_EQ(s.mRootsMissing, 1u);
}

TEST(DiaValueSchemaDirEdge, LoadIntoTwiceIntoTwoRegistriesIsIdentical)
{
	SchemaTempDirectory dir("DirEdge_TwoRegs");
	dir.Write("a.schema.json", SchemaFixtureBuilder().For("tr").Entry("a").Entry("b").Build());
	dir.Write("bad.schema.json", "{ nope");
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry r1, r2;
	const SchemaLoadStats s1 = loader.LoadInto(r1);
	const SchemaLoadStats s2 = loader.LoadInto(r2);
	EXPECT_EQ(s1.mFilesFound, s2.mFilesFound);
	EXPECT_EQ(s1.mFilesLoaded, s2.mFilesLoaded);
	EXPECT_EQ(s1.mFilesFailed, s2.mFilesFailed);
	EXPECT_EQ(s1.mEntriesLoaded, s2.mEntriesLoaded);
	EXPECT_EQ(s1.mEntriesRejected, s2.mEntriesRejected);
	EXPECT_EQ(r1.Count(), r2.Count());
	EXPECT_STREQ(SourceOf(r1, "tr/a"), SourceOf(r2, "tr/a"));
}

TEST(DiaValueSchemaDirEdge, LoadIntoTwiceIntoTheSameRegistryRejectsEveryEntry)
{
	SchemaTempDirectory dir("DirEdge_SameReg");
	dir.Write("a.schema.json", SchemaFixtureBuilder().For("sr").Entry("a").Entry("b").Build());
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats first = loader.LoadInto(reg);
	const SchemaLoadStats second = loader.LoadInto(reg);
	EXPECT_EQ(first.mEntriesLoaded, 2u);
	EXPECT_EQ(second.mEntriesLoaded, 0u);
	EXPECT_EQ(second.mEntriesRejected, 2u);
	EXPECT_EQ(second.mFilesLoaded, 1u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaDirEdge, LoadIntoPublishedRegistryRejectsEntriesButFilesStillLoad)
{
	SchemaTempDirectory dir("DirEdge_Published");
	dir.Write("a.schema.json", OneEntry("pub", "a"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	reg.Publish();
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mEntriesLoaded, 0u);
	EXPECT_EQ(s.mEntriesRejected, 1u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaDirEdge, StatsInvariantAcrossMixedDirectory)
{
	SchemaTempDirectory dir("DirEdge_Invariant");
	dir.Write("1.schema.json", SchemaFixtureBuilder().For("inv").Entry("a").Entry("b").Build());
	dir.Write("2.schema.json", SchemaFixtureBuilder().For("inv").Entry("a").Entry("c").Build());	// 'a' duplicate
	dir.Write("3.schema.json", "{ broken");
	dir.Write("4.schema.json", "");
	dir.Write("5.schema.json", SchemaFixtureBuilder().For("inv2").Entry("z").Build());
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 5u);
	EXPECT_EQ(s.mFilesLoaded, 3u);
	EXPECT_EQ(s.mFilesFailed, 2u);
	EXPECT_EQ(s.mEntriesLoaded, 4u);
	EXPECT_EQ(s.mEntriesRejected, 1u);
	ExpectInvariant(s);
}

// ---- on-disk encodings (FIX 1: BOM / CRLF) ------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, Utf8BomFileOnDiskLoads)
{
	SchemaTempDirectory dir("DirEdge_Bom");
	dir.Write("bom.schema.json", std::string("\xEF\xBB\xBF") + OneEntry("bom", "k"));
	dir.Write("plain.schema.json", OneEntry("plain", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_NE(reg.Find(StringCRC("bom/k")), nullptr);
}

TEST(DiaValueSchemaDirEdge, BomOnlyFileCountsFailed)
{
	SchemaTempDirectory dir("DirEdge_BomOnly");
	dir.Write("bom.schema.json", "\xEF\xBB\xBF");
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFailed, 1u);
}

TEST(DiaValueSchemaDirEdge, CrlfLineEndingsOnDiskLoad)
{
	SchemaTempDirectory dir("DirEdge_Crlf");
	const std::string crlf = ReplaceAll(ReplaceAll(OneEntry("crlf", "k"), ",", ",\r\n"), "{", "{\r\n");
	ASSERT_NE(crlf.find("\r\n"), std::string::npos);
	dir.Write("crlf.schema.json", crlf);
	dir.Write("bomcrlf.schema.json", std::string("\xEF\xBB\xBF") + ReplaceAll(OneEntry("crlf2", "k"), ",", ",\r\n"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFailed, 0u);
	EXPECT_EQ(s.mFilesLoaded, 2u);
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaDirEdge, NulBytesInFileTruncateAtFirstNulPin)
{
	// PIN: the file text is handed over as a C string, so bytes after the first NUL are ignored.
	SchemaTempDirectory dir("DirEdge_Nul");
	dir.Write("a.schema.json", OneEntry("nul", "k") + std::string(1, '\0') + "garbage { not json");
	dir.Write("b.schema.json", std::string(64, '\0'));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_EQ(s.mFilesFailed, 1u);
	EXPECT_NE(reg.Find(StringCRC("nul/k")), nullptr);
}

TEST(DiaValueSchemaDirEdge, InvalidUtf8BytesDoNotCrash)
{
	SchemaTempDirectory dir("DirEdge_BadUtf8");
	dir.Write("a.schema.json", "\xFF\xFE\xC0\x80 not json \xED\xA0\x80");
	dir.Write("b.schema.json", OneEntry("u8", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFailed, 1u);
	EXPECT_EQ(s.mFilesLoaded, 1u);
}

TEST(DiaValueSchemaDirEdge, LargeFileWithLeadingWhitespaceLoads)
{
	SchemaTempDirectory dir("DirEdge_BigFile");
	dir.Write("big.schema.json", std::string(2 * 1024 * 1024, ' ') + OneEntry("big", "k"));
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	EXPECT_EQ(loader.LoadInto(reg).mFilesLoaded, 1u);
}

TEST(DiaValueSchemaDirEdge, UnreadableLockedFileCountsFailedOthersLoad)
{
	SchemaTempDirectory dir("DirEdge_Locked");
	const std::string locked = dir.Write("a_locked.schema.json", OneEntry("lk", "a"));
	dir.Write("b_free.schema.json", OneEntry("lk", "b"));
	FILE* hold = _fsopen(locked.c_str(), "rb", _SH_DENYRW);	// exclusive: no other open for read
	if (!hold) { GTEST_SKIP() << "cannot take an exclusive handle"; }
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	fclose(hold);
	EXPECT_EQ(s.mFilesFound, 2u);
	EXPECT_EQ(s.mFilesFailed, 1u);
	EXPECT_EQ(s.mFilesLoaded, 1u);
	EXPECT_NE(reg.Find(StringCRC("lk/b")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("lk/a")), nullptr);
}

// ---- links ------------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaDirEdge, FileSymlinkToSameFileIsLoadedOncePin)
{
	// PIN: files are de-duplicated by canonical path, so a symlink to a file in the same root is not a second file.
	SchemaTempDirectory dir("DirEdge_FileLink");
	dir.Write("real.schema.json", OneEntry("lnk", "k"));
	std::error_code ec;
	fs::create_symlink(fs::path(dir.Path("real.schema.json")), fs::path(dir.Path("link.schema.json")), ec);
	if (ec) { GTEST_SKIP() << "symlinks not permitted: " << ec.message(); }
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	ASSERT_EQ(loader.Discover(), 1u);
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mEntriesRejected, 0u);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaDirEdge, DirectorySymlinkIsNotFollowedPin)
{
	// PIN: the recursive walk does not follow directory symlinks (no cycles, no double loads).
	SchemaTempDirectory dir("DirEdge_DirLink");
	dir.Write("real/a.schema.json", OneEntry("dl", "k"));
	std::error_code ec;
	fs::create_directory_symlink(fs::path(dir.Path("real")), fs::path(dir.Path("alias")), ec);
	if (ec) { GTEST_SKIP() << "symlinks not permitted: " << ec.message(); }
	fs::create_directory_symlink(fs::path(dir.Path()), fs::path(dir.Path("real/loop")), ec);	// cycle, best effort
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	EXPECT_EQ(loader.Discover(), 1u);
}

// ---- stress -----------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaDiscoveryStress, FiveHundredFilesSortedUniqueAndAllLoaded)
{
	SchemaTempDirectory dir("DirEdge_Stress500");
	for (int i = 0; i < 500; ++i)
	{
		char rel[64], fmt[32];
		snprintf(rel, sizeof(rel), "d%d/f%03d.schema.json", i % 7, i);
		snprintf(fmt, sizeof(fmt), "stress.f%d", i);
		dir.Write(rel, OneEntry(fmt, "k"));
	}
	SchemaDirectoryLoader loader;
	loader.AddRoot(dir.Path().c_str());
	loader.AddRoot(dir.Path("d3").c_str());	// overlap
	ASSERT_EQ(loader.Discover(), 500u);
	for (unsigned int i = 1; i < loader.FileCount(); ++i)
	{
		EXPECT_LT(std::string(loader.FileAt(i - 1)), std::string(loader.FileAt(i)));
	}
	ValueSchemaRegistry reg;
	const SchemaLoadStats s = loader.LoadInto(reg);
	EXPECT_EQ(s.mFilesFound, 500u);
	EXPECT_EQ(s.mFilesLoaded, 500u);
	EXPECT_EQ(s.mEntriesLoaded, 500u);
	EXPECT_EQ(reg.Count(), 500u);
	ExpectInvariant(s);
}

// ---- ValueSchemaHealth ----------------------------------------------------------------------------------------------

namespace
{
	using Dia::Observation::Health::HealthStatus;
}

TEST(DiaValueSchemaHealthEdge, ReporterNameIsFixed)
{
	SchemaLoadStats s;
	ValueSchemaHealth h(s);
	EXPECT_EQ(h.GetReporterName(), StringCRC("dia.valueschema"));
	RampLoadStats r;
	ValueSchemaHealth h2(s, &r);
	EXPECT_EQ(h2.GetReporterName(), StringCRC("dia.valueschema"));
}

TEST(DiaValueSchemaHealthEdge, WarningsMirrorMissingRootsAndStayOk)
{
	SchemaLoadStats s;
	s.mRootsMissing = 3;
	ValueSchemaHealth h(s);
	const auto r = h.Report();
	EXPECT_EQ(r.warnings, 3u);
	EXPECT_EQ(r.errors, 0u);
	EXPECT_EQ(r.status, HealthStatus::kOK);
	EXPECT_EQ(r.reason, StringCRC::kZero);
}

TEST(DiaValueSchemaHealthEdge, RampFilesFailedAloneIsDegradedWithRampReason)
{
	SchemaLoadStats s;
	RampLoadStats rs;
	rs.mFilesFailed = 2;
	ValueSchemaHealth h(s, &rs);
	const auto r = h.Report();
	EXPECT_EQ(r.status, HealthStatus::kDegraded);
	EXPECT_EQ(r.reason, StringCRC("valueschema.ramp_files_failed"));
	EXPECT_EQ(r.errors, 2u);
}

TEST(DiaValueSchemaHealthEdge, BothFailingKeepsSchemaReasonAndSumsErrors)
{
	SchemaLoadStats s;
	s.mFilesFailed = 1;
	RampLoadStats rs;
	rs.mFilesFailed = 4;
	ValueSchemaHealth h(s, &rs);
	const auto r = h.Report();
	EXPECT_EQ(r.status, HealthStatus::kDegraded);
	EXPECT_EQ(r.reason, StringCRC("valueschema.files_failed"));
	EXPECT_EQ(r.errors, 5u);
}

TEST(DiaValueSchemaHealthEdge, NullRampStatsBehavesAsZero)
{
	SchemaLoadStats s;
	s.mFilesFailed = 1;
	ValueSchemaHealth h(s, nullptr);
	EXPECT_EQ(h.Report().errors, 1u);
	SchemaLoadStats clean;
	ValueSchemaHealth h2(clean, nullptr);
	EXPECT_EQ(h2.Report().status, HealthStatus::kOK);
}

TEST(DiaValueSchemaHealthEdge, RejectedRampsAndEntriesDoNotDegradePin)
{
	// PIN: only whole-file failures degrade health; rejected entries / ramps are not errors.
	SchemaLoadStats s;
	s.mEntriesRejected = 9;
	RampLoadStats rs;
	rs.mRampsRejected = 9;
	ValueSchemaHealth h(s, &rs);
	const auto r = h.Report();
	EXPECT_EQ(r.status, HealthStatus::kOK);
	EXPECT_EQ(r.errors, 0u);
}

TEST(DiaValueSchemaHealthEdge, ReportTracksLiveStatsByReferencePin)
{
	// PIN: the reporter holds references, so later changes to the stats show in the next Report().
	SchemaLoadStats s;
	RampLoadStats rs;
	ValueSchemaHealth h(s, &rs);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
	rs.mFilesFailed = 1;
	EXPECT_EQ(h.Report().status, HealthStatus::kDegraded);
	s.mFilesFailed = 1;
	EXPECT_EQ(h.Report().reason, StringCRC("valueschema.files_failed"));
	s.mFilesFailed = 0;
	rs.mFilesFailed = 0;
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
	EXPECT_EQ(h.Report().reason, StringCRC::kZero);
}
