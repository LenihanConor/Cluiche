#pragma once

// ComponentFieldCollector: turns registered ECS component fields (DiaEntity ComponentRegistry, described via
// DescribeComponentFields) into ValueInfo entries in a ValueSchemaRegistry (pre-Publish only).
//
// Mapping, one entry per field:
//   for  = component type id (e.g. "foo-component")      key = field name      kind = ComponentField
//   label/description/unit/hint from the describe attribute (label falls back to the member name)
//   min/max from the range attribute; default from the constructed default
//   type from the default's JSON type (bool/int/float/string); without a default: Primitive -> float,
//   StringId -> string. Nested fields are one item: type string with ui.nested = true.
// Undescribed = ranged field with no describe attribute. Fields with neither are plain data (not reported).
// Entries whose text exceeds ValueInfo capacities are rejected (Error log, counted), never truncated.

#include <DiaValueSchema/CodeSideCollector.h>

namespace Dia::Entity { struct ComponentTypeDesc; }

namespace Dia::ValueSchema
{
	class ValueSchemaRegistry;

	struct ComponentFieldReport
	{
		unsigned int	mCollected = 0;			// component_field entries registered
		unsigned int	mDescribed = 0;			// of those, with a describe attribute
		unsigned int	mUndescribed = 0;		// of those, ranged with no describe attribute
		unsigned int	mRejectedCount = 0;		// capacity / registry rejections (not duplicates)
		unsigned int	mDuplicateCount = 0;	// DuplicateKey results (first wins)

		ValueList<UndescribedItem>	mUndescribedFields;	// name = "<type id>/<field>", owner = type id
	};

	class ComponentFieldCollector
	{
	public:
		static constexpr unsigned int kMaxFieldsPerComponent = 256;

		// Injectable seam: collects the given descriptors.
		ComponentFieldReport Collect(const Dia::Entity::ComponentTypeDesc* const* descs, unsigned int count,
			ValueSchemaRegistry& registry);
		// Collects every component in Dia::Entity::ComponentRegistry::Get().
		ComponentFieldReport Collect(ValueSchemaRegistry& registry);
	};
}
