// TestManifestSaver.cpp - Unit tests for ManifestSaver
// Suite: ManifestSaver

#include <gtest/gtest.h>
#include <DiaApplicationEditor/V2/ManifestSaver.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

#include <windows.h>
#include <cstdio>
#include <cstring>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::Core;

// ==============================================================================
// Test helpers
// ==============================================================================

namespace
{
    static const char* kTestFilePath = "C:\\Temp\\dia_test_saver.diaapp";
    static const char* kTestBakPath  = "C:\\Temp\\dia_test_saver.diaapp.bak";
    static const char* kTestTmpPath  = "C:\\Temp\\dia_test_saver.diaapp.tmp";

    static void EnsureTempDir()
    {
        CreateDirectoryA("C:\\Temp", nullptr);
    }

    static void WriteFile(const char* path, const char* content)
    {
        FILE* f = nullptr;
        fopen_s(&f, path, "wt");
        if (f)
        {
            fputs(content, f);
            fclose(f);
        }
    }

    static bool FileExists(const char* path)
    {
        return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    }

    static void DeleteIfExists(const char* path)
    {
        if (FileExists(path))
            DeleteFileA(path);
    }

    static void CleanupTestFiles()
    {
        DeleteIfExists(kTestFilePath);
        DeleteIfExists(kTestBakPath);
        DeleteIfExists(kTestTmpPath);
    }

    static ManifestEditorState MakeStateWithPath(const char* path)
    {
        ManifestEditorState state;
        strncpy_s(state.filePath, sizeof(state.filePath), path, _TRUNCATE);
        state.hasManifest = true;
        state.isDirty = true;
        return state;
    }
}

// ==============================================================================
// Save_CreatesBackupFile
// ==============================================================================

TEST(ManifestSaver, Save_CreatesBackupFile)
{
    EnsureTempDir();
    CleanupTestFiles();

    WriteFile(kTestFilePath, "{\"version\":2}");

    ManifestEditorState state = MakeStateWithPath(kTestFilePath);

    SaveResult result = ManifestSaver::Save(state);

    EXPECT_EQ(result.status, SaveStatus::Ok);
    EXPECT_TRUE(FileExists(kTestBakPath));

    CleanupTestFiles();
}

// ==============================================================================
// Save_AtomicWrite_OriginalReplaced
// ==============================================================================

TEST(ManifestSaver, Save_AtomicWrite_OriginalReplaced)
{
    EnsureTempDir();
    CleanupTestFiles();

    WriteFile(kTestFilePath, "{\"version\":2}");

    ManifestEditorState state = MakeStateWithPath(kTestFilePath);
    state.manifest.version = 2;

    SaveResult result = ManifestSaver::Save(state);

    EXPECT_EQ(result.status, SaveStatus::Ok);
    EXPECT_TRUE(FileExists(kTestFilePath));
    EXPECT_FALSE(state.isDirty);

    // File should have content
    FILE* f = nullptr;
    fopen_s(&f, kTestFilePath, "rt");
    ASSERT_NE(f, nullptr);
    char readBuf[256];
    readBuf[0] = '\0';
    if (fgets(readBuf, sizeof(readBuf), f))
    {
        EXPECT_GT(strlen(readBuf), 0u);
    }
    fclose(f);

    CleanupTestFiles();
}

// ==============================================================================
// Save_NoExistingFile_NoBakCreated
// ==============================================================================

TEST(ManifestSaver, Save_NoExistingFile_NoBakCreated)
{
    EnsureTempDir();
    CleanupTestFiles();

    ManifestEditorState state = MakeStateWithPath(kTestFilePath);

    SaveResult result = ManifestSaver::Save(state);

    EXPECT_EQ(result.status, SaveStatus::Ok);
    EXPECT_FALSE(FileExists(kTestBakPath));
    EXPECT_TRUE(FileExists(kTestFilePath));

    CleanupTestFiles();
}

// ==============================================================================
// SerializeToJson_ContainsVersionField
// ==============================================================================

TEST(ManifestSaver, SerializeToJson_ContainsVersionField)
{
    ManifestEditorState state;
    state.manifest.version = 2;
    state.hasManifest = true;

    char buf[4096];
    ManifestSaver::SerializeToJson(state, buf, sizeof(buf));

    EXPECT_NE(strstr(buf, "version"), nullptr);
}
