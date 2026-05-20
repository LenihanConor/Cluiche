////////////////////////////////////////////////////////////////////////////////
// Filename: TestManifestLoaderV2.cpp
// GoogleTest suite — DiaApplicationFlow v3 ApplicationManifestLoaderV2
//
// Tests JSON → ApplicationManifestV3 loading via LoadFromString().
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoaderV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Helpers — canonical minimal v3 manifest JSON
// ---------------------------------------------------------------------------
static const char* kMinimalManifest = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot", "transitions": [], "auto_advance": false }
  ],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

// ---------------------------------------------------------------------------
// Basic loading
// ---------------------------------------------------------------------------

TEST(ManifestLoaderV2, LoadMinimalManifest)
{
    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(kMinimalManifest, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    EXPECT_EQ(manifest.version, 3);
    EXPECT_EQ(manifest.initialStage, StringCRC("Boot"));
    ASSERT_EQ(manifest.stages.Size(), 1u);
    EXPECT_EQ(manifest.stages[0].name, StringCRC("Boot"));
    EXPECT_EQ(manifest.stages[0].transitions.Size(), 0u);
    EXPECT_FALSE(manifest.stages[0].autoAdvance);
    ASSERT_EQ(manifest.processingUnits.Size(), 1u);
    EXPECT_EQ(manifest.processingUnits[0].instanceId, StringCRC("MainPU"));
    EXPECT_FLOAT_EQ(manifest.processingUnits[0].frequencyHz, 30.0f);
    EXPECT_FALSE(manifest.processingUnits[0].dedicatedThread);
}

TEST(ManifestLoaderV2, LoadWithModuleDeclarations)
{
    const char* json = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot", "transitions": [], "auto_advance": false }
  ],
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

    ApplicationManifestV3 manifest;
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
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot", "transitions": [], "auto_advance": false }
  ],
  "streams": [{
    "id": "InputStream",
    "kind": "EventStream",
    "payload_type": "InputData",
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

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.streams.Size(), 1u);
    const StreamDeclaration& stream = manifest.streams[0];
    EXPECT_EQ(stream.id, StringCRC("InputStream"));
    EXPECT_EQ(stream.kind, StringCRC("EventStream"));
    EXPECT_EQ(stream.payloadType, StringCRC("InputData"));
    EXPECT_EQ(stream.fromPU, StringCRC("InputPU"));
    EXPECT_EQ(stream.toPU, StringCRC("MainPU"));
    EXPECT_FALSE(stream.multiWriter);
}

TEST(ManifestLoaderV2, InvalidVersionReturnsVersionMismatch)
{
    // v2-shaped manifest (version: 2, string stages) must be rejected
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot"],
  "processing_units": []
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kVersionMismatch);
}

TEST(ManifestLoaderV2, EmptyJsonStringReturnsParseError)
{
    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString("", manifest);

    EXPECT_EQ(result, LoadResult::kParseError);
}

TEST(ManifestLoaderV2, MultipleStagesParsed)
{
    const char* json = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot",    "transitions": ["Game"],  "auto_advance": false },
    { "name": "Game",    "transitions": ["Boot"],  "auto_advance": false },
    { "name": "Credits", "transitions": [],        "auto_advance": false }
  ],
  "processing_units": [{
    "instance_id": "MainPU",
    "frequency_hz": 30.0,
    "dedicated_thread": false,
    "modules": []
  }]
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.stages.Size(), 3u);
    EXPECT_EQ(manifest.stages[0].name, StringCRC("Boot"));
    EXPECT_EQ(manifest.stages[1].name, StringCRC("Game"));
    EXPECT_EQ(manifest.stages[2].name, StringCRC("Credits"));
}

TEST(ManifestLoaderV2, NullStringReturnsParseError)
{
    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(nullptr, manifest);
    EXPECT_EQ(result, LoadResult::kParseError);
}

TEST(ManifestLoaderV2, MultiWriterFlagParsedTrue)
{
    const char* json = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot", "transitions": [], "auto_advance": false }
  ],
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

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.streams.Size(), 1u);
    EXPECT_TRUE(manifest.streams[0].multiWriter);
}

// ---------------------------------------------------------------------------
// v3-specific: transitions and auto_advance
// ---------------------------------------------------------------------------

TEST(ManifestLoaderV2, BranchingTransitionsParsed)
{
    // Boot can transition to either DummyStage or StupidStage (hub pattern)
    const char* json = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot",       "transitions": ["DummyStage", "StupidStage"], "auto_advance": false },
    { "name": "DummyStage", "transitions": ["Boot"],                      "auto_advance": false },
    { "name": "StupidStage","transitions": ["Boot"],                      "auto_advance": false }
  ],
  "processing_units": []
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.stages.Size(), 3u);

    const StageDeclaration& boot = manifest.stages[0];
    EXPECT_EQ(boot.name, StringCRC("Boot"));
    ASSERT_EQ(boot.transitions.Size(), 2u);
    EXPECT_EQ(boot.transitions[0], StringCRC("DummyStage"));
    EXPECT_EQ(boot.transitions[1], StringCRC("StupidStage"));
    EXPECT_FALSE(boot.autoAdvance);

    const StageDeclaration& dummy = manifest.stages[1];
    ASSERT_EQ(dummy.transitions.Size(), 1u);
    EXPECT_EQ(dummy.transitions[0], StringCRC("Boot"));
    EXPECT_FALSE(dummy.autoAdvance);
}

TEST(ManifestLoaderV2, AutoAdvanceParsedTrue)
{
    const char* json = R"({
  "version": 3,
  "initial_stage": "Boot",
  "stages": [
    { "name": "Boot", "transitions": ["Game"], "auto_advance": true },
    { "name": "Game", "transitions": [],       "auto_advance": false }
  ],
  "processing_units": []
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.stages.Size(), 2u);
    EXPECT_TRUE(manifest.stages[0].autoAdvance);
    EXPECT_EQ(manifest.stages[0].transitions[0], StringCRC("Game"));
    EXPECT_FALSE(manifest.stages[1].autoAdvance);
}

TEST(ManifestLoaderV2, StringFormStageSkippedWithWarning)
{
    // String-form stage entries are no longer valid in v3; the version check
    // should have already rejected v2 files, but a v3 file with a malformed
    // string entry is skipped and the valid object entries are still loaded.
    const char* json = R"({
  "version": 3,
  "initial_stage": "Good",
  "stages": [
    "BadStringStage",
    { "name": "Good", "transitions": [], "auto_advance": false }
  ],
  "processing_units": []
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    // Load succeeds — the bad entry is skipped, the valid object is kept
    EXPECT_EQ(result, LoadResult::kSuccess);
    ASSERT_EQ(manifest.stages.Size(), 1u);
    EXPECT_EQ(manifest.stages[0].name, StringCRC("Good"));
}

TEST(ManifestLoaderV2, V2ManifestRejected)
{
    // A well-formed v2 manifest must be rejected by the v3 loader
    const char* json = R"({
  "version": 2,
  "initial_stage": "Boot",
  "stages": ["Boot", "Game"],
  "auto_stages": ["Boot"],
  "processing_units": []
})";

    ApplicationManifestV3 manifest;
    LoadResult result = ApplicationManifestLoaderV2::LoadFromString(json, manifest);

    EXPECT_EQ(result, LoadResult::kVersionMismatch);
}
