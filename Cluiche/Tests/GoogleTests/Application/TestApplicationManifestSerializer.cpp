#include <gtest/gtest.h>

#include <DiaApplication/Manifest/JsonApplicationManifestSerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <cstdio>

using namespace Dia::Application;

static const char* kValidManifest = R"({
    "version": 1,
    "processing_units": [
        {
            "type_id": "MainProcessingUnit",
            "instance_id": "main_pu",
            "frequency_hz": 60.0,
            "dedicated_thread": false,
            "initial_phase": "init",
            "phases": [
                { "type_id": "InitPhase", "instance_id": "init" },
                { "type_id": "UpdatePhase", "instance_id": "update" }
            ],
            "modules": [
                {
                    "type_id": "RenderModule",
                    "instance_id": "render",
                    "phase_ids": ["update"],
                    "dependencies": []
                }
            ],
            "transitions": [
                { "from": "init", "to": "update" }
            ]
        }
    ]
})";

static const char* kValidMinimal = R"({
    "version": 1,
    "processing_units": [
        {
            "type_id": "TestPU",
            "instance_id": "test",
            "initial_phase": "idle",
            "phases": [ { "type_id": "IdlePhase", "instance_id": "idle" } ],
            "modules": [],
            "transitions": []
        }
    ]
})";

static const char* kMissingVersion = R"({
    "processing_units": []
})";

static const char* kMissingProcessingUnits = R"({
    "version": 1
})";

static const char* kMalformed = R"({ bad json )";

static const char* kTmpPath = "C:\\Temp\\dia_test_manifest_serializer.json";

TEST(ApplicationManifestSerializer, Load_ValidManifest)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    ASSERT_TRUE(s.Load(kValidManifest, manifest));

    EXPECT_EQ(manifest.version, 1u);
    EXPECT_EQ(manifest.processingUnits.Size(), 1u);

    const auto& pu = manifest.processingUnits[0];
    EXPECT_EQ(pu.typeId,     Dia::Core::StringCRC("MainProcessingUnit"));
    EXPECT_EQ(pu.instanceId, Dia::Core::StringCRC("main_pu"));
    EXPECT_FLOAT_EQ(pu.frequencyHz, 60.0f);
    EXPECT_FALSE(pu.dedicatedThread);
    EXPECT_EQ(pu.initialPhase, Dia::Core::StringCRC("init"));

    EXPECT_EQ(pu.phases.Size(), 2u);
    EXPECT_EQ(pu.phases[0].typeId, Dia::Core::StringCRC("InitPhase"));
    EXPECT_EQ(pu.phases[1].typeId, Dia::Core::StringCRC("UpdatePhase"));

    EXPECT_EQ(pu.modules.Size(), 1u);
    EXPECT_EQ(pu.modules[0].typeId, Dia::Core::StringCRC("RenderModule"));
    EXPECT_EQ(pu.modules[0].phaseIds.Size(), 1u);
    EXPECT_EQ(pu.modules[0].phaseIds[0], Dia::Core::StringCRC("update"));

    EXPECT_EQ(pu.transitions.Size(), 1u);
    EXPECT_EQ(pu.transitions[0].fromPhase, Dia::Core::StringCRC("init"));
    EXPECT_EQ(pu.transitions[0].toPhase,   Dia::Core::StringCRC("update"));
}

TEST(ApplicationManifestSerializer, Load_Minimal)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    ASSERT_TRUE(s.Load(kValidMinimal, manifest));

    EXPECT_EQ(manifest.processingUnits.Size(), 1u);
    EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 1u);
    EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 0u);
    EXPECT_EQ(manifest.processingUnits[0].transitions.Size(), 0u);
}

TEST(ApplicationManifestSerializer, Load_MissingVersion_Fails)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    auto result = s.Load(kMissingVersion, manifest);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(ApplicationManifestSerializer, Load_MissingProcessingUnits_Fails)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    EXPECT_FALSE(s.Load(kMissingProcessingUnits, manifest));
}

TEST(ApplicationManifestSerializer, Load_Malformed_Fails)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    auto result = s.Load(kMalformed, manifest);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
    EXPECT_GT(strlen(result.error), 0u);
}

TEST(ApplicationManifestSerializer, RoundTrip)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    ASSERT_TRUE(s.Load(kValidManifest, manifest));

    char buffer[8192];
    ASSERT_TRUE(s.Save(manifest, buffer, sizeof(buffer)));

    ApplicationManifest manifest2;
    ASSERT_TRUE(s.Load(buffer, manifest2));

    EXPECT_EQ(manifest2.version, manifest.version);
    EXPECT_EQ(manifest2.processingUnits.Size(), manifest.processingUnits.Size());

    const auto& pu1 = manifest.processingUnits[0];
    const auto& pu2 = manifest2.processingUnits[0];
    EXPECT_EQ(pu2.typeId,      pu1.typeId);
    EXPECT_EQ(pu2.instanceId,  pu1.instanceId);
    EXPECT_FLOAT_EQ(pu2.frequencyHz, pu1.frequencyHz);
    EXPECT_EQ(pu2.initialPhase, pu1.initialPhase);
    EXPECT_EQ(pu2.phases.Size(), pu1.phases.Size());
    EXPECT_EQ(pu2.modules.Size(), pu1.modules.Size());
    EXPECT_EQ(pu2.transitions.Size(), pu1.transitions.Size());
}

TEST(ApplicationManifestSerializer, RoundTrip_PhaseTransitions_Preserved)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    ASSERT_TRUE(s.Load(kValidManifest, manifest));

    char buffer[8192];
    ASSERT_TRUE(s.Save(manifest, buffer, sizeof(buffer)));

    ApplicationManifest manifest2;
    ASSERT_TRUE(s.Load(buffer, manifest2));

    const auto& t1 = manifest.processingUnits[0].transitions[0];
    const auto& t2 = manifest2.processingUnits[0].transitions[0];
    EXPECT_EQ(t2.fromPhase, t1.fromPhase);
    EXPECT_EQ(t2.toPhase,   t1.toPhase);
}

TEST(ApplicationManifestSerializer, SaveToFileAndLoadFromFile)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    ASSERT_TRUE(s.Load(kValidManifest, manifest));

    ASSERT_TRUE(s.SaveToFile(kTmpPath, manifest));

    ApplicationManifest loaded;
    ASSERT_TRUE(s.LoadFromFile(kTmpPath, loaded));

    EXPECT_EQ(loaded.version, manifest.version);
    EXPECT_EQ(loaded.processingUnits.Size(), manifest.processingUnits.Size());
    EXPECT_EQ(loaded.processingUnits[0].typeId, manifest.processingUnits[0].typeId);

    remove(kTmpPath);
}

TEST(ApplicationManifestSerializer, LoadFromFile_Missing_Fails)
{
    JsonApplicationManifestSerializer s;
    ApplicationManifest manifest;
    auto result = s.LoadFromFile("C:\\Temp\\nonexistent_manifest.json", manifest);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error, nullptr);
}

TEST(ApplicationManifestSerializer, GetVersion_NotEmpty)
{
    JsonApplicationManifestSerializer s;
    const char* v = s.GetVersion();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(strlen(v), 0u);
}
