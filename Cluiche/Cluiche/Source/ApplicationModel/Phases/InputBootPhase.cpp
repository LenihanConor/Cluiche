#include "Source/ApplicationModel/Phases/InputBootPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

const Dia::Core::StringCRC InputBootPhase::kUniqueId("InputBootPhase");

InputBootPhase::InputBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
	: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
{}

InputBootPhase::~InputBootPhase()
{}
