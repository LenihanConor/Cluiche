// TestManifestSaver.cpp - Unit tests for ManifestSaver
// Suite: ManifestSaver

#include <gtest/gtest.h>
#include <DiaApplicationFlowEditor/V2/ManifestSaver.h>
#include <DiaApplicationFlowEditor/V2/ManifestLoader.h>
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>
#include <DiaStreams/OverflowPolicy.h>

#include <windows.h>
#include <cstdio>
#include <cstring>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
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

    WriteFile(kTestFilePath, "{\"version\":3}");

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

    WriteFile(kTestFilePath, "{\"version\":3}");

    ManifestEditorState state = MakeStateWithPath(kTestFilePath);
    state.manifest.version = 3;

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
    state.manifest.version = 3;
    state.hasManifest = true;

    char buf[4096];
    ManifestSaver::SerializeToJson(state, buf, sizeof(buf));

    EXPECT_NE(strstr(buf, "version"), nullptr);
}

// ==============================================================================
// RoundTrip_PreservesOverflowAndMultiWriter
// Verifies that overflow policy, block timeout, and multi_writer survive a
// load → save → reload cycle. Regression guard for the kind-aware stream
// inspector: if the saver drops these fields, edits made in the UI would be
// silently lost on disk.
// ==============================================================================

TEST(ManifestSaver, RoundTrip_PreservesOverflowAndMultiWriter)
{
    EnsureTempDir();
    CleanupTestFiles();

    static const char* kRoundTripJson =
        "{"
        "  \"version\": 3,"
        "  \"stages\": [{\"name\": \"Boot\", \"transitions\": [], \"auto_advance\": false}],"
        "  \"initial_stage\": \"Boot\","
        "  \"streams\": ["
        "    {"
        "      \"id\": \"EvtStream\","
        "      \"kind\": \"EventStream\","
        "      \"payload_type\": \"InputEvent\","
        "      \"from\": \"MainPU\","
        "      \"to\": \"SimPU\","
        "      \"capacity\": 32,"
        "      \"max_readers\": 2,"
        "      \"overflow\": \"block\","
        "      \"block_timeout_ms\": 250"
        "    },"
        "    {"
        "      \"id\": \"FrmStream\","
        "      \"kind\": \"FrameStream\","
        "      \"payload_type\": \"FrameData\","
        "      \"from\": \"MainPU\","
        "      \"to\": \"SimPU\","
        "      \"capacity\": 4,"
        "      \"max_readers\": 1,"
        "      \"multi_writer\": true"
        "    }"
        "  ],"
        "  \"processing_units\": ["
        "    {"
        "      \"instance_id\": \"MainPU\","
        "      \"frequency_hz\": 30.0,"
        "      \"dedicated_thread\": false,"
        "      \"modules\": []"
        "    }"
        "  ]"
        "}";

    WriteFile(kTestFilePath, kRoundTripJson);

    // ManifestEditorState is large (~hundreds of KB) — heap-allocate to keep
    // the test stack frame manageable on Windows default 1MB stacks.
    ManifestEditorState* state    = new ManifestEditorState();
    ManifestEditorState* reloaded = new ManifestEditorState();

    // Load
    LoadResult loadResult = ManifestLoader::Load(kTestFilePath, *state);
    ASSERT_EQ(loadResult.status, LoadStatus::Ok);

    // Save (writes back to kTestFilePath)
    SaveResult saveResult = ManifestSaver::Save(*state);
    ASSERT_EQ(saveResult.status, SaveStatus::Ok);

    // Reload from the saved file
    LoadResult reloadResult = ManifestLoader::Load(kTestFilePath, *reloaded);
    ASSERT_EQ(reloadResult.status, LoadStatus::Ok);
    ASSERT_EQ(reloaded->manifest.streams.Size(), 2u);

    // Find streams by id (saver may not preserve order)
    const StreamDeclaration* evt = nullptr;
    const StreamDeclaration* frm = nullptr;
    for (unsigned int i = 0; i < reloaded->manifest.streams.Size(); ++i)
    {
        const StreamDeclaration& s = reloaded->manifest.streams[i];
        if (s.id == StringCRC("EvtStream")) evt = &s;
        if (s.id == StringCRC("FrmStream")) frm = &s;
    }

    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->kind,           StringCRC("EventStream"));
    EXPECT_EQ(evt->overflowPolicy, OverflowPolicy::kBlock);
    EXPECT_EQ(evt->blockTimeoutMs, 250u);

    ASSERT_NE(frm, nullptr);
    EXPECT_EQ(frm->kind,        StringCRC("FrameStream"));
    EXPECT_TRUE(frm->multiWriter);

    delete state;
    delete reloaded;

    CleanupTestFiles();
}


// ==============================================================================
// RoundTrip_PreservesRequiresTickAfterAndModuleConfig
// The saver used to drop `requires_tick_after` and wrote the module config under
// "config_json" (a string) where every loader reads "config" (an object).
// ==============================================================================

TEST(ManifestSaver, RoundTrip_PreservesRequiresTickAfterAndModuleConfig)
{
    EnsureTempDir();
    CleanupTestFiles();

    static const char* kJson =
        "{"
        "  \"version\": 3,"
        "  \"stages\": [{\"name\": \"Boot\", \"transitions\": [], \"auto_advance\": false}],"
        "  \"initial_stage\": \"Boot\","
        "  \"streams\": [],"
        "  \"processing_units\": ["
        "    {"
        "      \"instance_id\": \"SimPU\","
        "      \"frequency_hz\": 30.0,"
        "      \"dedicated_thread\": false,"
        "      \"modules\": ["
        "        {\"instance_id\": \"Clock\", \"type_id\": \"Clock\", \"stages\": [\"all\"], \"dependencies\": [], \"channels\": []},"
        "        {\"instance_id\": \"Step\", \"type_id\": \"Step\", \"stages\": [\"all\"], \"dependencies\": [], \"channels\": [],"
        "         \"requires_tick_after\": [\"Clock\"], \"config\": {\"worker_cap\": \"auto\", \"ranges_per_worker\": 4}}"
        "      ]"
        "    }"
        "  ]"
        "}";
    WriteFile(kTestFilePath, kJson);

    ManifestEditorState* state    = new ManifestEditorState();
    ManifestEditorState* reloaded = new ManifestEditorState();
    ASSERT_EQ(ManifestLoader::Load(kTestFilePath, *state).status, LoadStatus::Ok);
    ASSERT_EQ(ManifestSaver::Save(*state).status, SaveStatus::Ok);
    ASSERT_EQ(ManifestLoader::Load(kTestFilePath, *reloaded).status, LoadStatus::Ok);

    ASSERT_EQ(reloaded->manifest.processingUnits.Size(), 1u);
    const ProcessingUnitDeclaration& pu = reloaded->manifest.processingUnits[0];
    ASSERT_EQ(pu.modules.Size(), 2u);
    EXPECT_EQ(pu.modules[0].requiresTickAfter.Size(), 0u);
    ASSERT_EQ(pu.modules[1].requiresTickAfter.Size(), 1u);
    EXPECT_EQ(pu.modules[1].requiresTickAfter[0], StringCRC("Clock"));
    EXPECT_NE(std::strstr(pu.modules[1].configJson.AsCStr(), "worker_cap"), nullptr) << pu.modules[1].configJson.AsCStr();
    EXPECT_NE(std::strstr(pu.modules[1].configJson.AsCStr(), "ranges_per_worker"), nullptr) << pu.modules[1].configJson.AsCStr();

    delete state;
    delete reloaded;
    CleanupTestFiles();
}
