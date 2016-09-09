#include "ApplicationFlow/Phases/MainBootPhase.h"

#include "CluicheKernel/MainKernelModule.h"
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainBootPhase::kUniqueId("MainBootPhase");

	MainBootPhase::MainBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: MainPhaseBase(associatedProcessingUnit, kUniqueId)
	{}

	void MainBootPhase::AfterModulesStart()
	{
		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kUniqueId);
	}

	void MainBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(Kernel::MainKernelModule::kUniqueId));
	}
}