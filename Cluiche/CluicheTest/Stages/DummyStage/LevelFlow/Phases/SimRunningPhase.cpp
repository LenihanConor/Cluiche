#include "LevelFlow/Phases/SimRunningPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

#include "CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h"

namespace Cluiche
{
	namespace DummyStage
	{
		const Dia::Core::StringCRC SimRunningPhase::kTypeId("DummyStage::SimRunningPhase");

		SimRunningPhase::SimRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kTypeId)
		{}

		void SimRunningPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Sim::TimeServerModule::kTypeId));
			AddModule(buildDependencies->GetModule(Sim::InputFrameStreamModule::kTypeId));
			AddModule(buildDependencies->GetModule(Sim::UIProxyModule::kTypeId));
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _DummyStageSimRunningPhase = Cluiche::DummyStage::SimRunningPhase; }
DIA_REGISTER_PHASE(_DummyStageSimRunningPhase) {
	return new Cluiche::DummyStage::SimRunningPhase(pu);
}
