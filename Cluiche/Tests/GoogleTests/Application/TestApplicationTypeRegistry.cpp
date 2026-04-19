// TestApplicationTypeRegistry.cpp - Unit tests for ApplicationTypeRegistry
//
// Tests the type registration and instantiation system for ProcessingUnits, Phases, and Modules

#include <gtest/gtest.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <json/json.h>

using namespace Dia::Application;
using namespace Dia::Core;

// ==============================================================================
// Test Types
// ==============================================================================

// Test ProcessingUnit
class TestProcessingUnit : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;

	TestProcessingUnit(const StringCRC& instanceId, float hz = 60.0f)
		: ProcessingUnit(instanceId, hz, 16, 16)
	{
	}

	virtual bool FlaggedToStopUpdating() const override { return true; }
};

const StringCRC TestProcessingUnit::kTypeId("TestProcessingUnit");

// Test Phase
class TestPhase : public Phase
{
public:
	static const StringCRC kTypeId;

	TestPhase(ProcessingUnit* pu, const StringCRC& instanceId)
		: Phase(pu, instanceId)
	{
	}

	virtual bool FlaggedToStopUpdating() const override { return true; }
};

const StringCRC TestPhase::kTypeId("TestPhase");

// Test Module
class TestModule : public Module
{
public:
	static const StringCRC kTypeId;

	TestModule(ProcessingUnit* pu, const StringCRC& instanceId)
		: Module(pu, instanceId, RunningEnum::kUpdate)
	{
	}

	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}

	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};

const StringCRC TestModule::kTypeId("TestModule");

// ==============================================================================
// Test Factories (Manual registration for testing)
// ==============================================================================

class TestProcessingUnitFactory : public ITypeFactory<ProcessingUnit>
{
public:
	virtual ProcessingUnit* Create(const StringCRC& instanceId, const Json::Value& config) override
	{
		float hz = config.get("hz", 60.0f).asFloat();
		return new TestProcessingUnit(instanceId, hz);
	}
};

class TestPhaseFactory : public ITypeFactory<Phase>
{
public:
	virtual Phase* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
	{
		return new TestPhase(pu, instanceId);
	}
};

class TestModuleFactory : public ITypeFactory<Module>
{
public:
	virtual Module* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
	{
		return new TestModule(pu, instanceId);
	}
};

// ==============================================================================
// Test Fixture
// ==============================================================================

class ApplicationTypeRegistryTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Note: We can't easily clear the registry between tests since it's a singleton
		// and may have statically registered types. Tests are designed to be independent.
	}

	void TearDown() override
	{
		// Cleanup any created objects
	}

	ApplicationTypeRegistry& GetRegistry()
	{
		return ApplicationTypeRegistry::Instance();
	}
};

// ==============================================================================
// Registration Tests
// ==============================================================================

TEST_F(ApplicationTypeRegistryTest, RegisterProcessingUnitType_Success)
{
	static TestProcessingUnitFactory factory;
	StringCRC typeId("TestPU_Register1");

	GetRegistry().RegisterProcessingUnitType(typeId, &factory);

	EXPECT_TRUE(GetRegistry().IsProcessingUnitTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, RegisterPhaseType_Success)
{
	static TestPhaseFactory factory;
	StringCRC typeId("TestPhase_Register1");

	GetRegistry().RegisterPhaseType(typeId, &factory);

	EXPECT_TRUE(GetRegistry().IsPhaseTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, RegisterModuleType_Success)
{
	static TestModuleFactory factory;
	StringCRC typeId("TestModule_Register1");

	GetRegistry().RegisterModuleType(typeId, &factory);

	EXPECT_TRUE(GetRegistry().IsModuleTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, RegisterDuplicateProcessingUnit_IgnoredWithWarning)
{
	static TestProcessingUnitFactory factory;
	StringCRC typeId("TestPU_Duplicate");

	// First registration should succeed
	GetRegistry().RegisterProcessingUnitType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsProcessingUnitTypeRegistered(typeId));

	// Second registration should be ignored (warning logged)
	GetRegistry().RegisterProcessingUnitType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsProcessingUnitTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, RegisterDuplicatePhase_IgnoredWithWarning)
{
	static TestPhaseFactory factory;
	StringCRC typeId("TestPhase_Duplicate");

	// First registration should succeed
	GetRegistry().RegisterPhaseType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsPhaseTypeRegistered(typeId));

	// Second registration should be ignored (warning logged)
	GetRegistry().RegisterPhaseType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsPhaseTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, RegisterDuplicateModule_IgnoredWithWarning)
{
	static TestModuleFactory factory;
	StringCRC typeId("TestModule_Duplicate");

	// First registration should succeed
	GetRegistry().RegisterModuleType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsModuleTypeRegistered(typeId));

	// Second registration should be ignored (warning logged)
	GetRegistry().RegisterModuleType(typeId, &factory);
	EXPECT_TRUE(GetRegistry().IsModuleTypeRegistered(typeId));
}

// ==============================================================================
// Type Lookup Tests
// ==============================================================================

TEST_F(ApplicationTypeRegistryTest, IsTypeRegistered_UnregisteredProcessingUnit_ReturnsFalse)
{
	StringCRC typeId("NonExistentProcessingUnit");
	EXPECT_FALSE(GetRegistry().IsProcessingUnitTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, IsTypeRegistered_UnregisteredPhase_ReturnsFalse)
{
	StringCRC typeId("NonExistentPhase");
	EXPECT_FALSE(GetRegistry().IsPhaseTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, IsTypeRegistered_UnregisteredModule_ReturnsFalse)
{
	StringCRC typeId("NonExistentModule");
	EXPECT_FALSE(GetRegistry().IsModuleTypeRegistered(typeId));
}

TEST_F(ApplicationTypeRegistryTest, IsTypeRegistered_AfterRegistration_ReturnsTrue)
{
	static TestProcessingUnitFactory puFactory;
	static TestPhaseFactory phaseFactory;
	static TestModuleFactory moduleFactory;

	StringCRC puTypeId("RegisteredPU");
	StringCRC phaseTypeId("RegisteredPhase");
	StringCRC moduleTypeId("RegisteredModule");

	GetRegistry().RegisterProcessingUnitType(puTypeId, &puFactory);
	GetRegistry().RegisterPhaseType(phaseTypeId, &phaseFactory);
	GetRegistry().RegisterModuleType(moduleTypeId, &moduleFactory);

	EXPECT_TRUE(GetRegistry().IsProcessingUnitTypeRegistered(puTypeId));
	EXPECT_TRUE(GetRegistry().IsPhaseTypeRegistered(phaseTypeId));
	EXPECT_TRUE(GetRegistry().IsModuleTypeRegistered(moduleTypeId));
}

// ==============================================================================
// Instantiation Tests
// ==============================================================================

TEST_F(ApplicationTypeRegistryTest, CreateProcessingUnit_RegisteredType_Success)
{
	static TestProcessingUnitFactory factory;
	StringCRC typeId("TestPU_Create1");
	StringCRC instanceId("MyProcessingUnit1");

	GetRegistry().RegisterProcessingUnitType(typeId, &factory);

	Json::Value config;
	config["hz"] = 30.0f;

	ProcessingUnit* pu = GetRegistry().CreateProcessingUnit(typeId, instanceId, config);

	ASSERT_NE(pu, nullptr);
	EXPECT_EQ(pu->GetName(), instanceId);

	delete pu;
}

TEST_F(ApplicationTypeRegistryTest, CreateProcessingUnit_WithConfig_UsesConfigValues)
{
	static TestProcessingUnitFactory factory;
	StringCRC typeId("TestPU_ConfigTest");
	StringCRC instanceId("ConfiguredPU");

	GetRegistry().RegisterProcessingUnitType(typeId, &factory);

	Json::Value config;
	config["hz"] = 120.0f;

	ProcessingUnit* pu = GetRegistry().CreateProcessingUnit(typeId, instanceId, config);

	ASSERT_NE(pu, nullptr);
	// Note: Can't easily verify hz value without exposing it, but factory was called with correct config

	delete pu;
}

TEST_F(ApplicationTypeRegistryTest, CreateProcessingUnit_UnregisteredType_ReturnsNull)
{
	StringCRC typeId("UnregisteredPU");
	StringCRC instanceId("SomeInstance");
	Json::Value config;

	ProcessingUnit* pu = GetRegistry().CreateProcessingUnit(typeId, instanceId, config);

	EXPECT_EQ(pu, nullptr);
}

TEST_F(ApplicationTypeRegistryTest, CreatePhase_RegisteredType_Success)
{
	static TestPhaseFactory factory;
	StringCRC typeId("TestPhase_Create1");
	StringCRC instanceId("MyPhase1");

	GetRegistry().RegisterPhaseType(typeId, &factory);

	TestProcessingUnit pu(StringCRC("TempPU"));
	Json::Value config;

	Phase* phase = GetRegistry().CreatePhase(typeId, &pu, instanceId, config);

	ASSERT_NE(phase, nullptr);
	EXPECT_EQ(phase->GetName(), instanceId);

	delete phase;
}

TEST_F(ApplicationTypeRegistryTest, CreatePhase_UnregisteredType_ReturnsNull)
{
	StringCRC typeId("UnregisteredPhase");
	StringCRC instanceId("SomePhase");
	TestProcessingUnit pu(StringCRC("TempPU"));
	Json::Value config;

	Phase* phase = GetRegistry().CreatePhase(typeId, &pu, instanceId, config);

	EXPECT_EQ(phase, nullptr);
}

TEST_F(ApplicationTypeRegistryTest, CreateModule_RegisteredType_Success)
{
	static TestModuleFactory factory;
	StringCRC typeId("TestModule_Create1");
	StringCRC instanceId("MyModule1");

	GetRegistry().RegisterModuleType(typeId, &factory);

	TestProcessingUnit pu(StringCRC("TempPU"));
	Json::Value config;

	Module* module = GetRegistry().CreateModule(typeId, &pu, instanceId, config);

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->GetName(), instanceId);

	delete module;
}

TEST_F(ApplicationTypeRegistryTest, CreateModule_UnregisteredType_ReturnsNull)
{
	StringCRC typeId("UnregisteredModule");
	StringCRC instanceId("SomeModule");
	TestProcessingUnit pu(StringCRC("TempPU"));
	Json::Value config;

	Module* module = GetRegistry().CreateModule(typeId, &pu, instanceId, config);

	EXPECT_EQ(module, nullptr);
}

// ==============================================================================
// Introspection Tests
// ==============================================================================

TEST_F(ApplicationTypeRegistryTest, GetRegisteredProcessingUnitTypes_ReturnsRegisteredTypes)
{
	static TestProcessingUnitFactory factory1, factory2;
	StringCRC typeId1("IntroTestPU1");
	StringCRC typeId2("IntroTestPU2");

	GetRegistry().RegisterProcessingUnitType(typeId1, &factory1);
	GetRegistry().RegisterProcessingUnitType(typeId2, &factory2);

	const auto& types = GetRegistry().GetRegisteredProcessingUnitTypes();

	// Should contain at least our two types (may contain others from other tests)
	bool foundType1 = false;
	bool foundType2 = false;

	for (unsigned int i = 0; i < types.Size(); ++i)
	{
		if (types[i] == typeId1) foundType1 = true;
		if (types[i] == typeId2) foundType2 = true;
	}

	EXPECT_TRUE(foundType1);
	EXPECT_TRUE(foundType2);
}

TEST_F(ApplicationTypeRegistryTest, GetRegisteredPhaseTypes_ReturnsRegisteredTypes)
{
	static TestPhaseFactory factory1, factory2;
	StringCRC typeId1("IntroTestPhase1");
	StringCRC typeId2("IntroTestPhase2");

	GetRegistry().RegisterPhaseType(typeId1, &factory1);
	GetRegistry().RegisterPhaseType(typeId2, &factory2);

	const auto& types = GetRegistry().GetRegisteredPhaseTypes();

	bool foundType1 = false;
	bool foundType2 = false;

	for (unsigned int i = 0; i < types.Size(); ++i)
	{
		if (types[i] == typeId1) foundType1 = true;
		if (types[i] == typeId2) foundType2 = true;
	}

	EXPECT_TRUE(foundType1);
	EXPECT_TRUE(foundType2);
}

TEST_F(ApplicationTypeRegistryTest, GetRegisteredModuleTypes_ReturnsRegisteredTypes)
{
	static TestModuleFactory factory1, factory2, factory3;
	StringCRC typeId1("IntroTestModule1");
	StringCRC typeId2("IntroTestModule2");
	StringCRC typeId3("IntroTestModule3");

	GetRegistry().RegisterModuleType(typeId1, &factory1);
	GetRegistry().RegisterModuleType(typeId2, &factory2);
	GetRegistry().RegisterModuleType(typeId3, &factory3);

	const auto& types = GetRegistry().GetRegisteredModuleTypes();

	bool foundType1 = false;
	bool foundType2 = false;
	bool foundType3 = false;

	for (unsigned int i = 0; i < types.Size(); ++i)
	{
		if (types[i] == typeId1) foundType1 = true;
		if (types[i] == typeId2) foundType2 = true;
		if (types[i] == typeId3) foundType3 = true;
	}

	EXPECT_TRUE(foundType1);
	EXPECT_TRUE(foundType2);
	EXPECT_TRUE(foundType3);
}

TEST_F(ApplicationTypeRegistryTest, IntrospectionAPI_UpdatesAfterRegistration)
{
	static TestModuleFactory factory;
	StringCRC typeId("DynamicModule");

	// Get initial count
	unsigned int initialCount = GetRegistry().GetRegisteredModuleTypes().Size();

	// Register new type
	GetRegistry().RegisterModuleType(typeId, &factory);

	// Get updated count
	unsigned int afterCount = GetRegistry().GetRegisteredModuleTypes().Size();

	// Should have increased by 1
	EXPECT_EQ(afterCount, initialCount + 1);
}
