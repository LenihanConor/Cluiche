////////////////////////////////////////////////////////////////////////////////
// Filename: DummyProject
////////////////////////////////////////////////////////////////////////////////
#include "DummyLevel/DummyLevel.h"

#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC Level::kLevelUniqueId("dummy_level"); //("dummy_project"; // This is registered with the level manager and is used by UI/flow to determine what to boot.

		Level::Level(Dia::Application::Phase* currentPhase, 
						Dia::Application::ProcessingUnit* mainPU, 
						Dia::Application::ProcessingUnit* simPU,
						Dia::Application::ProcessingUnit* renderPU)
			: mMainLoadPhase(mainPU)
		{
			mEntryPhaseUniqueId = Cluiche::DummyLevel::MainLoadPhase::kUniqueId;
			mainPU->AddPhaseTransiton(currentPhase, &mMainLoadPhase);

			mainPU->Initialize();
		}
	}
}