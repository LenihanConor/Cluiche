////////////////////////////////////////////////////////////////////////////////
// Filename: DummyProject
////////////////////////////////////////////////////////////////////////////////
#include "DummyStage/DummyStage.h"

#include "CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h"

#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyStage
	{
		const Dia::Core::StringCRC Level::kLevelUniqueId("dummy_stage"); // This is registered with the level manager and is used by UI/flow to determine what to boot.

		Level::Level(Dia::Application::Phase* currentPhase,
						Dia::Application::ProcessingUnit* mainPU,
						Dia::Application::ProcessingUnit* simPU,
						Dia::Application::ProcessingUnit* renderPU)
			: mMainLoadPhase(mainPU)
			, mMainFEPhase(mainPU)
			, mSimRunningPhase(simPU)
			, mSimPU(simPU)
		{
			mEntryPhaseUniqueId = Cluiche::DummyStage::MainLoadPhase::kTypeId;
			mExitPhaseUniqueId = currentPhase->GetUniqueId();

			mainPU->AddPhaseTransiton(currentPhase, &mMainLoadPhase);
			mainPU->AddPhaseTransiton(&mMainLoadPhase, &mMainFEPhase);
			mainPU->AddPhaseTransiton(&mMainFEPhase, currentPhase);

			if (simPU)
			{
				simPU->AddPhaseTransiton(simPU->GetCurrentPhase(), &mSimRunningPhase);
				simPU->Initialize();
			}

			mainPU->Initialize();
		}

		Level::~Level()
		{
			// NEED TO DEINITITALIZE!!!
		}
	}
}