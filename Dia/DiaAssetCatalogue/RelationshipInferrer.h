#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/FieldAttributes.h"
#include "DiaCore/Reflect/JsonArchive.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetTypeRegistry;

		//---------------------------------------------------------------------------------------------------------
		// RelationshipInferrer
		//
		// Walks an object's fields via DiaReflect, looking for fields with AssetRefAttribute markers,
		// and returns inferred "uses" edges based on the field values.
		//
		// Two overloads:
		//   - AssetRecord overload: placeholder, returns empty (no deserialized data available)
		//   - Typed template overload: usable version where caller provides a typed loaded object
		//---------------------------------------------------------------------------------------------------------
		class RelationshipInferrer
		{
		public:
			// Placeholder overload — returns empty (no deserialized instance available from AssetRecord alone).
			void InferRelationships(const AssetRecord& record,
				const AssetTypeRegistry& typeRegistry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const;

			// Walk fields of the typed object, find AssetRefAttribute-annotated char array fields,
			// and emit "uses" edges for non-empty values.
			//
			// typeCrc must be StringCRC(TypeName).Value() — the same key used when DIA_ATTR_ASSET_REF
			// was called for this type. E.g. for SpriteAsset: StringCRC("SpriteAsset").Value().
			template<typename T>
			void InferRelationships(
				const T& value,
				uint32_t typeCrc,
				const AssetTypeRegistry& typeRegistry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia

#include "DiaAssetCatalogue/RelationshipInferrer.inl"
