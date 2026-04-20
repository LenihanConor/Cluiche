#include "ApplicationFlow/Phases/SimBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h"

namespace Cluiche
{
	const Dia::Core::StringCRC SimBootStrapPhase::kTypeId("SimBootStrapPhase");

	SimBootStrapPhase::SimBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
		: Dia::Application::Phase(associatedProcessingUnit, instanceId)
	{}

	void SimBootStrapPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(Sim::UIProxyModule::kTypeId));
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _SimBootStrapPhase = Cluiche::SimBootStrapPhase; }
DIA_REGISTER_PHASE(_SimBootStrapPhase) {
	return new Cluiche::SimBootStrapPhase(pu, instanceId);
}