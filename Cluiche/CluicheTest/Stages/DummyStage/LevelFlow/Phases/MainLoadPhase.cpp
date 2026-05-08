#include "LevelFlow/Phases/MainLoadPhase.h"
#include "LevelFlow/Phases/MainFEPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaDebugServer/DebugServerModule.h>
#include <DiaLogger/DiaLog.h>

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
		}

		void MainLoadPhase::AfterModulesStart()
		{
			Cluiche::Main::AssetServiceModule* assetService = this->GetModule<Cluiche::Main::AssetServiceModule>();
			assetService->RequestStageLoad(Dia::Core::StringCRC("stage.dummy_stage"));

			GetAssociatedProcessingUnit()->QueuePhaseTransition(MainFEPhase::kTypeId);
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _DummyStageMainLoadPhase = Cluiche::DummyStage::MainLoadPhase; }
DIA_REGISTER_PHASE(_DummyStageMainLoadPhase) {
	return new Cluiche::DummyStage::MainLoadPhase(pu);
}