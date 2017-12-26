#include "LevelFlow/Phases/MainLoadPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC MainLoadPhase::kUniqueId("DummyProject::MainLoadPhase");

		MainLoadPhase::MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Kernel::MainPhaseBase(associatedProcessingUnit, kUniqueId)
		{}

		void MainLoadPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Cluiche::Kernel::MainKernelModule::kUniqueId));
			AddModule(buildDependencies->GetModule(Cluiche::MainUIModule::kUniqueId));
		}

		void MainLoadPhase::AfterModulesStart()
		{
	//		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kUniqueId);
		}
	}
}