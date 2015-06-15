#include "Source/ApplicationModel/Phases/SimulationBootPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

const Dia::Core::StringCRC SimulationBootPhase::kUniqueId("SimulationBootPhase");

SimulationBootPhase::SimulationBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
	: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
{}

SimulationBootPhase::~SimulationBootPhase()
{}
