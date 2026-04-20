#include "ApplicationFlow/Phases/SimBootPhase.h"

#include "ApplicationFlow/Phases/SimBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC SimBootPhase::kTypeId("SimBootPhase");

	SimBootPhase::SimBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
		: Dia::Application::Phase(associatedProcessingUnit, instanceId)
	{}

	void SimBootPhase::AfterModulesStart()
	{
		GetAssociatedProcessingUnit()->QueuePhaseTransition(SimBootStrapPhase::kTypeId);
	}

	void SimBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		//AddModule(buildDependencies->GetModule(KernelModule::kTypeId));
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _SimBootPhase = Cluiche::SimBootPhase; }
DIA_REGISTER_PHASE(_SimBootPhase) {
	return new Cluiche::SimBootPhase(pu, instanceId);
}