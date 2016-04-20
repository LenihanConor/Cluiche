#include "ApplicationFlow/Phases/SimBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC SimBootStrapPhase::kUniqueId("SimBootStrapPhase");

	SimBootStrapPhase::SimBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
	{}

	void SimBootStrapPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		//AddModule(buildDependencies->GetModule(MainKernelModule::kUniqueId));
	}
}