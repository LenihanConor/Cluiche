#ifndef DIA_APPLICATION_DEBUG_DATA_TYPES_H
#define DIA_APPLICATION_DEBUG_DATA_TYPES_H

#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Application
	{
		namespace DebugDataType
		{
			static const Dia::Core::StringCRC kProcessingUnitState("processing_unit_state");
			static const Dia::Core::StringCRC kPhaseTransition("phase_transition");
			static const Dia::Core::StringCRC kModuleState("module_state");
			static const Dia::Core::StringCRC kMessageBus("message_bus");
			static const Dia::Core::StringCRC kPerformanceBreakdown("performance_breakdown");
		}
	}
}

#endif // DIA_APPLICATION_DEBUG_DATA_TYPES_H
