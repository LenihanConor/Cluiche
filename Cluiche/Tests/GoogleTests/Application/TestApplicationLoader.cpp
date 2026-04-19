#include <gtest/gtest.h>
#include <DiaApplication/Loader/ApplicationLoader.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include <fstream>
#include <cstdio>

using namespace Dia::Application;
using namespace Dia::Core;

class LoaderTestPU : public ProcessingUnit
{
public:
	static const StringCRC kTypeId;
	LoaderTestPU(const StringCRC& instanceId, float hz)
		: ProcessingUnit(instanceId, hz, 16, 16) {}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC LoaderTestPU::kTypeId("LoaderTestPU");

class LoaderTestPhase : public Phase
{
public:
	static const StringCRC kTypeId;
	LoaderTestPhase(ProcessingUnit* pu, const StringCRC& instanceId)
		: Phase(pu, instanceId) {}
	virtual bool FlaggedToStopUpdating() const override { return true; }
};
const StringCRC LoaderTestPhase::kTypeId("LoaderTestPhase");

class LoaderTestModule : public Module
{
public:
	static const StringCRC kTypeId;
	LoaderTestModule(ProcessingUnit* pu, const StringCRC& instanceId)
		: Module(pu, instanceId, RunningEnum::kUpdate) {}
	virtual StateObject::OpertionResponse DoStart(const IStartData*) override
	{
		return StateObject::OpertionResponse::kImmediate;
	}
	virtual void DoUpdate() override {}
	virtual void DoStop() override {}
};
const StringCRC LoaderTestModule::kTypeId("LoaderTestModule");

static ProcessingUnit* FallbackFactory()
{
	return new LoaderTestPU(StringCRC("FallbackPU"), -1.0f);
}

static ProcessingUnit* NullFallbackFactory()
{
	return nullptr;
}

class ApplicationLoaderTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		static bool registered = false;
		if (!registered)
		{
			RegisterTestTypes();
			registered = true;
		}
	}

	void RegisterTestTypes()
	{
		class TestPUFactory : public ITypeFactory<ProcessingUnit>
		{
		public:
			virtual ProcessingUnit* Create(const StringCRC& instanceId, const Json::Value& config) override
			{
				float hz = config.get("frequency_hz", -1.0f).asFloat();
				return new LoaderTestPU(instanceId, hz);
			}
		};

		class TestPhaseFactory : public ITypeFactory<Phase>
		{
		public:
			virtual Phase* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
			{
				return new LoaderTestPhase(pu, instanceId);
			}
		};

		class TestModuleFactory : public ITypeFactory<Module>
		{
		public:
			virtual Module* Create(ProcessingUnit* pu, const StringCRC& instanceId, const Json::Value& config) override
			{
				return new LoaderTestModule(pu, instanceId);
			}
		};

		static TestPUFactory puFactory;
		static TestPhaseFactory phaseFactory;
		static TestModuleFactory moduleFactory;

		ApplicationTypeRegistry::Instance().RegisterProcessingUnitType(LoaderTestPU::kTypeId, &puFactory);
		ApplicationTypeRegistry::Instance().RegisterPhaseType(LoaderTestPhase::kTypeId, &phaseFactory);
		ApplicationTypeRegistry::Instance().RegisterModuleType(LoaderTestModule::kTypeId, &moduleFactory);
	}

	const char* WriteManifestFile(const char* json)
	{
		static const char* path = "test_loader_manifest.diaapp";
		std::ofstream file(path, std::ios::out | std::ios::trunc);
		file << json;
		file.close();
		return path;
	}

	void CleanupManifestFile()
	{
		std::remove("test_loader_manifest.diaapp");
	}
};

TEST_F(ApplicationLoaderTest, LoadApplication_ValidFile_ReturnsProcessingUnit)
{
	const char* json = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "LoaderTestPU",
				"instance_id": "MainPU",
				"frequency_hz": 60.0,
				"dedicated_thread": false,
				"phases": [
					{
						"type_id": "LoaderTestPhase",
						"instance_id": "Phase1"
					}
				],
				"transitions": [],
				"initial_phase": "Phase1",
				"modules": []
			}
		]
	})";

	const char* path = WriteManifestFile(json);
	ManifestValidationResult result;
	ProcessingUnit* pu = ApplicationLoader::LoadApplication(path, result);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_NE(pu, nullptr);

	delete pu;
	CleanupManifestFile();
}

TEST_F(ApplicationLoaderTest, LoadApplication_NonexistentFile_ReturnsNull)
{
	ManifestValidationResult result;
	ProcessingUnit* pu = ApplicationLoader::LoadApplication("nonexistent_file.diaapp", result);

	EXPECT_EQ(pu, nullptr);
	EXPECT_NE(result, ManifestValidationResult::kSuccess);
}

TEST_F(ApplicationLoaderTest, LoadApplication_InvalidJson_ReturnsNull)
{
	const char* path = WriteManifestFile("{ not valid json }");
	ManifestValidationResult result;
	ProcessingUnit* pu = ApplicationLoader::LoadApplication(path, result);

	EXPECT_EQ(pu, nullptr);
	EXPECT_EQ(result, ManifestValidationResult::kInvalidJSON);

	CleanupManifestFile();
}

TEST_F(ApplicationLoaderTest, LoadApplication_ConvenienceOverload_ReturnsNull)
{
	ProcessingUnit* pu = ApplicationLoader::LoadApplication("nonexistent_file.diaapp");
	EXPECT_EQ(pu, nullptr);
}

TEST_F(ApplicationLoaderTest, LoadApplicationWithFallback_ManifestFails_UsesFallback)
{
	ProcessingUnit* pu = ApplicationLoader::LoadApplicationWithFallback(
		"nonexistent_file.diaapp", FallbackFactory);

	EXPECT_NE(pu, nullptr);
	delete pu;
}

TEST_F(ApplicationLoaderTest, LoadApplicationWithFallback_ManifestSucceeds_SkipsFallback)
{
	const char* json = R"({
		"version": 1,
		"processing_units": [
			{
				"type_id": "LoaderTestPU",
				"instance_id": "MainPU",
				"frequency_hz": -1.0,
				"dedicated_thread": false,
				"phases": [],
				"transitions": [],
				"initial_phase": "",
				"modules": []
			}
		]
	})";

	const char* path = WriteManifestFile(json);
	ProcessingUnit* pu = ApplicationLoader::LoadApplicationWithFallback(path, FallbackFactory);

	EXPECT_NE(pu, nullptr);

	delete pu;
	CleanupManifestFile();
}

TEST_F(ApplicationLoaderTest, LoadApplicationWithFallback_NullFactory_ReturnsNull)
{
	ProcessingUnit* pu = ApplicationLoader::LoadApplicationWithFallback(
		"nonexistent_file.diaapp", NullFallbackFactory);

	EXPECT_EQ(pu, nullptr);
}

TEST_F(ApplicationLoaderTest, LoadApplication_EmptyProcessingUnits_ReturnsNull)
{
	const char* json = R"({
		"version": 1,
		"processing_units": []
	})";

	const char* path = WriteManifestFile(json);
	ManifestValidationResult result;
	ProcessingUnit* pu = ApplicationLoader::LoadApplication(path, result);

	EXPECT_EQ(pu, nullptr);

	CleanupManifestFile();
}
