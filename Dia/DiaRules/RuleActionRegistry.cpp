#include "RuleActionRegistry.h"

#include <cstdint>
#include <unordered_map>

namespace Dia
{
	namespace Rules
	{
		// -----------------------------------------------------------------------------------------
		// Impl — pimpl struct holding the std::unordered_map table.
		// Kept in the .cpp so no STL leaks into the public header (PD-004).
		//
		// Key: the raw 32-bit CRC value from StringCRC::Value().
		// Value: RuleActionFn — may be nullptr (caller may clear a slot, SD-004).
		// -----------------------------------------------------------------------------------------
		struct RuleActionRegistry::Impl
		{
			// pair<bool registered, RuleActionFn handler> — use a presence flag to distinguish
			// "never registered" from "registered with nullptr".
			struct Entry
			{
				RuleActionFn handler{ nullptr };
			};

			std::unordered_map<unsigned int, Entry> table;
		};

		// -----------------------------------------------------------------------------------------
		// RuleActionRegistry
		// -----------------------------------------------------------------------------------------

		RuleActionRegistry::RuleActionRegistry()
			: mImpl(new Impl())
		{
		}

		RuleActionRegistry::~RuleActionRegistry()
		{
			delete mImpl;
		}

		void RuleActionRegistry::Register(Dia::Core::StringCRC actionId, RuleActionFn handler)
		{
			mImpl->table[actionId.Value()].handler = handler;
		}

		RuleActionFn RuleActionRegistry::Find(Dia::Core::StringCRC actionId) const
		{
			const auto it = mImpl->table.find(actionId.Value());
			if (it == mImpl->table.end())
			{
				return nullptr;
			}
			return it->second.handler;
		}

		bool RuleActionRegistry::Has(Dia::Core::StringCRC actionId) const
		{
			return mImpl->table.count(actionId.Value()) != 0;
		}

	} // namespace Rules
} // namespace Dia
