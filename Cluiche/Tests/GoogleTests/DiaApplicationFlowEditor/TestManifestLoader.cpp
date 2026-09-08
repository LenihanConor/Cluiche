#include <gtest/gtest.h>
#include <DiaApplicationFlowEditor/V2/ManifestLoader.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdio.h>
#include <string.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
using Dia::Core::StringCRC;

// ==============================================================================
// Helpers
// ==============================================================================

static void WriteTempFile(const char* path, const char* content)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void DeleteTempFile(const char* path)
{
    remove(path);
}

static const char* kTempPath = "TestManifestLoader_temp.diaapp";

static const char* kValidMinimal =
    "{"
    "  \"version\": 3,"
    "  \"stages\": [{\"name\": \"Boot\", \"transitions\": [], \"auto_advance\": false}],"
    "  \"initial_stage\": \"Boot\","
    "  \"streams\": [],"
    "  \"processing_units\": ["
    "    {"
    "      \"instance_id\": \"MainPU\","
    "      \"frequency_hz\": 30.0,"
    "      \"dedicated_thread\": false,"
    "      \"modules\": []"
    "    }"
    "  ]"
    "}";

static const char* kValidWithStream =
    "{"
    "  \"version\": 3,"
    "  \"stages\": [{\"name\": \"Boot\", \"transitions\": [], \"auto_advance\": false}],"
    "  \"initial_stage\": \"Boot\","
    "  \"streams\": ["
    "    {"
    "      \"id\": \"InputToSim\","
    "      \"kind\": \"EventStream\","
    "      \"payload_type\": \"InputEvent\","
    "      \"from\": \"MainPU\","
    "      \"to\": \"SimPU\","
    "      \"capacity\": 0,"
    "      \"max_readers\": 0"
    "    }"
    "  ],"
    "  \"processing_units\": ["
    "    {"
    "      \"instance_id\": \"MainPU\","
    "      \"frequency_hz\": 30.0,"
    "      \"dedicated_thread\": false,"
    "      \"modules\": ["
    "        {"
    "          \"instance_id\": \"InputModule\","
    "          \"type_id\": \"InputModule\","
    "          \"stages\": [\"all\"],"
    "          \"dependencies\": [],"
    "          \"channels\": [{\"id\": \"InputToSim\", \"role\": \"writes\"}],"
    "          \"start_timeout_ms\": 10000.0,"
    "          \"stop_timeout_ms\": 5000.0,"
    "          \"config\": {}"
    "        }"
    "      ]"
    "    }"
    "  ]"
    "}";

// ==============================================================================
// Tests
// ==============================================================================

TEST(ManifestLoader, Load_ValidFile_ReturnsOk)
{
    WriteTempFile(kTempPath, kValidMinimal);

    ManifestEditorState state;
    LoadResult result = ManifestLoader::Load(kTempPath, state);

    EXPECT_EQ(result.status, LoadStatus::Ok);
    EXPECT_TRUE(state.hasManifest);
    EXPECT_FALSE(state.isDirty);
    EXPECT_EQ(state.manifest.processingUnits[0u].instanceId, StringCRC("MainPU"));

    DeleteTempFile(kTempPath);
}

TEST(ManifestLoader, Load_MalformedJson_ReturnsMalformed)
{
    WriteTempFile(kTempPath, "{bad json");

    ManifestEditorState state;
    LoadResult result = ManifestLoader::Load(kTempPath, state);

    EXPECT_EQ(result.status, LoadStatus::MalformedJson);

    DeleteTempFile(kTempPath);
}

TEST(ManifestLoader, Load_WrongVersion_ReturnsWrongVersion)
{
    // v2-shaped manifest must be rejected by the v3 loader
    const char* wrongVersion =
        "{\"version\": 2, \"stages\": [\"Boot\"], \"initial_stage\": \"\","
        " \"auto_stages\": [], \"streams\": [], \"processing_units\": []}";

    WriteTempFile(kTempPath, wrongVersion);

    ManifestEditorState state;
    LoadResult result = ManifestLoader::Load(kTempPath, state);

    EXPECT_EQ(result.status, LoadStatus::WrongVersion);

    DeleteTempFile(kTempPath);
}

TEST(ManifestLoader, Load_NonExistentFile_ReturnsLockedFile)
{
    ManifestEditorState state;
    LoadResult result = ManifestLoader::Load(
        "nonexistent_path_that_does_not_exist_loader_test.diaapp", state);

    EXPECT_EQ(result.status, LoadStatus::LockedFile);
}

TEST(ManifestLoader, Load_ValidFile_PopulatesStreams)
{
    WriteTempFile(kTempPath, kValidWithStream);

    ManifestEditorState state;
    LoadResult result = ManifestLoader::Load(kTempPath, state);

    ASSERT_EQ(result.status, LoadStatus::Ok);
    ASSERT_EQ(state.manifest.streams.Size(), 1u);

    const StreamDeclaration& stream = state.manifest.streams[0u];
    EXPECT_EQ(stream.id,          StringCRC("InputToSim"));
    EXPECT_EQ(stream.fromPU,      StringCRC("MainPU"));
    EXPECT_EQ(stream.toPU,        StringCRC("SimPU"));
    EXPECT_EQ(stream.kind,        StringCRC("EventStream"));
    EXPECT_EQ(stream.payloadType, StringCRC("InputEvent"));

    DeleteTempFile(kTempPath);
}
