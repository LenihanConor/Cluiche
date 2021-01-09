////////////////////////////////////////////////////////////////////////////////
// Filename: 
////////////////////////////////////////////////////////////////////////////////
#include "UnitTestLevel/UnitTestLevel.h"

#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace UnitTestLevel
	{
		const Dia::Core::StringCRC Level::kLevelUniqueId("unit_test_level"); // This is registered with the level manager and is used by UI/flow to determine what to boot.

		Level::Level(Dia::Application::Phase* currentPhase, 
						Dia::Application::ProcessingUnit* mainPU, 
						Dia::Application::ProcessingUnit* simPU,
						Dia::Application::ProcessingUnit* renderPU)
			: mMainFEPhase(mainPU)
			, mMainLoadPhase(mainPU)
		{
			mEntryPhaseUniqueId = Cluiche::UnitTestLevel::MainLoadPhase::kUniqueId;
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