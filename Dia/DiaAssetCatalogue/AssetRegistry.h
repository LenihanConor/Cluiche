#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"
#include "DiaAssetCatalogue/RelationshipIndex.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// AssetRegistry
		//
		// Central registry of AssetRecords, keyed by composite StringCRC ID ("type.name").
		// Backed by a fixed-capacity HashTableC (1024 records).
		//
		// ID format rules (enforced at Register time):
		//   - Exactly one '.' separator; parts before and after must be non-empty
		//   - All characters must be lowercase alphanumeric, '_', or '-'
		//
		// Any call to Register() or Remove() invalidates the RelationshipIndex reverse cache.
		//---------------------------------------------------------------------------------------------------------
		class AssetRegistry
		{
		public:
			AssetRegistry();

			// Correct copy constructor — re-inserts all records so HashTableC
			// internal pointer table is consistent with the new mPayloadNodes buffer.
			AssetRegistry(const AssetRegistry& other);
			AssetRegistry& operator=(const AssetRegistry& other);

			// Register a record. Returns false if:
			//   - the ID already exists in the registry
			//   - the ID format is invalid (see above)
			bool Register(const AssetRecord& record);

			// Remove by ID. Returns false if not found.
			bool Remove(const Dia::Core::StringCRC& id);

			// Find by ID (const and mutable variants). Returns nullptr if not found.
			const AssetRecord* FindById(const Dia::Core::StringCRC& id) const;
			AssetRecord* FindById(const Dia::Core::StringCRC& id);

			// Query all records of a given asset type ID.
			// Fills results up to the array's capacity.
			void QueryByType(const Dia::Core::StringCRC& typeId,
				Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const;

			// Query all records bearing a given tag.
			// Fills results up to the array's capacity.
			void QueryByTag(const Dia::Core::StringCRC& tag,
				Dia::Core::Containers::DynamicArrayC<const AssetRecord*, 64>& results) const;

			// Total number of registered records.
			unsigned int GetCount() const;

			// Access record by slot index (0 to GetCount()-1).
			// Used internally by RelationshipIndex to iterate all records.
			const AssetRecord& GetRecordByIndex(unsigned int index) const;

			// Access to the relationship index.
			RelationshipIndex& GetRelationshipIndex();
			const RelationshipIndex& GetRelationshipIndex() const;

		private:
			// Validate that an ID string conforms to the "type.name" format rules.
			static bool IsValidAssetId(const Dia::Core::StringCRC& id);

			// Hash functor for StringCRC keys.
			class AssetRecordHashFunctor
			{
			public:
				typedef Dia::Core::StringCRC Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(const Key& key, const TableData* tableData) const;
			};

			static const unsigned int kMaxRecords    = 128;
			static const unsigned int kMaxTableSize  = 192; // kMaxRecords * 1.5

			typedef Dia::Core::Containers::HashTableC<
				Dia::Core::StringCRC,
				AssetRecord,
				AssetRecordHashFunctor,
				kMaxRecords,
				kMaxTableSize> RecordMap;

			RecordMap        mRecordMap;
			unsigned int     mCount;
			RelationshipIndex mRelationshipIndex;
		};

	} // namespace AssetCatalogue
} // namespace Dia
