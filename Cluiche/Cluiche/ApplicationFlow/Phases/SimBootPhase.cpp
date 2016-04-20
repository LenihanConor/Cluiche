#include "ApplicationFlow/Phases/SimBootPhase.h"

#include "ApplicationFlow/Phases/SimBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC SimBootPhase::kUniqueId("SimBootPhase");

	SimBootPhase::SimBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
	{}

	void SimBootPhase::AfterModulesStart()
	{
		GetAssociatedProcessingUnit()->QueuePhaseTransition(SimBootStrapPhase::kUniqueId);
	}

	void SimBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		//AddModule(buildDependencies->GetModule(MainKernelModule::kUniqueId));
	}
}