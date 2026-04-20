#include "ApplicationFlow/Phases/MainBootPhase.h"

#include "ApplicationFlow/Modules/MainKernelModule.h"
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainBootPhase::kTypeId("MainBootPhase");

	MainBootPhase::MainBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: MainPhaseBase(associatedProcessingUnit, kTypeId)
	{}

	void MainBootPhase::AfterModulesStart()
	{
		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kTypeId);
	}

	void MainBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(MainKernelModule::kTypeId));
	}
}