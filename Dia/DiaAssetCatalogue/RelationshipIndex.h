#pragma once

#include "DiaAssetCatalogue/AssetRecord.h"

#include "DiaCore/CRC/CRC.h"
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
		// The graph uses Dia::Core::CRC (4 bytes) as its IDType rather than StringCRC (68 bytes),
		// and stores compact 4-byte CRC values as node/edge payloads. This keeps the graph
		// stack-allocated at ~47 KB (vs 255 KB with StringCRC payloads).
		// Public API still returns StringCRC / RelationshipEdge by CRC-based lookup into the registry.
		//
		// The cache is invalidated by AssetRegistry whenever records are added or removed.
		// Call InvalidateReverseCache() to mark the cache dirty; the next reverse query rebuilds it.
		//---------------------------------------------------------------------------------------------------------
		class RelationshipIndex
		{
		public:
			RelationshipIndex();

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

			// ------------------------------------------------------------------
			// Internal graph types — compact CRC-keyed storage
			// ------------------------------------------------------------------

			// Node payload: the asset CRC (for reverse-ref reconstruction via GetFrom()).
			struct AssetNodePayload
			{
				Dia::Core::CRC mAssetCRC;
				AssetNodePayload() {}
				explicit AssetNodePayload(const Dia::Core::CRC& c) : mAssetCRC(c) {}
			};

			// Edge payload: relationship type CRC + target asset CRC (8 bytes total).
			struct RelEdgePayload
			{
				Dia::Core::CRC mRelTypeCRC;
				Dia::Core::CRC mTargetCRC;
				RelEdgePayload() {}
				RelEdgePayload(const Dia::Core::CRC& rel, const Dia::Core::CRC& tgt)
					: mRelTypeCRC(rel), mTargetCRC(tgt) {}
			};

			// kMaxAssets must match AssetRegistry::kMaxRecords.
			// kMaxRelationships = kMaxAssets * 8 (max 8 forward refs per AssetRecord).
			static const unsigned int kMaxAssets        = 128;
			static const unsigned int kMaxRelationships = 1024;

			// Stack size with CRC IDType (4 bytes) and compact payloads:
			//   Node  = CRC(4) + AssetNodePayload(4) + OutEdgeList(8×8+8 = 72) = ~80 B × 128 = 10 KB
			//   Edge  = CRC(4) + RelEdgePayload(8)   + Node*(8) + Node*(8)     = ~28 B × 1024 = 28 KB
			//   Cache = DynamicArrayC<Edge*,8>(72 B) × 128                                   =  9 KB
			//   Total ≈ 47 KB — safe to embed in AssetRegistry on the stack.
			typedef Dia::Core::Containers::DirectedGraph<
				AssetNodePayload,
				kMaxAssets,
				RelEdgePayload,
				kMaxRelationships,
				Dia::Core::Containers::GraphPolicy::ReverseEdgeCache,
				8,
				Dia::Core::CRC> RelationshipGraph;

			bool              mGraphDirty;
			RelationshipGraph mGraph;
		};

	} // namespace AssetCatalogue
} // namespace Dia
