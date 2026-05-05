#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// RelationshipTypes
		//
		// Built-in relationship type constants.
		// Relationship types are open — callers can define their own StringCRC values.
		// These ship with the catalogue as the standard vocabulary.
		//---------------------------------------------------------------------------------------------------------
		namespace RelationshipTypes
		{
			extern const Dia::Core::StringCRC kUses;          // "uses"
			extern const Dia::Core::StringCRC kContains;      // "contains"
			extern const Dia::Core::StringCRC kConfiguredBy;  // "configured_by"
			extern const Dia::Core::StringCRC kDependsOn;     // "depends_on"

		} // namespace RelationshipTypes

	} // namespace AssetCatalogue
} // namespace Dia
