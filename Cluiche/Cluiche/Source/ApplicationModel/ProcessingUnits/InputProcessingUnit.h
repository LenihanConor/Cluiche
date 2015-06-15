#pragma once

#include <DiaApplication\ApplicationProcessingUnit.h>
#include <DiaCore\CRC\StringCRC.h>

#include "Source/ApplicationModel/Phases/InputBootPhase.h"

////////////////////////////////////////////////////////////////////////////////
// Class name: ApplicationProcessingUnit, Primary PU for this application
////////////////////////////////////////////////////////////////////////////////
class InputProcessingUnit: public Dia::Application::ProcessingUnit
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	InputProcessingUnit();
	virtual ~InputProcessingUnit();

	virtual void PrePhaseUpdate()override;
	virtual void PostPhaseUpdate()override;

private:
	// Phases
	InputBootPhase mBootPhase;				// Initial Boot Phase
};

