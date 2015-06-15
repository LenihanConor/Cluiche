#include "Source/ApplicationModel/ProcessingUnits/ApplicationProcessingUnit.h"

#include "Source/ApplicationModel/Phases/ApplicationBootPhase.h"
#include "Source/ApplicationModel/Phases/ApplicationCorePhase.h"

const Dia::Core::StringCRC ApplicationProcessingUnit::kUniqueId("ApplicationProcessingUnit");

ApplicationProcessingUnit::ApplicationProcessingUnit()
	: Dia::Application::ProcessingUnit(kUniqueId)
	, mBootPhase(this)
	, mCorePhase(this)
	, mApplicationTime(this)
	, mQuitApplication(false)
{

	// Add Phases/Modules
	AddPhase(&mBootPhase);
	AddPhase(&mCorePhase);

	mBootPhase.AddModule(&mApplicationTime);
	mCorePhase.AddModule(&mApplicationTime);

	AddModule(&mApplicationTime);

	// Setup Phase Transitions
	SetInitialPhase(&mBootPhase);
	AddPhaseTransiton(&mBootPhase, &mCorePhase);

	// We call this from here to build all dependancies. We dont need to do this here, intead 
	// we could of done it in the code below if there was dynamic adding of modules or phases
	// but for this cas case this is a nicer cleaner solution.
	Initialize();
}


ApplicationProcessingUnit::~ApplicationProcessingUnit()
{}

void ApplicationProcessingUnit::PrePhaseUpdate()
{}

void ApplicationProcessingUnit::PostPhaseUpdate()
{}


