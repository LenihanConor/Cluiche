// TestFilePath.cpp - Google Test unit tests for FilePath
//
// Tests FilePath from DiaCore FilePath subsystem.
// NOTE: Resolve() is not tested here because it depends on PathStore
// singleton runtime setup. We register a test alias for constructor/Create
// tests that require PathStore validation.

#include <gtest/gtest.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/PathStore.h>

using namespace Dia::Core;

// ==============================================================================
// Test Fixture — registers a path alias so constructors/Create don't assert
// ==============================================================================

class FilePathTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // Register a test alias once for all tests in this suite.
        // PathStore is a global singleton; registering "test" makes
        // FilePath constructors and Create methods safe to call.
        if (!PathStore::IsPathAliasRegistered(Path::Alias("test")))
        {
            PathStore::RegisterToStore(Path::Alias("test"), Path::String("C:/TestPath"));
        }
    }
};

// ==============================================================================
// Default Construction
// ==============================================================================

TEST_F(FilePathTest, DefaultConstruction_AccessorsReturnEmpty)
{
    FilePath path;

    EXPECT_EQ(path.GetPathAlias().Value(), 0u);
    EXPECT_EQ(path.GetPathAmendment().Length(), 0u);
    EXPECT_EQ(path.GetFileName().Length(), 0u);
}

// ==============================================================================
// Parameterized Construction
// ==============================================================================

TEST_F(FilePathTest, ConstructWithAliasAndFilename_StoresCorrectly)
{
    Path::Alias alias("test");
    FilePath::FileName filename("player.png");

    FilePath path(alias, filename);

    EXPECT_EQ(path.GetPathAlias(), alias);
    EXPECT_TRUE(path.GetFileName() == "player.png");
    EXPECT_EQ(path.GetPathAmendment().Length(), 0u);
}

TEST_F(FilePathTest, ConstructWithAliasAmendmentAndFilename_StoresAllThree)
{
    Path::Alias alias("test");
    FilePath::PathAmendment amendment("textures");
    FilePath::FileName filename("player.png");

    FilePath path(alias, amendment, filename);

    EXPECT_EQ(path.GetPathAlias(), alias);
    EXPECT_TRUE(path.GetPathAmendment() == "textures");
    EXPECT_TRUE(path.GetFileName() == "player.png");
}

// ==============================================================================
// Create Methods
// ==============================================================================

TEST_F(FilePathTest, CreateWithAliasAndFilename_OverwritesPreviousValues)
{
    Path::Alias alias("test");
    FilePath::FileName filename1("old.txt");
    FilePath::FileName filename2("new.txt");

    FilePath path(alias, filename1);
    path.Create(alias, filename2);

    EXPECT_TRUE(path.GetFileName() == "new.txt");
}

TEST_F(FilePathTest, CreateWithAliasAmendmentAndFilename_OverwritesPreviousValues)
{
    Path::Alias alias("test");
    FilePath::PathAmendment amendment1("olddir");
    FilePath::FileName filename1("old.txt");

    FilePath path(alias, amendment1, filename1);

    FilePath::PathAmendment amendment2("newdir");
    FilePath::FileName filename2("new.txt");
    path.Create(alias, amendment2, filename2);

    EXPECT_TRUE(path.GetPathAmendment() == "newdir");
    EXPECT_TRUE(path.GetFileName() == "new.txt");
}

// ==============================================================================
// Accessor Tests
// ==============================================================================

TEST_F(FilePathTest, GetPathAlias_ReturnsAlias)
{
    Path::Alias alias("test");
    FilePath::FileName filename("data.bin");

    FilePath path(alias, filename);

    EXPECT_EQ(path.GetPathAlias(), alias);
}

TEST_F(FilePathTest, GetPathAmendment_ReturnsAmendment)
{
    Path::Alias alias("test");
    FilePath::PathAmendment amendment("subdir");
    FilePath::FileName filename("data.bin");

    FilePath path(alias, amendment, filename);

    EXPECT_TRUE(path.GetPathAmendment() == "subdir");
}

TEST_F(FilePathTest, GetFileName_ReturnsFileName)
{
    Path::Alias alias("test");
    FilePath::FileName filename("sprite.png");

    FilePath path(alias, filename);

    EXPECT_TRUE(path.GetFileName() == "sprite.png");
}

// ==============================================================================
// ResolveFileType
// ==============================================================================

TEST_F(FilePathTest, ResolveFileType_ExtractsExtension)
{
    Path::Alias alias("test");
    FilePath::FileName filename("player.png");

    FilePath path(alias, filename);

    FilePath::FileType fileType = path.ResolveFileType();
    EXPECT_TRUE(fileType == "png");
}

// ==============================================================================
// ResolveFileNameWithoutFileType
// ==============================================================================

TEST_F(FilePathTest, ResolveFileNameWithoutFileType_StripsExtension)
{
    Path::Alias alias("test");
    FilePath::FileName filename("player.png");

    FilePath path(alias, filename);

    FilePath::FileName nameOnly = path.ResolveFileNameWithoutFileType();
    EXPECT_TRUE(nameOnly == "player");
}
