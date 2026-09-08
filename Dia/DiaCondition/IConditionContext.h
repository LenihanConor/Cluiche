#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Condition
	{
		//-------------------------------------------------------------------------------------------
		// IConditionContext
		//
		// Pure virtual interface for resolving named slot/field pairs to typed values at condition
		// evaluation time.  Concrete implementations (e.g. ConditionRegistry) register accessor
		// callbacks; the evaluator holds only an IConditionContext* and calls through this interface.
		//
		// SD-003: Not a singleton — explicitly constructed and passed by the owning module.
		// SD-009: GetFloat/GetBool return value directly. Missing slot/field returns 0.0f/false
		//         and fires DIA_ASSERT in debug builds.
		// PD-001: All slot/field keys are StringCRC — no raw strings at evaluation time.
		//-------------------------------------------------------------------------------------------
		class IConditionContext
		{
		public:
			virtual ~IConditionContext() = default;

			virtual float GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const = 0;
			virtual bool  GetBool (Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const = 0;
		};

	} // namespace Condition
} // namespace Dia
