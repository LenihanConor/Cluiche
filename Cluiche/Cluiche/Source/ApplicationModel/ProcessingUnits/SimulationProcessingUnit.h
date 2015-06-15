#pragma once

#include <DiaApplication\ApplicationProcessingUnit.h>
#include <DiaCore\CRC\StringCRC.h>

#include "Source/ApplicationModel/Phases/SimulationBootPhase.h"

////////////////////////////////////////////////////////////////////////////////
// Class name: SimulationProcessingUnit, Primary PU for all simulation 
////////////////////////////////////////////////////////////////////////////////
class SimulationProcessingUnit: public Dia::Application::ProcessingUnit
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	SimulationProcessingUnit();
	virtual ~SimulationProcessingUnit();

	virtual void PrePhaseUpdate()override;
	virtual void PostPhaseUpdate()override;

private:
	// Phases
	SimulationBootPhase mBootPhase;				// Initial Boot Phase
	//ApplicationBootStrapPhase mBootStrapPhase;		// Debug menu option to choose next phase
};

