#include "DiaAssetCatalogue/RelationshipIndex.h"
#include "DiaAssetCatalogue/AssetRegistry.h"

#include <stdio.h>
#include <new>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// RelationshipIndex
		//------------------------------------------------------------------------------------
		RelationshipIndex::RelationshipIndex()
			: mGraphDirty(true)
			, mGraph(new RelationshipGraph())
		{}

		RelationshipIndex::~RelationshipIndex()
		{
			delete mGraph;
			mGraph = nullptr;
		}

		RelationshipIndex::RelationshipIndex(const RelationshipIndex&)
			: mGraphDirty(true)
			, mGraph(new RelationshipGraph())
		{}

		RelationshipIndex& RelationshipIndex::operator=(const RelationshipIndex&)
		{
			mGraphDirty = true;
			mGraph->Clear();
			return *this;
		}

		void RelationshipIndex::GetForwardRefs(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results) const
		{
			const AssetRecord* record = registry.FindById(assetId);
			if (record == nullptr)
				return;

			for (unsigned int i = 0; i < record->mReferences.Size(); ++i)
			{
				if (!results.IsFull())
					results.Add(record->mReferences[i]);
			}
		}

		void RelationshipIndex::GetForwardRefsByType(const Dia::Core::StringCRC& assetId,
			const Dia::Core::StringCRC& relationshipType,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results) const
		{
			const AssetRecord* record = registry.FindById(assetId);
			if (record == nullptr)
				return;

			for (unsigned int i = 0; i < record->mReferences.Size(); ++i)
			{
				if (record->mReferences[i].mRelationshipType == relationshipType)
				{
					if (!results.IsFull())
						results.Add(record->mReferences[i].mTargetAssetId);
				}
			}
		}

		void RelationshipIndex::GetReverseRefs(const Dia::Core::StringCRC& assetId,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& results)
		{
			if (mGraphDirty)
				RebuildGraph(registry);

			// GetInEdges returns edges whose To node == assetId.
			// Each edge payload is the RelationshipEdge (relType + targetAssetId).
			// For a reverse query, the caller wants: who points TO assetId, and with what type?
			// Edge direction: from -> to means "from" asset has a forward ref to "to" asset.
			// So in-edges of assetId are all (fromId --relType--> assetId) relationships.
			RelationshipGraph::EdgeResults inEdges;
			mGraph->GetInEdges(assetId, inEdges);

			for (unsigned int i = 0; i < inEdges.Size(); ++i)
			{
				if (!results.IsFull())
				{
					const RelationshipEdge& payload = inEdges.At(i)->GetPayloadConst();
					// The reverse edge: "fromId" references assetId via relType
					RelationshipEdge reverseEdge(payload.mRelationshipType, inEdges.At(i)->GetFrom()->GetPayloadConst());
					results.Add(reverseEdge);
				}
			}
		}

		void RelationshipIndex::GetReverseRefsByType(const Dia::Core::StringCRC& assetId,
			const Dia::Core::StringCRC& relationshipType,
			const AssetRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& results)
		{
			if (mGraphDirty)
				RebuildGraph(registry);

			RelationshipGraph::EdgeResults inEdges;
			mGraph->GetInEdges(assetId, inEdges);

			for (unsigned int i = 0; i < inEdges.Size(); ++i)
			{
				const RelationshipEdge& payload = inEdges.At(i)->GetPayloadConst();
				if (payload.mRelationshipType == relationshipType)
				{
					if (!results.IsFull())
						results.Add(inEdges.At(i)->GetFrom()->GetPayloadConst());
				}
			}
		}

		void RelationshipIndex::InvalidateReverseCache()
		{
			mGraphDirty = true;
		}

		void RelationshipIndex::RebuildGraph(const AssetRegistry& registry)
		{
			// Rebuild in-place using Clear() to avoid dangling Edge* pointers that
			// would result from a copy/swap (Edge* in node out-edge lists and
			// InEdgeCache point into mEdgeList, which must stay stable after rebuild).
			mGraph->Clear();

			unsigned int count = registry.GetCount();

			// Pass 1: add all asset nodes (node payload = the asset ID itself)
			for (unsigned int i = 0; i < count; ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);
				mGraph->AddNode(record.mId, record.mId);
			}

			// Pass 2: add all relationship edges
			// Edge ID: composed from fromId ^ relType ^ targetId CRC values to ensure uniqueness.
			// We use snprintf into a char buffer to produce a stable string key.
			for (unsigned int i = 0; i < count; ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);

				for (unsigned int r = 0; r < record.mReferences.Size(); ++r)
				{
					const RelationshipEdge& ref = record.mReferences[r];

					// Only add the edge if the target node exists in the graph
					if (mGraph->FindNode(ref.mTargetAssetId) == nullptr)
						continue;

					// Compose a unique edge ID string from the three CRC values
					char edgeIdBuf[64];
					snprintf(edgeIdBuf, sizeof(edgeIdBuf), "%u_%u_%u",
						record.mId.Value(),
						ref.mRelationshipType.Value(),
						ref.mTargetAssetId.Value());

					mGraph->AddEdge(
						Dia::Core::StringCRC(edgeIdBuf),
						record.mId,
						ref.mTargetAssetId,
						ref);
				}
			}

			mGraphDirty = false;
		}

	} // namespace AssetCatalogue
} // namespace Dia
