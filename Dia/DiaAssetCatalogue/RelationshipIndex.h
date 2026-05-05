#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/Graphs/DirectedGraph.h"

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
		// Reverse refs are served from a DirectedGraph<GraphPolicy::ReverseEdgeCache> that is rebuilt
		// lazily on the first reverse query after any registry mutation.
		//
		// The RelationshipGraph is heap-allocated: it is ~255 KB (128 nodes × StringCRC payloads,
		// 1024 edges × RelationshipEdge payloads) which is too large to embed on the stack,
		// given that AssetRegistry is itself stack-allocated in several places.
		//
		// The cache is invalidated by AssetRegistry whenever records are added or removed.
		// Call InvalidateReverseCache() to mark the cache dirty; the next reverse query rebuilds it.
		//---------------------------------------------------------------------------------------------------------
		class RelationshipIndex
		{
		public:
			RelationshipIndex();
			~RelationshipIndex();

			// Copy leaves the graph dirty so it is rebuilt lazily on first reverse query.
			// Copying a live DirectedGraph would produce dangling Edge* pointers.
			RelationshipIndex(const RelationshipIndex&);
			RelationshipIndex& operator=(const RelationshipIndex&);

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
			void RebuildGraph(const AssetRegistry& registry);

			// kMaxAssets must match AssetRegistry::kMaxRecords.
			// kMaxRelationships = kMaxAssets * 8 (max 8 forward refs per AssetRecord).
			static const unsigned int kMaxAssets        = 128;
			static const unsigned int kMaxRelationships = 1024;

			// ReverseEdgeCache policy: GetInEdges() is O(1) after the first lazy rebuild,
			// which is exactly the "rarely mutated, frequently queried" access pattern of
			// the asset registry.
			// kMaxOutEdgesPerNode=8: each asset has at most 8 forward references.
			typedef Dia::Core::Containers::DirectedGraph<
				Dia::Core::StringCRC,
				kMaxAssets,
				RelationshipEdge,
				kMaxRelationships,
				Dia::Core::Containers::GraphPolicy::ReverseEdgeCache,
				8> RelationshipGraph;

			bool               mGraphDirty;
			RelationshipGraph* mGraph;     // heap-allocated; ~255 KB per instance
		};

	} // namespace AssetCatalogue
} // namespace Dia
