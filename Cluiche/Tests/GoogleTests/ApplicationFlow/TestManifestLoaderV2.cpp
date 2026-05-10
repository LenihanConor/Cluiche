////////////////////////////////////////////////////////////////////////////////
// Filename: TestManifestLoaderV2.cpp
// GoogleTest suite — DiaApplicationFlow v2 ApplicationManifestLoaderV2
//
// Tests JSON → ApplicationManifestV2 loading via LoadFromString().
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoaderV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Minimal valid manifest JSON (no modules — loader does not validate types)
// ---------------------------------------------------------------------------
static const char* kMinimalManifest = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(ManifestLoaderV2, LoadMinimalManifest)
{
    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(kMinimalManifest, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    EXPECT_EQ(manifest.version, 2);
    EXPECT_EQ(manifest.initialStage, StringCRC("Boot"));
    ASSERT_EQ(manifest.stages.Size(), 1u);
    EXPECT_EQ(manifest.stages[0].name, StringCRC("Boot"));
    ASSERT_EQ(manifest.processingUnits.Size(), 1u);
    EXPECT_EQ(manifest.processingUnits[0].instanceId, StringCRC("MainPU"));
    EXPECT_FLOAT_EQ(manifest.processingUnits[0].frequencyHz, 30.0f);
    EXPECT_FALSE(manifest.processingUnits[0].dedicatedThread);
}

TEST(ManifestLoaderV2, LoadWithModuleDeclarations)
{
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 60.0,
    "dedicated_thread": false,
    "modules": [{
      "instance_id": "TestModInst",
      "type_id": "TestModType",
      "stages": ["Boot"],
      "start_timeout_ms": 5000.0,
      "stop_timeout_ms": 2000.0
    }]
  }]
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.processingUnits.Size(), 1u);
    const ProcessingUnitDeclaration& pu = manifest.processingUnits[0];
    ASSERT_EQ(pu.modules.Size(), 1u);

    const ModuleDeclaration& mod = pu.modules[0];
    EXPECT_EQ(mod.instanceId, StringCRC("TestModInst"));
    EXPECT_EQ(mod.typeId, StringCRC("TestModType"));
    ASSERT_EQ(mod.stages.Size(), 1u);
    EXPECT_EQ(mod.stages[0], StringCRC("Boot"));
    EXPECT_FLOAT_EQ(mod.startTimeoutMs, 5000.0f);
    EXPECT_FLOAT_EQ(mod.stopTimeoutMs, 2000.0f);
}

TEST(ManifestLoaderV2, LoadWithStreams)
{
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "streams": [{
    "id": "InputStream",
    "type": "InputData",
    "from": "InputPU",
    "to": "MainPU",
    "multi_writer": false
  }],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.streams.Size(), 1u);
    const StreamDeclaration& stream = manifest.streams[0];
    EXPECT_EQ(stream.id, StringCRC("InputStream"));
    EXPECT_EQ(stream.type, StringCRC("InputData"));
    EXPECT_EQ(stream.fromPU, StringCRC("InputPU"));
    EXPECT_EQ(stream.toPU, StringCRC("MainPU"));
    EXPECT_FALSE(stream.multiWriter);
}

TEST(ManifestLoaderV2, InvalidVersionReturnsVersionMismatch)
{
    const char* json = R"({
  "version": 1,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "processing_units": []
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kVersionMismatch);
}

TEST(ManifestLoaderV2, EmptyJsonStringReturnsParseError)
{
    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString("", manifest);

    EXPECT_EQ(result, LoadResult::kParseError);
}

TEST(ManifestLoaderV2, MultipleStagesParsed)
{
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot", "Game", "Credits"],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.stages.Size(), 3u);
    EXPECT_EQ(manifest.stages[0].name, StringCRC("Boot"));
    EXPECT_EQ(manifest.stages[1].name, StringCRC("Game"));
    EXPECT_EQ(manifest.stages[2].name, StringCRC("Credits"));
}

TEST(ManifestLoaderV2, AutoStagesParsed)
{
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot", "Game"],
  "auto_stages": ["Boot"],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.autoStages.Size(), 1u);
    EXPECT_EQ(manifest.autoStages[0], StringCRC("Boot"));
}

TEST(ManifestLoaderV2, NullStringReturnsParseError)
{
    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(nullptr, manifest);
    EXPECT_EQ(result, LoadResult::kParseError);
}

TEST(ManifestLoaderV2, MultiWriterFlagParsedTrue)
{
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "streams": [{
    "id": "SharedStream",
    "type": "EventData",
    "from": "PU1",
    "to": "PU2",
    "multi_writer": true
  }],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

    ApplicationManifestV2 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.streams.Size(), 1u);
    EXPECT_TRUE(manifest.streams[0].multiWriter);
}
