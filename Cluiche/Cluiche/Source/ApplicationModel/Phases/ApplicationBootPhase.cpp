#include "Source/ApplicationModel/Phases/ApplicationBootPhase.h"

#include "Source/ApplicationModel/Phases/ApplicationCorePhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

const Dia::Core::StringCRC ApplicationBootPhase::kUniqueId("ApplicationBootPhase");


ApplicationBootPhase::ApplicationBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
{}


ApplicationBootPhase::~ApplicationBootPhase()
{}

void ApplicationBootPhase::PostUpdate()
{
	QueuePhaseTransition(ApplicationCorePhase::kUniqueId);
}