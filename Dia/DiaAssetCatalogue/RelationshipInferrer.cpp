#include "DiaAssetCatalogue/RelationshipInferrer.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"
#include "DiaAssetCatalogue/AssetTypeDescriptor.h"
#include "DiaAssetCatalogue/RelationshipTypes.h"

#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// InferRelationships (AssetRecord overload — placeholder, returns empty)
		//------------------------------------------------------------------------------------
		void RelationshipInferrer::InferRelationships(const AssetRecord& record,
			const AssetTypeRegistry& typeRegistry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const
		{
			// Cannot infer relationships without deserialized instance data.
			// Caller should use the typed template overload instead.
			(void)record;
			(void)typeRegistry;
			(void)outEdges;
		}

	} // namespace AssetCatalogue
} // namespace Dia
