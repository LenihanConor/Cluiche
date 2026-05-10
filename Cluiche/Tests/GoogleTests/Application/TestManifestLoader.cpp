// TestManifestLoader.cpp - Unit tests for ApplicationManifestLoader
//
// Tests manifest loading, validation, and instantiation

#include <gtest/gtest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoader.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifest.h>
#include <DiaApplicationFlow/Manifest/ManifestValidator.h>
#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Application;
using namespace Dia::Core;

// ==============================================================================
// Test Types (for instantiation tests)
// ==============================================================================

class ManifestTestProcessingUnit : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	ManifestTestProcessingUnit(const StringCRC& instanceId, float hz)
		: ProcessingUnit(instanceId, hz, 16, 16) {}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC ManifestTestProcessingUnit::kTypeId("ManifestTestProcessingUnit");

class ManifestTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	ManifestTestPhase(ProcessingUnit* pu, const StringCRC& instanceId)
		: Phase(pu, instanceId) {}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC ManifestTestPhase::kTypeId("ManifestTestPhase");

class ManifestTestModule : public Module
{
public:
	static const StringCRC kTypeId;
	ManifestTestModule(ProcessingUnit* pu, const StringCRC& instanceId)
		: Module(pu, instanceId, RunningEnum::kUpdate) {}
	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};
const StringCRC ManifestTestModule::kTypeId("ManifestTestModule");

// ==============================================================================
// Test Fixture
// ==============================================================================

class ManifestLoaderTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mRegistry = new ApplicationTypeRegistry();
		mRegistry->DrainPendingRegistrations();
		RegisterTestTypes();
	}

	void TearDown() override
	{
		delete mRegistry;
		mRegistry = nullptr;
	}

	void RegisterTestTypes()
	{
		class TestPUFactory : public ITypeFactory<ProcessingUnit>
		{
		public:
			virtual ProcessingUnit* Create(const StringCRC& instanceId, const Json::Value& config) override
			{
				float hz = config.get("hz", 60.0f).asFloat();
				return new ManifestTestProcessingUnit(instanceId, hz);
			}
		};

		class TestPhaseFactory : public ITypeFactory<Phase>
		{
		public:
			virtual Phase* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
			{
				return new ManifestTestPhase(pu, instanceId);
			}
		};

		class TestModuleFactory : public ITypeFactory<Module>
		{
		public:
			virtual Module* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
			{
				return new ManifestTestModule(pu, instanceId);
			}
		};

		static TestPUFactory puFactory;
		static TestPhaseFactory phaseFactory;
		static TestModuleFactory moduleFactory;

		mRegistry->RegisterProcessingUnitType(ManifestTestProcessingUnit::kTypeId, &puFactory);
		mRegistry->RegisterPhaseType(ManifestTestPhase::kTypeId, &phaseFactory);
		mRegistry->RegisterModuleType(ManifestTestModule::kTypeId, &moduleFactory);
	}

	ApplicationTypeRegistry* mRegistry = nullptr;
};

// ==============================================================================
// Valid Manifest Tests
// ==============================================================================

TEST_F(ManifestLoaderTest, LoadFromString_ValidManifest_Success)
{
	const char* validJson = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "MainPU",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "InitPhase"
					},
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "UpdatePhase"
					}
				],
				"transitions": [
					{
						"from": "InitPhase",
						"to": "UpdatePhase"
					}
				],
				"initial_phase": "InitPhase",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "TestMod1",
						"phase_ids": ["InitPhase", "UpdatePhase"],
						"dependencies": []
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(validJson, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.version, 1u);
	EXPECT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 2u);
	EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 1u);
	EXPECT_EQ(manifest.processingUnits[0].transitions.Size(), 1u);
}

TEST_F(ManifestLoaderTest, LoadFromString_MinimalManifest_Success)
{
	const char* minimalJson = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "SimplePU",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [],
				"transitions": [],
				"initial_phase": "",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(minimalJson, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 0u);
	EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 0u);
}

// ==============================================================================
// Invalid JSON Tests
// ==============================================================================

TEST_F(ManifestLoaderTest, LoadFromString_InvalidJSON_ReturnsError)
{
	const char* invalidJson = "{ this is not valid json }";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(invalidJson, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kInvalidJSON);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, LoadFromString_EmptyString_ReturnsError)
{
	const char* emptyJson = "";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(emptyJson, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kInvalidJSON);
}

// ==============================================================================
// Missing Required Fields Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, LoadFromString_MissingVersion_ReturnsError)
{
	const char* noVersion = R"({
		"processing_units": []
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(noVersion, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, LoadFromString_MissingProcessingUnits_ReturnsError)
{
	const char* noPUs = R"({
		"version": 1
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(noPUs, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);
}

TEST_F(ManifestLoaderTest, LoadFromString_MissingProcessingUnitType_ReturnsError)
{
	const char* noType = R"({
		"version": 1,
		"processing_units": [
			{
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(noType, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);
}

TEST_F(ManifestLoaderTest, LoadFromString_MissingProcessingUnitInstanceId_ReturnsError)
{
	const char* noInstanceId = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"hz": 60.0,
				"dedicated_thread": false
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(noInstanceId, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);
}

// ==============================================================================
// Unknown Type Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, Validate_UnknownProcessingUnitType_ReturnsError)
{
	const char* unknownType = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "NonExistentProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [],
				"transitions": [],
				"initial_phase": "",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(unknownType, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kUnknownType);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, Validate_UnknownPhaseType_ReturnsError)
{
	const char* unknownPhase = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "NonExistentPhase",
						"instance_id": "BadPhase"
					}
				],
				"transitions": [],
				"initial_phase": "",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(unknownPhase, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kUnknownType);
}

TEST_F(ManifestLoaderTest, Validate_UnknownModuleType_ReturnsError)
{
	const char* unknownModule = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "NonExistentModule",
						"instance_id": "BadModule",
						"phase_ids": ["Phase1"],
						"dependencies": []
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(unknownModule, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kUnknownType);
}

// ==============================================================================
// Schema Version Tests (AC13)
// ==============================================================================

TEST_F(ManifestLoaderTest, Validate_UnsupportedSchemaVersion_ReturnsError)
{
	const char* wrongVersion = R"({
		"version": 999,
		"processing_units": []
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(wrongVersion, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSchemaVersionUnsupported);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, Validate_Version1_Success)
{
	const char* version1 = R"({
		"version": 1,
		"processing_units": []
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(version1, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
}

// ==============================================================================
// Circular Dependency Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, DISABLED_Validate_CircularDependency_ReturnsError)
{
	const char* circularDeps = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "ModA",
						"phase_ids": ["Phase1"],
						"dependencies": ["ModB"]
					},
					{
						"type_id": "ManifestTestModule",
						"instance_id": "ModB",
						"phase_ids": ["Phase1"],
						"dependencies": ["ModC"]
					},
					{
						"type_id": "ManifestTestModule",
						"instance_id": "ModC",
						"phase_ids": ["Phase1"],
						"dependencies": ["ModA"]
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(circularDeps, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kCircularDependency);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, DISABLED_Validate_SelfDependency_ReturnsError)
{
	const char* selfDep = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "ModA",
						"phase_ids": ["Phase1"],
						"dependencies": ["ModA"]
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(selfDep, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kCircularDependency);
}

// ==============================================================================
// Invalid Phase Transition Tests (AC11, AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, Validate_TransitionToNonExistentPhase_ReturnsError)
{
	const char* badTransition = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [
					{
						"from": "Phase1",
						"to": "NonExistentPhase"
					}
				],
				"initial_phase": "Phase1",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(badTransition, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kInvalidPhaseTransition);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, Validate_TransitionFromNonExistentPhase_ReturnsError)
{
	const char* badFromTransition = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [
					{
						"from": "NonExistentPhase",
						"to": "Phase1"
					}
				],
				"initial_phase": "Phase1",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(badFromTransition, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kInvalidPhaseTransition);
}

// ==============================================================================
// Duplicate Instance ID Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, Validate_DuplicatePhaseInstanceId_ReturnsError)
{
	const char* duplicatePhase = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "DuplicatePhase"
					},
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "DuplicatePhase"
					}
				],
				"transitions": [],
				"initial_phase": "DuplicatePhase",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(duplicatePhase, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kDuplicateInstanceId);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, Validate_DuplicateModuleInstanceId_ReturnsError)
{
	const char* duplicateModule = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "DuplicateModule",
						"phase_ids": ["Phase1"],
						"dependencies": []
					},
					{
						"type_id": "ManifestTestModule",
						"instance_id": "DuplicateModule",
						"phase_ids": ["Phase1"],
						"dependencies": []
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(duplicateModule, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kDuplicateInstanceId);
}

// ==============================================================================
// Module-Phase Reference Validation Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, Validate_ModuleReferencesNonExistentPhase_ReturnsError)
{
	const char* badPhaseRef = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "Mod1",
						"phase_ids": ["NonExistentPhase"],
						"dependencies": []
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(badPhaseRef, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kModuleMissingFromPhase);
	EXPECT_GT(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, Validate_ModuleReferencesValidPhases_Success)
{
	const char* goodPhaseRef = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "ManifestTestProcessingUnit",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase1"
					},
					{
						"type_id": "ManifestTestPhase",
						"instance_id": "Phase2"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": [
					{
						"type_id": "ManifestTestModule",
						"instance_id": "Mod1",
						"phase_ids": ["Phase1", "Phase2"],
						"dependencies": []
					}
				]
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	ManifestValidationResult result = loader.LoadFromString(goodPhaseRef, manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
}

// ==============================================================================
// Error Reporting Tests (AC17)
// ==============================================================================

TEST_F(ManifestLoaderTest, GetErrors_AfterValidationFailure_ReturnsErrors)
{
	const char* badManifest = R"({
		"version": 999,
		"processing_units": []
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	loader.LoadFromString(badManifest, manifest);

	const auto& errors = loader.GetErrors();

	EXPECT_GT(errors.Size(), 0u);
	EXPECT_EQ(errors[0].code, ManifestValidationResult::kSchemaVersionUnsupported);
}

TEST_F(ManifestLoaderTest, ClearErrors_RemovesAllErrors)
{
	const char* badManifest = R"({
		"version": 999,
		"processing_units": []
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	loader.LoadFromString(badManifest, manifest);
	EXPECT_GT(loader.GetErrors().Size(), 0u);

	loader.ClearErrors();
	EXPECT_EQ(loader.GetErrors().Size(), 0u);
}

TEST_F(ManifestLoaderTest, GetErrors_ContainsContextInformation)
{
	const char* badManifest = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "NonExistentType",
				"instance_id": "PU1",
				"hz": 60.0,
				"dedicated_thread": false,
				"phases": [],
				"transitions": [],
				"initial_phase": "",
				"modules": []
			}
		]
	})";

	ApplicationManifestLoader loader(*mRegistry);
	ApplicationManifest manifest;

	loader.LoadFromString(badManifest, manifest);

	const auto& errors = loader.GetErrors();

	ASSERT_GT(errors.Size(), 0u);
	// Error should have context path
	EXPECT_FALSE(errors[0].context.IsEmpty());
}
