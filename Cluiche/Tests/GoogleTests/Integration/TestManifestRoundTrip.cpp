// TestManifestRoundTrip.cpp - Integration tests for manifest load and serialization
//
// Tests that:
//  1. Loading a manifest from a JSON string produces the correct in-memory structure
//  2. Serializing that structure to JSON produces correct output shape
//
// Note: ManifestSerializer uses a different schema (key "type") than ApplicationManifestLoader
// (key "type_id") — the serializer output targets the React UI, not the loader. These tests
// verify each direction independently.

#include <gtest/gtest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoader.h>
#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplicationEditor/ManifestSerializer.h>
#include <DiaApplicationEditor/ManifestEditorData.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>

#include <sstream>

using namespace Dia::Application;
using namespace Dia::Application::Editor;
using namespace Dia::Core;

// ==============================================================================
// Test Types
// ==============================================================================

class RoundTripTestProcessingUnit : public ProcessingUnit
{
public:
    static const StringCRC kTypeId;
    RoundTripTestProcessingUnit(const StringCRC& instanceId, float hz)
        : ProcessingUnit(instanceId, hz, 16, 16) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC RoundTripTestProcessingUnit::kTypeId("RoundTripTestProcessingUnit");

class RoundTripTestPhase : public Phase
{
public:
    static const StringCRC kTypeId;
    RoundTripTestPhase(ProcessingUnit* pu, const StringCRC& instanceId)
        : Phase(pu, instanceId) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC RoundTripTestPhase::kTypeId("RoundTripTestPhase");

class RoundTripTestModule : public Module
{
public:
    static const StringCRC kTypeId;
    RoundTripTestModule(ProcessingUnit* pu, const StringCRC& instanceId)
        : Module(pu, instanceId, RunningEnum::kUpdate) {}
    virtual StateObject::OpertionResponse DoStart(const IStartData*) override
    {
        return StateObject::OpertionResponse::kImmediate;
    }
    virtual void DoUpdate() override {}
    virtual void DoStop() override {}
};
const StringCRC RoundTripTestModule::kTypeId("RoundTripTestModule");

// ==============================================================================
// Test Fixture
// ==============================================================================

class ManifestRoundTripTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mRegistry = new ApplicationTypeRegistry();
        mRegistry->DrainPendingRegistrations();

        class PUFactory : public ITypeFactory<ProcessingUnit>
        {
        public:
            ProcessingUnit* Create(const StringCRC& instanceId, const Json::Value& config) override
            {
                return new RoundTripTestProcessingUnit(instanceId, config.get("hz", 60.0f).asFloat());
            }
        };
        class PhaseFactory : public ITypeFactory<Phase>
        {
        public:
            Phase* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value&) override
            {
                return new RoundTripTestPhase(pu, instanceId);
            }
        };
        class ModuleFactory : public ITypeFactory<Module>
        {
        public:
            Module* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value&) override
            {
                return new RoundTripTestModule(pu, instanceId);
            }
        };

        static PUFactory puFactory;
        static PhaseFactory phaseFactory;
        static ModuleFactory moduleFactory;

        mRegistry->RegisterProcessingUnitType(RoundTripTestProcessingUnit::kTypeId, &puFactory);
        mRegistry->RegisterPhaseType(RoundTripTestPhase::kTypeId, &phaseFactory);
        mRegistry->RegisterModuleType(RoundTripTestModule::kTypeId, &moduleFactory);
    }

    void TearDown() override
    {
        delete mRegistry;
        mRegistry = nullptr;
    }

    ApplicationManifest LoadFromString(const char* json)
    {
        ApplicationManifestLoader loader(*mRegistry);
        ApplicationManifest manifest;
        loader.LoadFromString(json, manifest);
        return manifest;
    }

    ApplicationTypeRegistry* mRegistry = nullptr;

    static const char* kFullManifestJson;
};

const char* ManifestRoundTripTest::kFullManifestJson = R"({
    "version": 1,
    "processing_units": [
        {
            "type_id": "RoundTripTestProcessingUnit",
            "instance_id": "MainPU",
            "frequency_hz": 60.0,
            "dedicated_thread": false,
            "phases": [
                { "type_id": "RoundTripTestPhase", "instance_id": "InitPhase" },
                { "type_id": "RoundTripTestPhase", "instance_id": "UpdatePhase" }
            ],
            "transitions": [
                { "from": "InitPhase", "to": "UpdatePhase" }
            ],
            "initial_phase": "InitPhase",
            "modules": [
                {
                    "type_id": "RoundTripTestModule",
                    "instance_id": "InputMod",
                    "phase_ids": ["InitPhase", "UpdatePhase"],
                    "dependencies": []
                }
            ]
        }
    ]
})";

// ==============================================================================
// Load → In-Memory Structure Tests
// ==============================================================================

TEST_F(ManifestRoundTripTest, Load_PreservesVersion)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.version, 1u);
}

TEST_F(ManifestRoundTripTest, Load_CorrectProcessingUnitCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits.Size(), 1u);
}

TEST_F(ManifestRoundTripTest, Load_CorrectPhaseCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 2u);
}

TEST_F(ManifestRoundTripTest, Load_CorrectModuleCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 1u);
}

TEST_F(ManifestRoundTripTest, Load_CorrectTransitionCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits[0].transitions.Size(), 1u);
}

TEST_F(ManifestRoundTripTest, Load_PhaseInstanceIdsCorrect)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits[0].phases[0].instanceId, StringCRC("InitPhase"));
    EXPECT_EQ(manifest.processingUnits[0].phases[1].instanceId, StringCRC("UpdatePhase"));
}

TEST_F(ManifestRoundTripTest, Load_InitialPhaseCorrect)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_EQ(manifest.processingUnits[0].initialPhase, StringCRC("InitPhase"));
}

TEST_F(ManifestRoundTripTest, Load_TransitionFromToCorrect)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    const auto& t = manifest.processingUnits[0].transitions[0];
    EXPECT_EQ(t.fromPhase, StringCRC("InitPhase"));
    EXPECT_EQ(t.toPhase, StringCRC("UpdatePhase"));
}

TEST_F(ManifestRoundTripTest, Load_ModulePhaseIdsCorrect)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    const auto& mod = manifest.processingUnits[0].modules[0];
    ASSERT_EQ(mod.phaseIds.Size(), 2u);
    EXPECT_EQ(mod.phaseIds[0], StringCRC("InitPhase"));
    EXPECT_EQ(mod.phaseIds[1], StringCRC("UpdatePhase"));
}

TEST_F(ManifestRoundTripTest, Load_FrequencyCorrect)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    EXPECT_FLOAT_EQ(manifest.processingUnits[0].frequencyHz, 60.0f);
}

// ==============================================================================
// In-Memory → Serialize Tests
// ==============================================================================

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_HasVersionKey)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ASSERT_TRUE(ManifestSerializer::Serialize(manifest, outJson));
    EXPECT_EQ(outJson["version"].asUInt(), 1u);
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_CorrectPUCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_EQ(outJson["processing_units"].size(), 1u);
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_CorrectPhaseCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_EQ(outJson["processing_units"][0]["phases"].size(), 2u);
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_CorrectModuleCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_EQ(outJson["processing_units"][0]["modules"].size(), 1u);
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_CorrectTransitionCount)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_EQ(outJson["processing_units"][0]["transitions"].size(), 1u);
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_PhaseInstanceIdsPreserved)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& phases = outJson["processing_units"][0]["phases"];
    EXPECT_STREQ(phases[0]["instance_id"].asCString(), "InitPhase");
    EXPECT_STREQ(phases[1]["instance_id"].asCString(), "UpdatePhase");
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_TransitionFromToPreserved)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& t = outJson["processing_units"][0]["transitions"][0];
    EXPECT_STREQ(t["from"].asCString(), "InitPhase");
    EXPECT_STREQ(t["to"].asCString(), "UpdatePhase");
}

TEST_F(ManifestRoundTripTest, Serialize_AfterLoad_FrequencyPreserved)
{
    ApplicationManifest manifest = LoadFromString(kFullManifestJson);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_FLOAT_EQ(outJson["processing_units"][0]["frequency_hz"].asFloat(), 60.0f);
}

// ==============================================================================
// ManifestEditorData State Tests
// ==============================================================================

TEST_F(ManifestRoundTripTest, ManifestEditorData_DefaultState_NotDirty)
{
    ManifestEditorData data;
    EXPECT_FALSE(data.isDirty);
}

TEST_F(ManifestRoundTripTest, ManifestEditorData_DefaultState_NoManifest)
{
    ManifestEditorData data;
    EXPECT_FALSE(data.hasManifest);
}

// ==============================================================================
// Invalid Input Tests
// ==============================================================================

TEST_F(ManifestRoundTripTest, LoadFromString_InvalidJson_ReturnsError)
{
    ApplicationManifestLoader loader(*mRegistry);
    ApplicationManifest manifest;
    ManifestValidationResult result = loader.LoadFromString("{ not valid json }", manifest);
    EXPECT_NE(result, ManifestValidationResult::kSuccess);
}

TEST_F(ManifestRoundTripTest, LoadFromString_EmptyString_ReturnsError)
{
    ApplicationManifestLoader loader(*mRegistry);
    ApplicationManifest manifest;
    ManifestValidationResult result = loader.LoadFromString("", manifest);
    EXPECT_NE(result, ManifestValidationResult::kSuccess);
}

TEST_F(ManifestRoundTripTest, LoadFromString_InvalidJson_HasErrors)
{
    ApplicationManifestLoader loader(*mRegistry);
    ApplicationManifest manifest;
    loader.LoadFromString("{ not valid json }", manifest);
    EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestRoundTripTest, LoadFromString_InvalidJson_ManifestUnchanged)
{
    ApplicationManifestLoader loader(*mRegistry);
    ApplicationManifest manifest;
    loader.LoadFromString("{ not valid json }", manifest);
    EXPECT_EQ(manifest.processingUnits.Size(), 0u);
}
