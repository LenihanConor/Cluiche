// TestTypeRegistrySeeding.cpp - Integration tests for editor-mode type registry seeding
//
// The DiaApplicationEditor uses RegisterKnownXxxType to "seed" the registry with
// type names extracted from a manifest, without providing factories. This allows
// ManifestValidator to pass IsXxxTypeRegistered checks in offline mode.

#include <gtest/gtest.h>
#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoader.h>
#include <DiaApplicationFlow/Manifest/ManifestValidator.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Application;
using namespace Dia::Core;

// ==============================================================================
// Minimal test PU (needed to call CreatePhase which requires a non-null PU ptr)
// ==============================================================================

class SeedingTestPU : public ProcessingUnit
{
public:
    SeedingTestPU() : ProcessingUnit(StringCRC("SeedingTestPU"), 60.0f, 4, 4) {}
    bool FlaggedToStopUpdating() const override { return true; }
};

// ==============================================================================
// Test Fixture
// ==============================================================================

class TypeRegistrySeedingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mRegistry = new ApplicationTypeRegistry();
        mRegistry->DrainPendingRegistrations();
    }

    void TearDown() override
    {
        delete mRegistry;
        mRegistry = nullptr;
    }

    // Seed the registry with all types declared in a manifest (editor offline mode)
    void SeedFromManifest(const ApplicationManifest& manifest)
    {
        for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
        {
            const auto& pu = manifest.processingUnits[i];
            mRegistry->RegisterKnownProcessingUnitType(pu.typeId);

            for (unsigned int j = 0; j < pu.phases.Size(); ++j)
                mRegistry->RegisterKnownPhaseType(pu.phases[j].typeId);

            for (unsigned int j = 0; j < pu.modules.Size(); ++j)
                mRegistry->RegisterKnownModuleType(pu.modules[j].typeId);
        }
    }

    ApplicationTypeRegistry* mRegistry = nullptr;
};

// ==============================================================================
// RegisterKnownXxx Tests
// ==============================================================================

TEST_F(TypeRegistrySeedingTest, RegisterKnownPU_IsRegistered)
{
    StringCRC typeId("OfflinePU");
    mRegistry->RegisterKnownProcessingUnitType(typeId);
    EXPECT_TRUE(mRegistry->IsProcessingUnitTypeRegistered(typeId));
}

TEST_F(TypeRegistrySeedingTest, RegisterKnownPhase_IsRegistered)
{
    StringCRC typeId("OfflinePhase");
    mRegistry->RegisterKnownPhaseType(typeId);
    EXPECT_TRUE(mRegistry->IsPhaseTypeRegistered(typeId));
}

TEST_F(TypeRegistrySeedingTest, RegisterKnownModule_IsRegistered)
{
    StringCRC typeId("OfflineModule");
    mRegistry->RegisterKnownModuleType(typeId);
    EXPECT_TRUE(mRegistry->IsModuleTypeRegistered(typeId));
}

TEST_F(TypeRegistrySeedingTest, RegisterKnownPU_CreateReturnsNull_NoFactory)
{
    StringCRC typeId("StubPU");
    mRegistry->RegisterKnownProcessingUnitType(typeId);

    Json::Value config;
    ProcessingUnit* pu = mRegistry->CreateProcessingUnit(typeId, StringCRC("inst"), config);
    EXPECT_EQ(pu, nullptr);
}

TEST_F(TypeRegistrySeedingTest, RegisterKnownPhase_CreateReturnsNull_NoFactory)
{
    StringCRC typeId("StubPhase");
    mRegistry->RegisterKnownPhaseType(typeId);

    SeedingTestPU pu;
    Json::Value config;
    Phase* phase = mRegistry->CreatePhase(typeId, &pu, StringCRC("inst"), config);
    EXPECT_EQ(phase, nullptr);
}

// ==============================================================================
// Seeding From Manifest Tests
// ==============================================================================

TEST_F(TypeRegistrySeedingTest, SeedFromManifest_PUTypeIsRegistered)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("GamePU");
    pu.instanceId = StringCRC("GamePUInst");
    pu.frequencyHz = 60.0f;
    manifest.processingUnits.Add(pu);

    SeedFromManifest(manifest);

    EXPECT_TRUE(mRegistry->IsProcessingUnitTypeRegistered(StringCRC("GamePU")));
}

TEST_F(TypeRegistrySeedingTest, SeedFromManifest_PhaseTypesAreRegistered)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("GamePU");
    pu.instanceId = StringCRC("GamePUInst");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::PhaseEntry phase1;
    phase1.typeId = StringCRC("InitPhaseType");
    phase1.instanceId = StringCRC("Init");
    pu.phases.Add(phase1);

    ApplicationManifest::PhaseEntry phase2;
    phase2.typeId = StringCRC("UpdatePhaseType");
    phase2.instanceId = StringCRC("Update");
    pu.phases.Add(phase2);

    manifest.processingUnits.Add(pu);

    SeedFromManifest(manifest);

    EXPECT_TRUE(mRegistry->IsPhaseTypeRegistered(StringCRC("InitPhaseType")));
    EXPECT_TRUE(mRegistry->IsPhaseTypeRegistered(StringCRC("UpdatePhaseType")));
}

TEST_F(TypeRegistrySeedingTest, SeedFromManifest_ModuleTypesAreRegistered)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("GamePU");
    pu.instanceId = StringCRC("GamePUInst");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("InputModuleType");
    mod.instanceId = StringCRC("InputMod");
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    SeedFromManifest(manifest);

    EXPECT_TRUE(mRegistry->IsModuleTypeRegistered(StringCRC("InputModuleType")));
}

TEST_F(TypeRegistrySeedingTest, SeedFromManifest_UnseededType_NotRegistered)
{
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("SeededPU");
    pu.instanceId = StringCRC("SeededPUInst");
    pu.frequencyHz = 60.0f;
    manifest.processingUnits.Add(pu);

    SeedFromManifest(manifest);

    // A type NOT in the manifest should not appear registered
    EXPECT_FALSE(mRegistry->IsProcessingUnitTypeRegistered(StringCRC("NotInManifestPU")));
    EXPECT_FALSE(mRegistry->IsPhaseTypeRegistered(StringCRC("NotInManifestPhase")));
    EXPECT_FALSE(mRegistry->IsModuleTypeRegistered(StringCRC("NotInManifestModule")));
}

TEST_F(TypeRegistrySeedingTest, SeedFromManifest_RegisterKnownIdempotent)
{
    StringCRC typeId("DuplicateType");
    mRegistry->RegisterKnownProcessingUnitType(typeId);
    mRegistry->RegisterKnownProcessingUnitType(typeId);  // second call should be a no-op

    EXPECT_TRUE(mRegistry->IsProcessingUnitTypeRegistered(typeId));

    // Count should only have one entry
    const auto& types = mRegistry->GetRegisteredProcessingUnitTypes();
    int count = 0;
    for (unsigned int i = 0; i < types.Size(); ++i)
    {
        if (types[i] == typeId)
            count++;
    }
    EXPECT_EQ(count, 1);
}

// ==============================================================================
// Validation With Seeded Registry
// ==============================================================================

TEST_F(TypeRegistrySeedingTest, SeedThenValidate_KnownTypes_NoUnknownTypeError)
{
    // Build a simple manifest
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("SimPU");
    pu.instanceId = StringCRC("SimPUInst");
    pu.frequencyHz = 60.0f;

    ApplicationManifest::PhaseEntry phase;
    phase.typeId = StringCRC("SimPhase");
    phase.instanceId = StringCRC("SimPhaseInst");
    pu.phases.Add(phase);

    pu.initialPhase = StringCRC("SimPhaseInst");

    ApplicationManifest::ModuleEntry mod;
    mod.typeId = StringCRC("SimModule");
    mod.instanceId = StringCRC("SimModInst");
    mod.phaseIds.Add(StringCRC("SimPhaseInst"));
    pu.modules.Add(mod);

    manifest.processingUnits.Add(pu);

    // Seed registry from manifest
    SeedFromManifest(manifest);

    // Validate — should pass type checks
    ManifestValidator validator(*mRegistry);
    ManifestValidationResult result = validator.Validate(manifest);

    // Check no kUnknownType error
    const auto& errors = validator.GetErrors();
    for (unsigned int i = 0; i < errors.Size(); ++i)
    {
        EXPECT_NE(errors[i].code, ManifestValidationResult::kUnknownType)
            << "Unexpected kUnknownType for seeded type: " << errors[i].message.AsCStr();
    }
}

TEST_F(TypeRegistrySeedingTest, ValidateWithoutSeeding_UnregisteredType_HasUnknownTypeError)
{
    // Manifest with a type NOT registered in the registry
    ApplicationManifest manifest;
    manifest.version = 1;

    ApplicationManifest::ProcessingUnitEntry pu;
    pu.typeId = StringCRC("UnknownPU");
    pu.instanceId = StringCRC("UnknownPUInst");
    pu.frequencyHz = 60.0f;
    manifest.processingUnits.Add(pu);

    // Do NOT seed — registry is empty
    ManifestValidator validator(*mRegistry);
    validator.Validate(manifest);

    const auto& errors = validator.GetErrors();
    bool foundUnknownType = false;
    for (unsigned int i = 0; i < errors.Size(); ++i)
    {
        if (errors[i].code == ManifestValidationResult::kUnknownType)
        {
            foundUnknownType = true;
            break;
        }
    }
    EXPECT_TRUE(foundUnknownType);
}
