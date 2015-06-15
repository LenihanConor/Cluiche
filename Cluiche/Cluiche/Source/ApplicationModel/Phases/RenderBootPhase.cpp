#include "Source/ApplicationModel/Phases/RenderBootPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

const Dia::Core::StringCRC RenderBootPhase::kUniqueId("RenderBootPhase");

RenderBootPhase::RenderBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
	: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
{}

RenderBootPhase::~RenderBootPhase()
{}
