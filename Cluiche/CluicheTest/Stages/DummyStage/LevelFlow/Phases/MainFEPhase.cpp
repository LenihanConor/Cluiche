#include "LevelFlow/Phases/MainFEPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaDebugServer/DebugServerModule.h>

namespace Cluiche
{
	namespace DummyStage
	{
		const Dia::Core::StringCRC MainFEPhase::kTypeId("DummyProject::MainFEPhase");

		MainFEPhase::MainFEPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Main::MainPhaseBase(associatedProcessingUnit, kTypeId)
			, mUi(this)
		{}

		void MainFEPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Cluiche::Main::KernelModule::kTypeId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::LevelFactoryModule::kTypeId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::UIModule::kTypeId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::AssetServiceModule::kTypeId));
			AddModule(buildDependencies->GetModule(Dia::DebugServer::DebugServerModule::kTypeId));
		}

		void MainFEPhase::AfterModulesStart()
		{
			mUi.InitializePage();

			Cluiche::Main::UIModule* ui = this->GetModule<Cluiche::Main::UIModule>();
			ui->GetUISystem()->LoadPage(mUi);
		}

		void MainFEPhase::BeforeModulesStop()
		{
			Cluiche::Main::AssetServiceModule* assetService = this->GetModule<Cluiche::Main::AssetServiceModule>();
			assetService->RequestStageUnload(Dia::Core::StringCRC("stage.dummy_stage"));

			Cluiche::Main::UIModule* ui = this->GetModule<Cluiche::Main::UIModule>();
			ui->GetUISystem()->UnloadPage();
		}


		void MainFEPhase::RequestExitLevel()
		{
			const Cluiche::Main::LevelFactoryModule* levelRegistryModule = this->GetModule<Cluiche::Main::LevelFactoryModule>();

			if (levelRegistryModule == nullptr)
			{
				DIA_ASSERT(nullptr, "Could not find level registry");
			}

			const Kernel::LevelFactory& levelRegistry = levelRegistryModule->GetLevelFactory();

			const Cluiche::Kernel::ILevel* level = levelRegistry.GetCurrentLevel();

			if (level == nullptr)
			{
				DIA_ASSERT(nullptr, "Could not find current level");
			}

			const Dia::Core::StringCRC& exitPhaseName = level->GetExitPhaseUniqueId();
			GetAssociatedProcessingUnit()->QueuePhaseTransition(exitPhaseName);
		}
	}
}