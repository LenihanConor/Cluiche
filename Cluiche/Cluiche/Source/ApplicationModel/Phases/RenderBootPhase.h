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
// Class name: ApplicationBootPhase, Initial phase for rendetr
////////////////////////////////////////////////////////////////////////////////
class RenderBootPhase: public Dia::Application::Phase
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	RenderBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	virtual ~RenderBootPhase();
};

