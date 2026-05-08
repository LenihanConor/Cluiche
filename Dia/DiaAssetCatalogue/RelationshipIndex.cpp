#include "DiaAssetCatalogue/RelationshipIndex.h"
#include "DiaAssetCatalogue/AssetRegistry.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// RelationshipIndex
		//------------------------------------------------------------------------------------
		RelationshipIndex::RelationshipIndex()
			: mGraphDirty(true)
			, mGraph()
		{}

		RelationshipIndex::RelationshipIndex(const RelationshipIndex&)
			: mGraphDirty(true)
			, mGraph()
		{}

		RelationshipIndex& RelationshipIndex::operator=(const RelationshipIndex&)
		{
			mGraphDirty = true;
			mGraph.Clear();
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

			RelationshipGraph::EdgeResults inEdges;
			mGraph.GetInEdges(Dia::Core::CRC(assetId.Value()), inEdges);

			for (unsigned int i = 0; i < inEdges.Size(); ++i)
			{
				if (results.IsFull())
					break;

				const RelEdgePayload& payload = inEdges.At(i)->GetPayloadConst();

				// Reconstruct StringCRC values from CRC hashes. StringCRC::operator== compares
				// only mCRC, so a CRC-only StringCRC is functionally equivalent for comparisons.
				Dia::Core::StringCRC relType;
				static_cast<Dia::Core::CRC&>(relType) = payload.mRelTypeCRC;

				Dia::Core::StringCRC fromId;
				static_cast<Dia::Core::CRC&>(fromId) = inEdges.At(i)->GetFrom()->GetPayloadConst().mAssetCRC;

				results.Add(RelationshipEdge(relType, fromId));
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
			mGraph.GetInEdges(Dia::Core::CRC(assetId.Value()), inEdges);

			for (unsigned int i = 0; i < inEdges.Size(); ++i)
			{
				const RelEdgePayload& payload = inEdges.At(i)->GetPayloadConst();
				if (payload.mRelTypeCRC == relationshipType)
				{
					if (results.IsFull())
						break;

					Dia::Core::StringCRC fromId;
					static_cast<Dia::Core::CRC&>(fromId) = inEdges.At(i)->GetFrom()->GetPayloadConst().mAssetCRC;
					results.Add(fromId);
				}
			}
		}

		void RelationshipIndex::InvalidateReverseCache()
		{
			mGraphDirty = true;
		}

		void RelationshipIndex::RebuildGraph(const AssetRegistry& registry)
		{
			mGraph.Clear();

			unsigned int count = registry.GetCount();

			// Pass 1: add all asset nodes keyed by CRC value
			for (unsigned int i = 0; i < count; ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);
				Dia::Core::CRC assetCRC(record.mId.Value());
				mGraph.AddNode(assetCRC, AssetNodePayload(assetCRC));
			}

			// Pass 2: add all relationship edges
			for (unsigned int i = 0; i < count; ++i)
			{
				const AssetRecord& record = registry.GetRecordByIndex(i);
				Dia::Core::CRC fromCRC(record.mId.Value());

				for (unsigned int r = 0; r < record.mReferences.Size(); ++r)
				{
					const RelationshipEdge& ref = record.mReferences[r];
					Dia::Core::CRC targetCRC(ref.mTargetAssetId.Value());

					if (mGraph.FindNode(targetCRC) == nullptr)
						continue;

					Dia::Core::CRC relTypeCRC(ref.mRelationshipType.Value());

					// Edge key: mix all three CRCs into a unique unsigned int, then wrap as CRC
					unsigned int edgeKey = fromCRC.Value()
						^ (relTypeCRC.Value() * 2654435761u)
						^ (targetCRC.Value()  * 2246822519u);

					mGraph.AddEdge(
						Dia::Core::CRC(edgeKey),
						fromCRC,
						targetCRC,
						RelEdgePayload(relTypeCRC, targetCRC));
				}
			}

			mGraphDirty = false;
		}

	} // namespace AssetCatalogue
} // namespace Dia
