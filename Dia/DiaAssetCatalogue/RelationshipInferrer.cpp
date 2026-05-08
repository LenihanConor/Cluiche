#include "DiaAssetCatalogue/RelationshipInferrer.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"
#include "DiaAssetCatalogue/AssetTypeDescriptor.h"
#include "DiaAssetCatalogue/RelationshipTypes.h"

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeVariableAttributes.h"
#include "DiaCore/Type/TypeInstance.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/LinkList/LinkListC.h"

#include <string.h>

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
			// Caller should use the TypeInstance overload instead.
			(void)record;
			(void)typeRegistry;
			(void)outEdges;
		}

		//------------------------------------------------------------------------------------
		// InferRelationships (TypeInstance overload — usable version)
		//------------------------------------------------------------------------------------
		void RelationshipInferrer::InferRelationships(const Dia::Core::Types::TypeInstance& instance,
			const Dia::Core::StringCRC& assetTypeId,
			const AssetTypeRegistry& typeRegistry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const
		{
			const AssetTypeDescriptor* descriptor = typeRegistry.FindByTypeId(assetTypeId);
			if (descriptor == nullptr || descriptor->mTypeDefinition == nullptr)
			{
				return;
			}

			const Dia::Core::Types::TypeDefinition* typeDef = descriptor->mTypeDefinition;
			const Dia::Core::Types::TypeDefinition::VariableLinkList& variables = typeDef->GetVariables();

			// Walk all TypeVariables looking for AssetReference attributes
			const Dia::Core::Types::TypeDefinition::VariableLinkListNode* node = variables.HeadConst();
			while (node != nullptr)
			{
				Dia::Core::Types::TypeVariable* const& variableRef = node->GetPayloadConst();
				const Dia::Core::Types::TypeVariable* variable = variableRef;
				if (variable != nullptr)
				{
					if (variable->HasAttribute<Dia::Core::Types::TypeVariableAttributeAssetReference>())
					{
						const Dia::Core::Types::TypeVariableAttributeAssetReference* attr =
							variable->GetAttributeConst<Dia::Core::Types::TypeVariableAttributeAssetReference>();

						if (attr != nullptr)
						{
							// Read the field value as a char array from the instance's memory.
							// The field offset from the parent gives us the location in the object.
							const char* instanceBase = static_cast<const char*>(instance.Pointee());
							unsigned int offset = variable->GetOffsetFromParent(0);
							const char* fieldData = instanceBase + (offset / sizeof(char));

							if (fieldData != nullptr && fieldData[0] != '\0')
							{
								Dia::Core::StringCRC targetId(fieldData);

								// Add a "uses" edge
								RelationshipEdge edge(RelationshipTypes::kUses, targetId);
								if (!outEdges.IsFull())
								{
									outEdges.Add(edge);
								}
							}
						}
					}
				}
				node = node->GetNextConst();
			}
		}

	} // namespace AssetCatalogue
} // namespace Dia
