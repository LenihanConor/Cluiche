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
// Class name: InputBootPhase, Initial phase for input
////////////////////////////////////////////////////////////////////////////////
class InputBootPhase: public Dia::Application::Phase
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	InputBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
	virtual ~InputBootPhase();
};

