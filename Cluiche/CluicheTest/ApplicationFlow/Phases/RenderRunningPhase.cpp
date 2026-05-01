#include "ApplicationFlow/Phases/RenderRunningPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC RenderRunningPhase::kTypeId("RenderRunningPhase");

	RenderRunningPhase::RenderRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
		: Dia::Application::Phase(associatedProcessingUnit, instanceId)
	{}

	void RenderRunningPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		//AddModule(buildDependencies->GetModule(KernelModule::kTypeId));
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _RenderRunningPhase = Cluiche::RenderRunningPhase; }
DIA_REGISTER_PHASE(_RenderRunningPhase) {
	return new Cluiche::RenderRunningPhase(pu, instanceId);
}