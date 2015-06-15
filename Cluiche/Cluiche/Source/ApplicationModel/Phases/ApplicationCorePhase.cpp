#include "Source/ApplicationModel/Phases/ApplicationCorePhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

const Dia::Core::StringCRC ApplicationCorePhase::kUniqueId("ApplicationCorePhase");


ApplicationCorePhase::ApplicationCorePhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
	: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
{}

ApplicationCorePhase::~ApplicationCorePhase()
{}

void ApplicationCorePhase::PostStart()
{
	mInputPU.Start();
	mSimulationPU.Start();
	mRenderPU.Start();
}

