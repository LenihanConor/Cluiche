////////////////////////////////////////////////////////////////////////////////
// Filename: DummyProject
////////////////////////////////////////////////////////////////////////////////
#include "DummyStage/DummyStage.h"

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
			: mMainFEPhase(mainPU)
			, mMainLoadPhase(mainPU)
		{
			mEntryPhaseUniqueId = Cluiche::DummyStage::MainLoadPhase::kTypeId;
			mExitPhaseUniqueId = currentPhase->GetUniqueId();

			mainPU->AddPhaseTransiton(currentPhase, &mMainLoadPhase);
			mainPU->AddPhaseTransiton(&mMainLoadPhase, &mMainFEPhase);
			mainPU->AddPhaseTransiton(&mMainFEPhase, currentPhase);

			mainPU->Initialize();
		}

		Level::~Level()
		{
			// NEED TO DEINITITALIZE!!!
		}
	}
}