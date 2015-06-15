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
// Class name: ApplicationBootPhase, Initial phase after booting
////////////////////////////////////////////////////////////////////////////////
class ApplicationBootPhase: public Dia::Application::Phase
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	ApplicationBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	virtual ~ApplicationBootPhase();

	virtual void PostUpdate()override;
};

