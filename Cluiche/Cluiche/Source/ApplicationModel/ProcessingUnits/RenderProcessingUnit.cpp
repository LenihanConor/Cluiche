#include "Source/ApplicationModel/ProcessingUnits/RenderProcessingUnit.h"

const Dia::Core::StringCRC RenderProcessingUnit::kUniqueId("RenderProcessingUnit");

RenderProcessingUnit::RenderProcessingUnit()
	: Dia::Application::ProcessingUnit(kUniqueId)
	, mBootPhase(this)
{
	AddPhase(&mBootPhase);

	SetInitialPhase(&mBootPhase);

	Initialize();
}

RenderProcessingUnit::~RenderProcessingUnit()
{}

void RenderProcessingUnit::PrePhaseUpdate()
{}

void RenderProcessingUnit::PostPhaseUpdate()
{}


