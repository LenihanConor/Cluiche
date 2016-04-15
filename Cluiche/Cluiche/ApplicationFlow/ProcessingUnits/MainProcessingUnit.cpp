#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

namespace Cluiche
{
	const Dia::Core::StringCRC MainProcessingUnit::kUniqueId("MainProcessingUnit");

	MainProcessingUnit::MainProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId)
		, mBootPhase(this)
		, mBootStrapPhase(this)
		, mKernelModule(this)
	{
		// Add Phases
		AddPhase(&mBootPhase);
		AddPhase(&mBootStrapPhase);

		// Setup Phase Transitions
		SetInitialPhase(&mBootPhase);
		AddPhaseTransiton(&mBootPhase, &mBootStrapPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this cas case this is a nicer cleaner solution.
		Initialize();
	}

	bool MainProcessingUnit::FlaggedToStopUpdating()const
	{
		const Cluiche::MainPhaseBase* mainPhase = static_cast<const Cluiche::MainPhaseBase*>(GetCurrentPhase());
		return mainPhase->ShouldQuitApplication();
	}
}