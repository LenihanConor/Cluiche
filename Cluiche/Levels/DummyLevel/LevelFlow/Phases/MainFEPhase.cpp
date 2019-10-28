#include "LevelFlow/Phases/MainFEPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC MainFEPhase::kUniqueId("DummyProject::MainFEPhase");

		MainFEPhase::MainFEPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Main::MainPhaseBase(associatedProcessingUnit, kUniqueId)
			, mUi(this)
		{}

		void MainFEPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Cluiche::Main::KernelModule::kUniqueId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::LevelFactoryModule::kUniqueId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::UIModule::kUniqueId));
		}

		void MainFEPhase::AfterModulesStart()
		{
			mUi.InitializePage();

			Cluiche::Main::UIModule* ui = this->GetModule<Cluiche::Main::UIModule>();
			ui->GetUISystem()->LoadPage(mUi);
		}

		void MainFEPhase::BeforeModulesStop()
		{
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