#pragma once

#include <DiaApplication/ApplicationPhase.h>
#include <DiaCore\CRC\StringCRC.h>


#include "Source/ApplicationModel/ProcessingUnits/InputProcessingUnit.h"
#include "Source/ApplicationModel/ProcessingUnits/RenderProcessingUnit.h"
#include "Source/ApplicationModel/ProcessingUnits/SimulationProcessingUnit.h"

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Class name: ApplicationCorePhase, Loading of all core functionality
////////////////////////////////////////////////////////////////////////////////
class ApplicationCorePhase : public Dia::Application::Phase
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	ApplicationCorePhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	virtual ~ApplicationCorePhase();

private:

	virtual void PostStart()override;

	InputProcessingUnit mInputPU;
	RenderProcessingUnit mRenderPU;
	SimulationProcessingUnit mSimulationPU;
};

