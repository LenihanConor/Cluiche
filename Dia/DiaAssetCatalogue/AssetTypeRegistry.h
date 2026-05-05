#pragma once

#include "DiaAssetCatalogue/AssetTypeDescriptor.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// AssetTypeRegistry
		//
		// Central registry of AssetTypeDescriptors, keyed by StringCRC type ID.
		// Backed by a fixed-capacity HashTableC (64 entries).
		//
		// Usage:
		//   AssetTypeRegistry registry;
		//   RegisterBuiltInAssetTypes(registry);
		//
		//   const AssetTypeDescriptor* desc = registry.FindByTypeId(StringCRC("config"));
		//   const AssetTypeDescriptor* desc = registry.FindByFilePath(myFilePath);
		//---------------------------------------------------------------------------------------------------------
		class AssetTypeRegistry
		{
		public:
			AssetTypeRegistry();

			// Register a descriptor. Returns false if the type ID is already registered.
			bool Register(const AssetTypeDescriptor& descriptor);

			// Lookup by type ID. Returns nullptr if not found.
			const AssetTypeDescriptor* FindByTypeId(const Dia::Core::StringCRC& typeId) const;

			// Lookup by file path — matches the filename against registered *.suffix patterns.
			// Returns the first matching descriptor, or nullptr if none matches.
			const AssetTypeDescriptor* FindByFilePath(const Dia::Core::FilePath& path) const;

			// Lookup by raw filename string — same suffix matching as FindByFilePath but
			// without requiring a registered PathAlias. Used by runtime file scanners.
			const AssetTypeDescriptor* FindByFileName(const char* filename) const;

			// Number of registered types.
			unsigned int GetCount() const;

		private:
			// Hash functor for StringCRC keys (delegates to the underlying CRC value).
			class AssetTypeHashFunctor
			{
			public:
				typedef Dia::Core::StringCRC Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(Key key, const TableData* tableData) const;
			};

			static const unsigned int kMaxTypes      = 64;
			static const unsigned int kMaxTableSize  = static_cast<unsigned int>(kMaxTypes * 1.5); // 96

			typedef Dia::Core::Containers::HashTableC<
				Dia::Core::StringCRC,
				AssetTypeDescriptor,
				AssetTypeHashFunctor,
				kMaxTypes,
				kMaxTableSize> TypeMap;

			TypeMap      mTypeMap;
			unsigned int mCount;
		};

	} // namespace AssetCatalogue
} // namespace Dia
