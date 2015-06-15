#pragma once

#include <DiaApplication\ApplicationProcessingUnit.h>
#include <DiaCore\CRC\StringCRC.h>

#include "Source/ApplicationModel/Phases/RenderBootPhase.h"

////////////////////////////////////////////////////////////////////////////////
// Class name: RenderProcessingUnit, Primary PU for all rendering
////////////////////////////////////////////////////////////////////////////////
class RenderProcessingUnit: public Dia::Application::ProcessingUnit
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	RenderProcessingUnit();
	virtual ~RenderProcessingUnit();

	virtual void PrePhaseUpdate()override;
	virtual void PostPhaseUpdate()override;
private:
	// Phases
	RenderBootPhase mBootPhase;				// Initial Boot Phase
};

