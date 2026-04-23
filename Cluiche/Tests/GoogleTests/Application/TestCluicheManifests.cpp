#include <gtest/gtest.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <json/json.h>

#include <fstream>
#include <sstream>

using namespace Dia::Application;
using namespace Dia::Core;

class CluicheManifestTest : public ::testing::Test
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

	ApplicationTypeRegistry* mRegistry = nullptr;

	bool LoadManifest(const char* path, ApplicationManifest& outManifest)
	{
		std::ifstream file(path, std::ios::in | std::ios::binary);
		if (!file.is_open()) return false;
		std::stringstream buffer;
		buffer << file.rdbuf();

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(buffer.str(), root, false)) return false;

		outManifest.version = root["version"].asUInt();

		const Json::Value& pus = root["processing_units"];
		for (unsigned int i = 0; i < pus.size(); ++i)
		{
			ApplicationManifest::ProcessingUnitEntry puEntry;
			puEntry.typeId = StringCRC(pus[i]["type_id"].asCString());
			puEntry.instanceId = StringCRC(pus[i]["instance_id"].asCString());
			puEntry.frequencyHz = pus[i].get("frequency_hz", -1.0f).asFloat();
			puEntry.dedicatedThread = pus[i].get("dedicated_thread", false).asBool();
			if (pus[i].isMember("config"))
				puEntry.config = new Json::Value(pus[i]["config"]);

			const Json::Value& phases = pus[i]["phases"];
			for (unsigned int j = 0; j < phases.size(); ++j)
			{
				ApplicationManifest::PhaseEntry pe;
				pe.typeId = StringCRC(phases[j]["type_id"].asCString());
				pe.instanceId = StringCRC(phases[j]["instance_id"].asCString());
				if (phases[j].isMember("config"))
					pe.config = new Json::Value(phases[j]["config"]);
				puEntry.phases.Add(pe);
			}

			const Json::Value& transitions = pus[i]["transitions"];
			for (unsigned int j = 0; j < transitions.size(); ++j)
			{
				ApplicationManifest::PhaseTransition pt;
				pt.fromPhase = StringCRC(transitions[j]["from"].asCString());
				pt.toPhase = StringCRC(transitions[j]["to"].asCString());
				puEntry.transitions.Add(pt);
			}

			puEntry.initialPhase = StringCRC(pus[i]["initial_phase"].asCString());

			const Json::Value& modules = pus[i]["modules"];
			for (unsigned int j = 0; j < modules.size(); ++j)
			{
				ApplicationManifest::ModuleEntry me;
				me.typeId = StringCRC(modules[j]["type_id"].asCString());
				me.instanceId = StringCRC(modules[j]["instance_id"].asCString());

				const Json::Value& phaseIds = modules[j]["phase_ids"];
				for (unsigned int k = 0; k < phaseIds.size(); ++k)
					me.phaseIds.Add(StringCRC(phaseIds[k].asCString()));

				if (modules[j].isMember("dependencies"))
				{
					const Json::Value& deps = modules[j]["dependencies"];
					for (unsigned int k = 0; k < deps.size(); ++k)
						me.dependencies.Add(StringCRC(deps[k].asCString()));
				}

				if (modules[j].isMember("config"))
					me.config = new Json::Value(modules[j]["config"]);

				puEntry.modules.Add(me);
			}

			outManifest.processingUnits.Add(puEntry);
		}
		return true;
	}

	bool FileExists(const char* path)
	{
		std::ifstream file(path);
		return file.good();
	}
};

// ============================================================================
// Main Manifest
// ============================================================================

TEST_F(CluicheManifestTest, MainManifest_FileExists)
{
	ASSERT_TRUE(FileExists("Data/Manifests/cluiche_main.diaapp"));
}

TEST_F(CluicheManifestTest, MainManifest_ParsesSuccessfully)
{
	ApplicationManifest manifest;
	ASSERT_TRUE(LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest));
}

TEST_F(CluicheManifestTest, MainManifest_HasOneProcessingUnit)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	ASSERT_EQ(manifest.processingUnits.Size(), 1u);
}

TEST_F(CluicheManifestTest, MainManifest_PUTypeAndInstance)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& pu = manifest.processingUnits[0];

	EXPECT_EQ(pu.typeId, StringCRC("MainProcessingUnit"));
	EXPECT_EQ(pu.instanceId, StringCRC("MainProcessingUnit"));
}

TEST_F(CluicheManifestTest, MainManifest_HasTwoPhases)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 2u);
}

TEST_F(CluicheManifestTest, MainManifest_PhaseTypes)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& phases = manifest.processingUnits[0].phases;

	EXPECT_EQ(phases[0].typeId, StringCRC("MainBootPhase"));
	EXPECT_EQ(phases[1].typeId, StringCRC("MainBootStrapPhase"));
}

TEST_F(CluicheManifestTest, MainManifest_InitialPhase)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits[0].initialPhase, StringCRC("MainBootPhase"));
}

TEST_F(CluicheManifestTest, MainManifest_HasOneTransition)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& transitions = manifest.processingUnits[0].transitions;

	ASSERT_EQ(transitions.Size(), 1u);
	EXPECT_EQ(transitions[0].fromPhase, StringCRC("MainBootPhase"));
	EXPECT_EQ(transitions[0].toPhase, StringCRC("MainBootStrapPhase"));
}

TEST_F(CluicheManifestTest, MainManifest_HasFiveModules)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 6u);
}

TEST_F(CluicheManifestTest, MainManifest_ModuleTypes)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& modules = manifest.processingUnits[0].modules;

	EXPECT_EQ(modules[0].typeId, StringCRC("LoggerModule"));
	EXPECT_EQ(modules[1].typeId, StringCRC("Main::KernelModule"));
	EXPECT_EQ(modules[2].typeId, StringCRC("Main::LevelRegistryModule"));
	EXPECT_EQ(modules[3].typeId, StringCRC("Main::UIModule"));
	EXPECT_EQ(modules[4].typeId, StringCRC("MetricsCollectorModule"));
	EXPECT_EQ(modules[5].typeId, StringCRC("DebugServerModule"));
}

TEST_F(CluicheManifestTest, MainManifest_KernelModuleInBothPhases)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& kernelModule = manifest.processingUnits[0].modules[1];

	ASSERT_EQ(kernelModule.phaseIds.Size(), 2u);
	EXPECT_EQ(kernelModule.phaseIds[0], StringCRC("MainBootPhase"));
	EXPECT_EQ(kernelModule.phaseIds[1], StringCRC("MainBootStrapPhase"));
}

TEST_F(CluicheManifestTest, MainManifest_UIModuleDependsOnKernel)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", manifest);
	const auto& uiModule = manifest.processingUnits[0].modules[3];

	ASSERT_EQ(uiModule.dependencies.Size(), 1u);
	EXPECT_EQ(uiModule.dependencies[0], StringCRC("Main::KernelModule"));
}

// ============================================================================
// Render Manifest
// ============================================================================

TEST_F(CluicheManifestTest, RenderManifest_FileExists)
{
	ASSERT_TRUE(FileExists("Data/Manifests/cluiche_render.diaapp"));
}

TEST_F(CluicheManifestTest, RenderManifest_ParsesSuccessfully)
{
	ApplicationManifest manifest;
	ASSERT_TRUE(LoadManifest("Data/Manifests/cluiche_render.diaapp", manifest));
}

TEST_F(CluicheManifestTest, RenderManifest_PUTypeAndFrequency)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_render.diaapp", manifest);
	const auto& pu = manifest.processingUnits[0];

	EXPECT_EQ(pu.typeId, StringCRC("RenderProcessingUnit"));
	EXPECT_EQ(pu.instanceId, StringCRC("RenderProcessingUnit"));
	EXPECT_FLOAT_EQ(pu.frequencyHz, 60.0f);
}

TEST_F(CluicheManifestTest, RenderManifest_HasOnePhaseNoModules)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_render.diaapp", manifest);
	const auto& pu = manifest.processingUnits[0];

	EXPECT_EQ(pu.phases.Size(), 1u);
	EXPECT_EQ(pu.modules.Size(), 0u);
	EXPECT_EQ(pu.transitions.Size(), 0u);
}

TEST_F(CluicheManifestTest, RenderManifest_PhaseType)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_render.diaapp", manifest);

	EXPECT_EQ(manifest.processingUnits[0].phases[0].typeId, StringCRC("RenderRunningPhase"));
	EXPECT_EQ(manifest.processingUnits[0].initialPhase, StringCRC("RenderRunningPhase"));
}

// ============================================================================
// Sim Manifest
// ============================================================================

TEST_F(CluicheManifestTest, SimManifest_FileExists)
{
	ASSERT_TRUE(FileExists("Data/Manifests/cluiche_sim.diaapp"));
}

TEST_F(CluicheManifestTest, SimManifest_ParsesSuccessfully)
{
	ApplicationManifest manifest;
	ASSERT_TRUE(LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest));
}

TEST_F(CluicheManifestTest, SimManifest_PUTypeAndFrequency)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& pu = manifest.processingUnits[0];

	EXPECT_EQ(pu.typeId, StringCRC("SimProcessingUnit"));
	EXPECT_EQ(pu.instanceId, StringCRC("SimProcessingUnit"));
	EXPECT_FLOAT_EQ(pu.frequencyHz, 30.0f);
}

TEST_F(CluicheManifestTest, SimManifest_HasTwoPhases)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& pu = manifest.processingUnits[0];

	ASSERT_EQ(pu.phases.Size(), 2u);
	EXPECT_EQ(pu.phases[0].typeId, StringCRC("SimBootPhase"));
	EXPECT_EQ(pu.phases[1].typeId, StringCRC("SimBootStrapPhase"));
}

TEST_F(CluicheManifestTest, SimManifest_HasOneTransition)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& transitions = manifest.processingUnits[0].transitions;

	ASSERT_EQ(transitions.Size(), 1u);
	EXPECT_EQ(transitions[0].fromPhase, StringCRC("SimBootPhase"));
	EXPECT_EQ(transitions[0].toPhase, StringCRC("SimBootStrapPhase"));
}

TEST_F(CluicheManifestTest, SimManifest_HasThreeModules)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits[0].modules.Size(), 3u);
}

TEST_F(CluicheManifestTest, SimManifest_ModuleTypes)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& modules = manifest.processingUnits[0].modules;

	EXPECT_EQ(modules[0].typeId, StringCRC("Sim::TimeServerModule"));
	EXPECT_EQ(modules[1].typeId, StringCRC("Sim::UIProxyModule"));
	EXPECT_EQ(modules[2].typeId, StringCRC("Sim::InputFrameStreamModule"));
}

TEST_F(CluicheManifestTest, SimManifest_TimeServerModuleConfig)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& timeModule = manifest.processingUnits[0].modules[0];

	ASSERT_NE(timeModule.config, nullptr);
	EXPECT_FLOAT_EQ(timeModule.config->get("hz", 0.0f).asFloat(), 30.0f);
}

TEST_F(CluicheManifestTest, SimManifest_UIProxyModuleDependencies)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	const auto& uiProxy = manifest.processingUnits[0].modules[1];

	ASSERT_EQ(uiProxy.dependencies.Size(), 2u);
	EXPECT_EQ(uiProxy.dependencies[0], StringCRC("Sim::TimeServerModule"));
	EXPECT_EQ(uiProxy.dependencies[1], StringCRC("Sim::InputFrameStreamModule"));
}

TEST_F(CluicheManifestTest, SimManifest_InitialPhase)
{
	ApplicationManifest manifest;
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", manifest);
	EXPECT_EQ(manifest.processingUnits[0].initialPhase, StringCRC("SimBootPhase"));
}

// ============================================================================
// Cross-Manifest Consistency
// ============================================================================

TEST_F(CluicheManifestTest, AllManifests_VersionIsOne)
{
	ApplicationManifest mainManifest, renderManifest, simManifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", mainManifest);
	LoadManifest("Data/Manifests/cluiche_render.diaapp", renderManifest);
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", simManifest);

	EXPECT_EQ(mainManifest.version, 1u);
	EXPECT_EQ(renderManifest.version, 1u);
	EXPECT_EQ(simManifest.version, 1u);
}

TEST_F(CluicheManifestTest, AllManifests_InstanceIdsMatchTypeIds)
{
	ApplicationManifest mainManifest, renderManifest, simManifest;
	LoadManifest("Data/Manifests/cluiche_main.diaapp", mainManifest);
	LoadManifest("Data/Manifests/cluiche_render.diaapp", renderManifest);
	LoadManifest("Data/Manifests/cluiche_sim.diaapp", simManifest);

	EXPECT_EQ(mainManifest.processingUnits[0].typeId, mainManifest.processingUnits[0].instanceId);
	EXPECT_EQ(renderManifest.processingUnits[0].typeId, renderManifest.processingUnits[0].instanceId);
	EXPECT_EQ(simManifest.processingUnits[0].typeId, simManifest.processingUnits[0].instanceId);
}
