#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Rules
	{
		using RuleActionFn = void(*)(void* actionContext);

		//-------------------------------------------------------------------------------------------
		// RuleActionRegistry
		//
		// Open handler table mapping StringCRC action names to callbacks.
		//
		// SD-003: Explicitly constructed — not a singleton. Caller creates, owns, passes by ref.
		// SD-004: Action callbacks use type-erased void* actionContext.
		// PD-001: All action IDs use StringCRC.
		// PD-004: No STL in the public API — internal implementation uses std::unordered_map.
		// AD-003: All code in Dia::Rules:: namespace.
		//-------------------------------------------------------------------------------------------
		class RuleActionRegistry
		{
		public:
			RuleActionRegistry();
			~RuleActionRegistry();

			void Register(Dia::Core::StringCRC actionId, RuleActionFn handler);

			RuleActionFn Find(Dia::Core::StringCRC actionId) const;
			bool         Has(Dia::Core::StringCRC actionId) const;

		private:
			// Forward-declared to keep STL out of the public header (PD-004).
			struct Impl;
			Impl* mImpl;
		};

	} // namespace Rules
} // namespace Dia
