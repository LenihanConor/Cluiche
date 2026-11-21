// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC7, default file paths)
// WorldFiles: name validation, atomic writes, out/deployed lookup, size caps, round trip through a real file.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldFiles.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
namespace fs = std::filesystem;

namespace
{

// A fresh temp directory per test, removed afterwards.
class WorldFilesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{ 0 };
        mRoot = fs::temp_directory_path() / ("cow_worldfiles_test_" + std::to_string(counter.fetch_add(1)) + "_" +
                                              std::to_string(static_cast<unsigned long long>(::testing::UnitTest::GetInstance()->random_seed())) +
                                              std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFF));
        std::error_code ec;
        fs::remove_all(mRoot, ec);
        fs::create_directories(mRoot, ec);
    }
    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(mRoot, ec);
    }

    std::string Dir(const char* leaf) const { return (mRoot / leaf).string(); }

    static std::string ReadAll(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    static int CountFiles(const std::string& dir)
    {
        int n = 0;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(dir, ec)) { (void)e; ++n; }
        return n;
    }

    fs::path mRoot;
};

} // namespace

TEST(WorldFileNames, ValidNamesAreLettersDigitsUnderscoreAndDashUpToSixtyFourCharacters)
{
    for (const char* ok : { "a", "Z", "0", "world_1", "seed-123", "12345", "My_World-2", "x_" })
    {
        EXPECT_TRUE(IsValidWorldName(ok)) << ok;
    }
    EXPECT_TRUE(IsValidWorldName(std::string(64, 'a')));
    EXPECT_FALSE(IsValidWorldName(std::string(65, 'a')));
}

TEST(WorldFileNames, SeparatorsDotsDrivesSpacesAndControlCharactersAreRefused)
{
    for (const char* bad : { "", ".", "..", "a/b", "a\\b", "../x", "..\\x", "/abs", "C:", "C:x", "a.b", "a.wld.json", "a b", " a", "a ",
                             "a:b", "a*", "a?", "a\"", "a<", "a|", "~", "a~1", "\xC3\xA9", "a\tb", "a\nb" })
    {
        EXPECT_FALSE(IsValidWorldName(bad)) << bad;
    }
    EXPECT_FALSE(IsValidWorldName(std::string("a\0b", 3)));
    EXPECT_FALSE(IsValidWorldName(std::string("\0", 1)));
}

TEST(WorldFileNames, WindowsDeviceNamesAreRefusedInAnyCase)
{
    for (const char* bad : { "CON", "con", "Con", "PRN", "AUX", "NUL", "nul", "COM1", "com9", "LPT1", "lpt9" })
    {
        EXPECT_FALSE(IsValidWorldName(bad)) << bad;
    }
    // Near misses are fine.
    for (const char* ok : { "COM", "COM0", "COM10", "LPT", "CONSOLE", "NULL", "AUXILIARY" })
    {
        EXPECT_TRUE(IsValidWorldName(ok)) << ok;
    }
}

TEST(WorldFileNames, AC7_DefaultNameIsTheSeedAndThePathAddsTheExtension)
{
    EXPECT_EQ(DefaultWorldName(12345), "12345");
    EXPECT_EQ(DefaultWorldName(0), "0");
    EXPECT_EQ(DefaultWorldName(0xFFFFFFFFFFFFFFFFull), "18446744073709551615");
    EXPECT_TRUE(IsValidWorldName(DefaultWorldName(0xFFFFFFFFFFFFFFFFull)));
    const std::string path = WorldFilePath((fs::path("Cluiche") / "out" / "CoW" / "worlds").string(), DefaultWorldName(12345));
    EXPECT_EQ(fs::path(path).filename().string(), "12345.wld.json");
    EXPECT_EQ(fs::path(path).parent_path().filename().string(), "worlds");
}

TEST_F(WorldFilesTest, WriteCreatesTheDirectoryAndWritesTheExactBytes)
{
    const std::string dir = Dir("a/b/worlds");   // does not exist yet
    const std::string text = std::string("line one\r\nline two\n") + std::string(1, '\0') + "tail";   // CRLF, LF and a NUL survive
    const WorldFileResult r = WriteWorldFile(dir, "w1", text, false);
    ASSERT_TRUE(r.Ok()) << r.message;
    EXPECT_EQ(r.bytes, text.size());
    EXPECT_EQ(fs::path(r.path).filename().string(), "w1.wld.json");
    EXPECT_EQ(ReadAll(r.path), text);
    EXPECT_EQ(CountFiles(dir), 1) << "no temporary file may remain";
}

TEST_F(WorldFilesTest, WriteRefusesToOverwriteUnlessAskedAndKeepsTheOldFile)
{
    const std::string dir = Dir("worlds");
    ASSERT_TRUE(WriteWorldFile(dir, "w", "old", false).Ok());
    const WorldFileResult refused = WriteWorldFile(dir, "w", "new", false);
    EXPECT_EQ(refused.status, WorldFileStatus::AlreadyExists);
    EXPECT_EQ(ReadAll(WorldFilePath(dir, "w")), "old");
    ASSERT_TRUE(WriteWorldFile(dir, "w", "new", true).Ok());
    EXPECT_EQ(ReadAll(WorldFilePath(dir, "w")), "new");
    EXPECT_EQ(CountFiles(dir), 1);
}

TEST_F(WorldFilesTest, FailedReplaceLeavesNoTemporaryFileAndNoDamage)
{
    const std::string dir = Dir("worlds");
    // The target name is occupied by a directory, so the final rename cannot succeed.
    fs::create_directories(fs::path(WorldFilePath(dir, "blocked")));
    const WorldFileResult r = WriteWorldFile(dir, "blocked", "data", true);
    EXPECT_EQ(r.status, WorldFileStatus::IoError);
    EXPECT_FALSE(r.message.empty());
    EXPECT_TRUE(fs::is_directory(WorldFilePath(dir, "blocked")));
    EXPECT_EQ(CountFiles(dir), 1) << "only the blocking directory; the temp file must be cleaned up";
}

TEST_F(WorldFilesTest, WriteIntoAPathUnderAFileFails)
{
    const std::string file = Dir("afile");
    { std::ofstream f(file); f << "x"; }
    const WorldFileResult r = WriteWorldFile(file + "/sub", "w", "data", false);
    EXPECT_EQ(r.status, WorldFileStatus::IoError);
    EXPECT_FALSE(r.message.empty());
}

TEST_F(WorldFilesTest, WriteRejectsBadNamesNoDirectoryAndOversizeText)
{
    EXPECT_EQ(WriteWorldFile(Dir("w"), "../evil", "x", true).status, WorldFileStatus::InvalidName);
    EXPECT_EQ(WriteWorldFile(Dir("w"), "NUL", "x", true).status, WorldFileStatus::InvalidName);
    EXPECT_EQ(WriteWorldFile("", "ok", "x", true).status, WorldFileStatus::NoDirectory);
    EXPECT_EQ(WriteWorldFile(Dir("w"), "big", std::string(kMaxWorldJsonBytes + 1, 'x'), true).status, WorldFileStatus::TooLarge);
    EXPECT_FALSE(fs::exists(Dir("w"))) << "nothing may be created for a refused write";
}

TEST_F(WorldFilesTest, WriteAtTheSizeCapIsAccepted)
{
    const std::string dir = Dir("w");
    const WorldFileResult r = WriteWorldFile(dir, "cap", std::string(kMaxWorldJsonBytes, 'x'), false);
    ASSERT_TRUE(r.Ok()) << r.message;
    EXPECT_EQ(r.bytes, kMaxWorldJsonBytes);
    std::string back;
    EXPECT_TRUE(ReadWorldFile(dir, "", "cap", back).Ok());
    EXPECT_EQ(back.size(), kMaxWorldJsonBytes);
}

TEST_F(WorldFilesTest, ReadPrefersTheOutDirectoryThenTheDeployedOne)
{
    const std::string out = Dir("out"), deployed = Dir("deployed");
    ASSERT_TRUE(WriteWorldFile(deployed, "both", "deployed copy", false).Ok());
    ASSERT_TRUE(WriteWorldFile(deployed, "only_deployed", "from deployed", false).Ok());
    ASSERT_TRUE(WriteWorldFile(out, "both", "out copy", false).Ok());

    std::string text;
    WorldFileResult r = ReadWorldFile(out, deployed, "both", text);
    ASSERT_TRUE(r.Ok());
    EXPECT_EQ(text, "out copy");
    EXPECT_EQ(r.path, WorldFilePath(out, "both"));
    r = ReadWorldFile(out, deployed, "only_deployed", text);
    ASSERT_TRUE(r.Ok());
    EXPECT_EQ(text, "from deployed");
    EXPECT_EQ(r.path, WorldFilePath(deployed, "only_deployed"));
    // Either directory may be unconfigured.
    EXPECT_TRUE(ReadWorldFile("", deployed, "both", text).Ok());
    EXPECT_EQ(text, "deployed copy");
    EXPECT_TRUE(ReadWorldFile(out, "", "both", text).Ok());
}

TEST_F(WorldFilesTest, ReadReportsMissingBadNamesAndNoDirectories)
{
    std::string text = "stale";
    WorldFileResult r = ReadWorldFile(Dir("out"), Dir("deployed"), "ghost", text);
    EXPECT_EQ(r.status, WorldFileStatus::NotFound);
    EXPECT_NE(r.message.find("ghost"), std::string::npos);
    EXPECT_TRUE(text.empty());
    EXPECT_EQ(ReadWorldFile(Dir("out"), "", "../x", text).status, WorldFileStatus::InvalidName);
    EXPECT_EQ(ReadWorldFile(Dir("out"), "", "", text).status, WorldFileStatus::InvalidName);
    EXPECT_EQ(ReadWorldFile("", "", "ok", text).status, WorldFileStatus::NoDirectory);
}

TEST_F(WorldFilesTest, ReadRefusesAnOversizeFileWithoutLoadingIt)
{
    const std::string dir = Dir("w");
    fs::create_directories(dir);
    {
        std::ofstream f(WorldFilePath(dir, "huge"), std::ios::binary);
        f.seekp(static_cast<std::streamoff>(kMaxWorldJsonBytes));   // sparse-ish: size cap + 1 once a byte lands
        f.put('x');
    }
    std::string text = "stale";
    const WorldFileResult r = ReadWorldFile(dir, "", "huge", text);
    EXPECT_EQ(r.status, WorldFileStatus::TooLarge);
    EXPECT_TRUE(text.empty());
}

TEST_F(WorldFilesTest, AFolderNamedLikeAWorldIsNotAWorld)
{
    const std::string dir = Dir("w");
    fs::create_directories(fs::path(WorldFilePath(dir, "dirworld")));
    std::string text;
    EXPECT_EQ(ReadWorldFile(dir, "", "dirworld", text).status, WorldFileStatus::NotFound);
}

TEST_F(WorldFilesTest, EmptyFileReadsAsEmptyText)
{
    const std::string dir = Dir("w");
    ASSERT_TRUE(WriteWorldFile(dir, "empty", "", false).Ok());
    std::string text = "stale";
    const WorldFileResult r = ReadWorldFile(dir, "", "empty", text);
    EXPECT_TRUE(r.Ok());
    EXPECT_TRUE(text.empty());
}

TEST_F(WorldFilesTest, AWorldSurvivesExportWriteReadImportThroughARealFile)
{
    const WorldStructure original = MakeTinyWorldWithUnknownKind();
    std::string text;
    JsonIssue issue;
    ASSERT_TRUE(ExportWorldText(original, TestImportContext(), text, issue));
    const std::string dir = Dir("worlds");
    ASSERT_TRUE(WriteWorldFile(dir, DefaultWorldName(original.provenance.seed), text, false).Ok());

    std::string loaded;
    ASSERT_TRUE(ReadWorldFile(dir, "", DefaultWorldName(original.provenance.seed), loaded).Ok());
    EXPECT_EQ(loaded, text) << "no CRLF translation either way";
    WorldStructure imported;
    const WorldImportResult r = ImportWorldText(loaded, TestImportContext(), imported);
    ASSERT_TRUE(r.ok) << r.error.field << ": " << r.error.message;
    EXPECT_TRUE(SameCells(original, imported));
    EXPECT_TRUE(original.sites == imported.sites);
}
