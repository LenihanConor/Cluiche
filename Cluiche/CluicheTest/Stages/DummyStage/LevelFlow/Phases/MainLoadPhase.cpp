#include "LevelFlow/Phases/MainLoadPhase.h"
#include "LevelFlow/Phases/MainFEPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaDebugServer/DebugServerModule.h>

namespace Cluiche
{
	namespace DummyStage
	{
		const Dia::Core::StringCRC MainLoadPhase::kTypeId("DummyProject::MainLoadPhase");

		MainLoadPhase::MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Cluiche::Main::MainPhaseBase(associatedProcessingUnit, kTypeId)
		{}

		void MainLoadPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(Cluiche::Main::KernelModule::kTypeId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::LevelFactoryModule::kTypeId));
			AddModule(buildDependencies->GetModule(Cluiche::Main::UIModule::kTypeId));
			AddModule(buildDependencies->GetModule(Dia::DebugServer::DebugServerModule::kTypeId));
		}

		void MainLoadPhase::AfterModulesStart()
		{
			GetAssociatedProcessingUnit()->QueuePhaseTransition(MainFEPhase::kTypeId);
		}
	}
}