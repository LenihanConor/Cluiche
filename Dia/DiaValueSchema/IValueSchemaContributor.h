#pragma once

// IValueSchemaContributor: the code-side seam for Feature 4 (metric / command-arg infos) and
// Feature 7 (entity component fields).
//
// Add a contributor with ValueSchemaModule::AddContributor BEFORE the module starts. In DoStart the
// module loads every sidecar file first, then calls each contributor in the order added, then
// publishes. Contributors register through ValueSchemaRegistry::Register (duplicates and collisions
// follow the registry's normal first-wins / reject rules). The registry reference is only valid for
// the duration of the call; registering after publish is rejected with an Error (AC 9).

namespace Dia::ValueSchema
{
	class ValueSchemaRegistry;

	class IValueSchemaContributor
	{
	public:
		virtual ~IValueSchemaContributor() = default;
		virtual void ContributeEntries(ValueSchemaRegistry& registry) = 0;
	};
}
