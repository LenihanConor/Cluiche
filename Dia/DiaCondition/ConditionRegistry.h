#pragma once

#include <DiaCondition/IConditionContext.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Condition
	{
		using FloatAccessorFn = float(*)(void* data);
		using BoolAccessorFn  = bool (*)(void* data);

		//-------------------------------------------------------------------------------------------
		// ConditionRegistry
		//
		// Concrete IConditionContext that stores named float/bool accessor callbacks keyed by
		// slot+field StringCRC pairs.  The owning module constructs a ConditionRegistry, registers
		// accessors pointing at its own data, then passes an IConditionContext* to evaluators.
		//
		// SD-003: Explicitly constructed with a void* data pointer — not a singleton.
		// SD-009: GetFloat/GetBool assert (debug) and return 0.0f/false on missing key.
		// PD-004: No STL in the public API — internal implementation uses std::unordered_map.
		// PD-001: Slot/field keys are StringCRC only.
		//-------------------------------------------------------------------------------------------
		class ConditionRegistry : public IConditionContext
		{
		public:
			explicit ConditionRegistry(void* data);
			~ConditionRegistry();

			void RegisterFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, FloatAccessorFn accessor);
			void RegisterBool (Dia::Core::StringCRC slot, Dia::Core::StringCRC field, BoolAccessorFn  accessor);

			bool HasFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const;
			bool HasBool (Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const;

			float GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override;
			bool  GetBool (Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override;

		private:
			void*  mData;

			// Forward-declared to keep STL out of the public header.
			struct Impl;
			Impl*  mImpl;
		};

	} // namespace Condition
} // namespace Dia
