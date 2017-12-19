#include "LevelFlow/Phases/MainLoadPhase.h"

//#include "ApplicationFlow/Modules/MainKernelModule.h"
//#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC MainLoadPhase::kUniqueId("DummyProject::MainLoadPhase");

		MainLoadPhase::MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Kernel::MainPhaseBase(associatedProcessingUnit, kUniqueId)
		{}

		void MainLoadPhase::AfterModulesStart()
		{
	//		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kUniqueId);
		}

		void MainLoadPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
	//		AddModule(buildDependencies->GetModule(MainKernelModule::kUniqueId));
		}
	}
}