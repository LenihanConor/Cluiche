#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeInstance;
		}
	}

	namespace AssetCatalogue
	{
		class AssetTypeRegistry;

		//---------------------------------------------------------------------------------------------------------
		// RelationshipInferrer
		//
		// Walks a TypeDefinition's fields looking for TypeVariableAttributeAssetReference markers
		// and returns inferred "uses" edges based on the field values.
		//
		// Two overloads:
		//   - AssetRecord overload: placeholder, returns empty (no deserialized data available)
		//   - TypeInstance overload: usable version where caller provides loaded data
		//---------------------------------------------------------------------------------------------------------
		class RelationshipInferrer
		{
		public:
			// Walk the record's TypeDefinition, find TypeVariableAttributeAssetReference fields.
			// This overload returns empty (no deserialized instance available from AssetRecord alone).
			void InferRelationships(const AssetRecord& record,
				const AssetTypeRegistry& typeRegistry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const;

			// Walk the TypeDefinition of the given type, read StringCRC values from the instance,
			// and return inferred "uses" edges.
			void InferRelationships(const Dia::Core::Types::TypeInstance& instance,
				const Dia::Core::StringCRC& assetTypeId,
				const AssetTypeRegistry& typeRegistry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia
