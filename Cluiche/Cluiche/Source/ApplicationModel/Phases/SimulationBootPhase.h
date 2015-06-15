#pragma once

#include <DiaApplication/ApplicationPhase.h>
#include <DiaCore\CRC\StringCRC.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Class name: SimulationBootPhase, Initial phase for the sim PU
////////////////////////////////////////////////////////////////////////////////
class SimulationBootPhase: public Dia::Application::Phase
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	SimulationBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	virtual ~SimulationBootPhase();
};

