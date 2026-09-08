#pragma once

#include "DiaAssetCatalogue/RelationshipTypes.h"
#include "DiaAssetCatalogue/AssetTypeRegistry.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/FieldAttributes.h"

#include <type_traits>
#include <cstdint>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// AssetRefScanArchive
		//
		// A read-like archive that scans fields annotated with AssetRefAttribute and
		// emits "uses" relationship edges for non-empty char array values.
		//
		// For each NamedField<T>:
		//   - Checks FieldAttributeRegistry for AssetRefAttribute on (typeCrc, fieldCrc).
		//   - If found AND field is a char[N], reads the value as a C-string and adds
		//     a RelationshipEdge.
		//   - All other field types are silently skipped.
		//
		// typeCrc must equal StringCRC(TypeName).Value() — the same key used with DIA_ATTR_ASSET_REF.
		//------------------------------------------------------------------------------------
		class AssetRefScanArchive
		{
		public:
			bool IsReading() const { return true; }
			bool IsWriting() const { return false; }

			AssetRefScanArchive(uint32_t typeCrc,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges)
				: mTypeCrc(typeCrc)
				, mOutEdges(outEdges)
			{}

			template<typename T>
			AssetRefScanArchive& operator&(Dia::Reflect::NamedField<T> field)
			{
				// Check if this (type, field) pair has an AssetRefAttribute.
				// We use GetAttributes + GetKind() string comparison to avoid dynamic_cast,
				// since DiaAssetCatalogue may compile with RTTI disabled.
				static constexpr unsigned int kMaxAttrs = 8u;
				const Dia::Reflect::FieldAttribute* attrs[kMaxAttrs] = {};
				unsigned int count = Dia::Reflect::FieldAttributeRegistry::Instance().GetAttributes(
					mTypeCrc, field.name.Value(), attrs, kMaxAttrs);

				bool hasAssetRef = false;
				for (unsigned int i = 0u; i < count; ++i)
				{
					if (attrs[i] != nullptr && strcmp(attrs[i]->GetKind(), "AssetRef") == 0)
					{
						hasAssetRef = true;
						break;
					}
				}

				if (hasAssetRef)
				{
					// Only char arrays can hold asset reference strings
					if constexpr (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>)
					{
						const char* str = field.value;
						if (str != nullptr && str[0] != '\0')
						{
							Dia::Core::StringCRC targetId(str);
							RelationshipEdge edge(RelationshipTypes::kUses, targetId);
							if (!mOutEdges.IsFull())
							{
								mOutEdges.Add(edge);
							}
						}
					}
				}
				return *this;
			}

			// Other field kinds are not relevant to relationship scanning — silently ignored
			template<typename T>
			AssetRefScanArchive& operator&(Dia::Reflect::OwnedPtrField<T>) { return *this; }

			template<typename Base>
			AssetRefScanArchive& operator&(Dia::Reflect::PolyOwnedPtrField<Base>) { return *this; }

			template<typename T>
			AssetRefScanArchive& operator&(Dia::Reflect::RefIdField<T>) { return *this; }

		private:
			uint32_t mTypeCrc;
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& mOutEdges;
		};

		// Compile-time check: AssetRefScanArchive must satisfy the Archive concept
		static_assert(Dia::Reflect::Archive<AssetRefScanArchive>,
			"AssetRefScanArchive must satisfy Dia::Reflect::Archive concept");

		//------------------------------------------------------------------------------------
		// InferRelationships (typed template overload)
		//
		// typeCrc — StringCRC(TypeName).Value(), matching what DIA_ATTR_ASSET_REF registered.
		//------------------------------------------------------------------------------------
		template<typename T>
		void RelationshipInferrer::InferRelationships(
			const T& value,
			uint32_t typeCrc,
			const AssetTypeRegistry& typeRegistry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const
		{
			(void)typeRegistry;

			AssetRefScanArchive scanAr(typeCrc, outEdges);
			// const_cast is safe: AssetRefScanArchive only reads field values, never modifies them
			serialize(scanAr, const_cast<T&>(value), 0u);
		}

	} // namespace AssetCatalogue
} // namespace Dia
