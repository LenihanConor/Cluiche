#include "ApplicationFlow/Phases/MainBootPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainBootPhase::kTypeId("MainBootPhase");

	MainBootPhase::MainBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
		: MainPhaseBase(associatedProcessingUnit, instanceId)
	{}

	void MainBootPhase::AfterModulesStart()
	{
		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kTypeId);
	}

	void MainBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(Main::KernelModule::kTypeId));
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _MainBootPhase = Cluiche::MainBootPhase; }
DIA_REGISTER_PHASE(_MainBootPhase) {
	return new Cluiche::MainBootPhase(pu, instanceId);
}