#include "Source/ApplicationModel/ProcessingUnits/SimulationProcessingUnit.h"


const Dia::Core::StringCRC SimulationProcessingUnit::kUniqueId("SimulationProcessingUnit");

SimulationProcessingUnit::SimulationProcessingUnit()
	: Dia::Application::ProcessingUnit(kUniqueId)
	, mBootPhase(this)
{
	AddPhase(&mBootPhase);

	SetInitialPhase(&mBootPhase);

	Initialize();
}

SimulationProcessingUnit::~SimulationProcessingUnit()
{}

void SimulationProcessingUnit::PrePhaseUpdate()
{}

void SimulationProcessingUnit::PostPhaseUpdate()
{}


