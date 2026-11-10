#pragma once

// Built-in 12-colour categorical list used by the loader to assign colours to enum values that do
// not specify one. Feature 5's `categorical.12` ramp replaces this list.

namespace Dia::ValueSchema
{
	constexpr unsigned int kCategoricalColourCount = 12;
	// Returns "#RRGGBB" for index % kCategoricalColourCount.
	const char* GetCategoricalColour(unsigned int index);
}
