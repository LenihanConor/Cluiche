#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		class AssetRegistry;

		//---------------------------------------------------------------------------------------------------------
		// RelationshipIndex
		//
		// Manages bidirectional relationship queries over an AssetRegistry.
		//
		// Forward refs are read directly from each AssetRecord's mReferences list.
		// Reverse refs are computed lazily on first GetReverseRefs call and cached until invalidated.
		//
		// The cache is invalidated by AssetRegistry whenever records are added or removed.
		// Call InvalidateReverseCache() to mark the cache dirty; the next reverse query rebuilds it.
		//---------------------------------------------------------------------------------------------------------
		class RelationshipIndex
		{
		public:
			RelationshipIndex();

			// Forward refs — read directly from the record's mReferences field.
			void GetForwardRefs(const Dia::Core::StringCRC& assetId,
				const AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results) const;

			void GetForwardRefsByType(const Dia::Core::StringCRC& assetId,
				const Dia::Core::StringCRC& relationshipType,
				const AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results) const;

			// Reverse refs — lazily built on first call, then cached.
			void GetReverseRefs(const Dia::Core::StringCRC& assetId,
				const AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results);

			void GetReverseRefsByType(const Dia::Core::StringCRC& assetId,
				const Dia::Core::StringCRC& relationshipType,
				const AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results);

			// Mark the reverse cache as dirty so it is rebuilt on next query.
			void InvalidateReverseCache();

		private:
			void BuildReverseIndex(const AssetRegistry& registry);

			// Flat reverse index entry: one entry per (target, from, relType) triple.
			struct ReverseEntry
			{
				Dia::Core::StringCRC mTargetId;
				Dia::Core::StringCRC mFromId;
				Dia::Core::StringCRC mRelType;

				ReverseEntry()
					: mTargetId()
					, mFromId()
					, mRelType()
				{}
			};

			static const unsigned int kMaxReverseEntries = 256; // generous for a small game

			bool                                                           mReverseCacheDirty;
			Dia::Core::Containers::DynamicArrayC<ReverseEntry, kMaxReverseEntries> mReverseIndex;
		};

	} // namespace AssetCatalogue
} // namespace Dia
