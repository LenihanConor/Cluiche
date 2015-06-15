#include "Source/ApplicationModel/ProcessingUnits/InputProcessingUnit.h"

const Dia::Core::StringCRC InputProcessingUnit::kUniqueId("InputProcessingUnit");

InputProcessingUnit::InputProcessingUnit()
	: Dia::Application::ProcessingUnit(kUniqueId)
	, mBootPhase(this)
{
	AddPhase(&mBootPhase);

	SetInitialPhase(&mBootPhase);

	Initialize();
}

InputProcessingUnit::~InputProcessingUnit()
{}

void InputProcessingUnit::PrePhaseUpdate()
{}

void InputProcessingUnit::PostPhaseUpdate()
{}


