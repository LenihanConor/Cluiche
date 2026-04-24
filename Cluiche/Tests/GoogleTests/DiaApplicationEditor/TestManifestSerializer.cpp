// TestManifestSerializer.cpp - Unit tests for ManifestSerializer
//
// Tests JSON serialization of ApplicationManifest IR

#include <gtest/gtest.h>
#include <DiaApplicationEditor/ManifestSerializer.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Application;
using namespace Dia::Application::Editor;
using namespace Dia::Core;

// ==============================================================================
// Test Helpers
// ==============================================================================

static ApplicationManifest MakeEmptyManifest()
{
    ApplicationManifest manifest;
    manifest.version = 1;
    return manifest;
}

static ApplicationManifest MakeManifestWithOnePU(
    const char* typeId = "TestPU",
    const char* instanceId = "TestPUInstance",
    float hz = 60.0f)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC(typeId);
    pu.instanceId = StringCRC(instanceId);
    pu.frequencyHz = hz;
    pu.dedicatedThread = false;

    manifest.processingUnits.Add(pu);
    return manifest;
}

// ==============================================================================
// Manifest-level Tests
// ==============================================================================

TEST(ManifestSerializer, Serialize_ReturnsTrue)
{
    ApplicationManifest manifest = MakeEmptyManifest();
    Json::Value outJson;
    EXPECT_TRUE(ManifestSerializer::Serialize(manifest, outJson));
}

TEST(ManifestSerializer, Serialize_HasVersionKey)
{
    ApplicationManifest manifest = MakeEmptyManifest();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    ASSERT_TRUE(outJson.isMember("version"));
    EXPECT_EQ(outJson["version"].asUInt(), 1u);
}

TEST(ManifestSerializer, Serialize_HasProcessingUnitsArray)
{
    ApplicationManifest manifest = MakeEmptyManifest();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    ASSERT_TRUE(outJson.isMember("processing_units"));
    EXPECT_TRUE(outJson["processing_units"].isArray());
}

TEST(ManifestSerializer, Serialize_EmptyManifest_NoPUs)
{
    ApplicationManifest manifest = MakeEmptyManifest();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_EQ(outJson["processing_units"].size(), 0u);
}

TEST(ManifestSerializer, Serialize_TwoPUs_ArrayHasTwoEntries)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu1;
    pu1.typeId = StringCRC("PU1");
    pu1.instanceId = StringCRC("PU1");
    pu1.frequencyHz = 60.0f;
    manifest.processingUnits.Add(pu1);

    ApplicationManifest::ProcessingUnitEntry pu2;
    pu2.typeId = StringCRC("PU2");
    pu2.instanceId = StringCRC("PU2");
    pu2.frequencyHz = 30.0f;
    manifest.processingUnits.Add(pu2);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_EQ(outJson["processing_units"].size(), 2u);
}

TEST(ManifestSerializer, Serialize_NoImports_NoImportsKey)
{
    ApplicationManifest manifest = MakeEmptyManifest();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);
    EXPECT_FALSE(outJson.isMember("imports"));
}

// ==============================================================================
// ProcessingUnit Field Tests
// ==============================================================================

TEST(ManifestSerializer, Serialize_PU_HasRequiredFields)
{
    ApplicationManifest manifest = MakeManifestWithOnePU();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& pu = outJson["processing_units"][0];
    EXPECT_TRUE(pu.isMember("type"));
    EXPECT_TRUE(pu.isMember("instance_id"));
    EXPECT_TRUE(pu.isMember("frequency_hz"));
    EXPECT_TRUE(pu.isMember("dedicated_thread"));
    EXPECT_TRUE(pu.isMember("phases"));
    EXPECT_TRUE(pu.isMember("transitions"));
    EXPECT_TRUE(pu.isMember("modules"));
}

TEST(ManifestSerializer, Serialize_PU_TypeAndInstanceIdMatchInput)
{
    ApplicationManifest manifest = MakeManifestWithOnePU("MyPUType", "MyPUInstance");
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& pu = outJson["processing_units"][0];
    EXPECT_STREQ(pu["type"].asCString(), "MyPUType");
    EXPECT_STREQ(pu["instance_id"].asCString(), "MyPUInstance");
}

TEST(ManifestSerializer, Serialize_PU_FrequencyPreserved)
{
    ApplicationManifest manifest = MakeManifestWithOnePU("PU", "PU", 30.0f);
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_FLOAT_EQ(outJson["processing_units"][0]["frequency_hz"].asFloat(), 30.0f);
}

TEST(ManifestSerializer, Serialize_PU_WithInitialPhase_HasInitialPhaseKey)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;
    pu.initialPhase = StringCRC("InitPhase");
    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& puJson = outJson["processing_units"][0];
    ASSERT_TRUE(puJson.isMember("initial_phase"));
    EXPECT_STREQ(puJson["initial_phase"].asCString(), "InitPhase");
}

TEST(ManifestSerializer, Serialize_PU_WithoutInitialPhase_NoInitialPhaseKey)
{
    ApplicationManifest manifest = MakeManifestWithOnePU();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_FALSE(outJson["processing_units"][0].isMember("initial_phase"));
}

TEST(ManifestSerializer, Serialize_PU_NoPhasesOrModules_EmptyArrays)
{
    ApplicationManifest manifest = MakeManifestWithOnePU();
    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& pu = outJson["processing_units"][0];
    EXPECT_EQ(pu["phases"].size(), 0u);
    EXPECT_EQ(pu["transitions"].size(), 0u);
    EXPECT_EQ(pu["modules"].size(), 0u);
}

// ==============================================================================
// Phase Tests
// ==============================================================================

TEST(ManifestSerializer, Serialize_Phase_TypeAndInstanceIdPreserved)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::PhaseEntry phase;
    phase.typeId = StringCRC("UpdatePhase");
    phase.instanceId = StringCRC("UpdatePhaseInst");
    pu.phases.Add(phase);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& phaseJson = outJson["processing_units"][0]["phases"][0];
    EXPECT_STREQ(phaseJson["type"].asCString(), "UpdatePhase");
    EXPECT_STREQ(phaseJson["instance_id"].asCString(), "UpdatePhaseInst");
}

// ==============================================================================
// Transition Tests
// ==============================================================================

TEST(ManifestSerializer, Serialize_Transitions_FromAndToPreserved)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::PhaseTransition t;
    t.fromPhase = StringCRC("PhaseA");
    t.toPhase = StringCRC("PhaseB");
    pu.transitions.Add(t);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& transitions = outJson["processing_units"][0]["transitions"];
    ASSERT_EQ(transitions.size(), 1u);
    EXPECT_STREQ(transitions[0]["from"].asCString(), "PhaseA");
    EXPECT_STREQ(transitions[0]["to"].asCString(), "PhaseB");
}

TEST(ManifestSerializer, Serialize_MultipleTransitions_AllPreserved)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::PhaseTransition t1;
    t1.fromPhase = StringCRC("Init");
    t1.toPhase = StringCRC("Update");
    pu.transitions.Add(t1);

    ApplicationManifest::PhaseTransition t2;
    t2.fromPhase = StringCRC("Update");
    t2.toPhase = StringCRC("Shutdown");
    pu.transitions.Add(t2);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& transitions = outJson["processing_units"][0]["transitions"];
    EXPECT_EQ(transitions.size(), 2u);
}

// ==============================================================================
// Module Tests
// ==============================================================================

TEST(ManifestSerializer, Serialize_Module_TypeAndInstanceIdPreserved)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("InputModule");
    mod.instanceId = StringCRC("InputMod");
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& modJson = outJson["processing_units"][0]["modules"][0];
    EXPECT_STREQ(modJson["type"].asCString(), "InputModule");
    EXPECT_STREQ(modJson["instance_id"].asCString(), "InputMod");
}

TEST(ManifestSerializer, Serialize_Module_WithPhaseIds_PreservedInPhasesArray)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("TestModule");
    mod.instanceId = StringCRC("TestMod");
    mod.phaseIds.Add(StringCRC("InitPhase"));
    mod.phaseIds.Add(StringCRC("UpdatePhase"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& phases = outJson["processing_units"][0]["modules"][0]["phases"];
    ASSERT_EQ(phases.size(), 2u);
    EXPECT_STREQ(phases[0].asCString(), "InitPhase");
    EXPECT_STREQ(phases[1].asCString(), "UpdatePhase");
}

TEST(ManifestSerializer, Serialize_Module_WithDependencies_PreservedInDepsArray)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("TestModule");
    mod.instanceId = StringCRC("TestMod");
    mod.dependencies.Add(StringCRC("LoggerModule"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    const Json::Value& deps = outJson["processing_units"][0]["modules"][0]["dependencies"];
    ASSERT_EQ(deps.size(), 1u);
    EXPECT_STREQ(deps[0].asCString(), "LoggerModule");
}

TEST(ManifestSerializer, Serialize_Module_NoDependencies_NoDepsKey)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("TestModule");
    mod.instanceId = StringCRC("TestMod");
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_FALSE(outJson["processing_units"][0]["modules"][0].isMember("dependencies"));
}

TEST(ManifestSerializer, Serialize_Module_NoPhaseIds_NoPhasesKey)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("TestPU");
    pu.instanceId = StringCRC("TestPU");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("TestModule");
    mod.instanceId = StringCRC("TestMod");
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    Json::Value outJson;
    ManifestSerializer::Serialize(manifest, outJson);

    EXPECT_FALSE(outJson["processing_units"][0]["modules"][0].isMember("phases"));
}
