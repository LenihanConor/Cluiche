#include "LevelFlow/Phases/MainLoadPhase.h"
#include "LevelFlow/Phases/MainFEPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace UnitTestLevel
	{
		const Dia::Core::StringCRC MainLoadPhase::kUniqueId("UnitTestLevel::MainLoadPhase");

		MainLoadPhase::MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Main::MainPhaseBase(associatedProcessingUnit, kUniqueId)
		{}

		void MainLoadPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Cluiche::Main::KernelModule::kUniqueId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::LevelFactoryModule::kUniqueId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::UIModule::kUniqueId));
		}

		void MainLoadPhase::AfterModulesStart()
		{
			GetAssociatedProcessingUnit()->QueuePhaseTransition(MainFEPhase::kUniqueId);
		}
	}
}