#include <DiaCellHistory/DeltaCardState.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryConfig.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <cmath>

namespace Dia::CellHistory
{
	namespace
	{
		struct MaskCtx { uint8_t* mask; uint32_t count; };

		void MarkSelected(uint32_t cellIndex, void* user)
		{
			MaskCtx* c = static_cast<MaskCtx*>(user);
			if (cellIndex < c->count) { c->mask[cellIndex] = 1; }
		}

		Json::Value Number(float v, bool has)
		{
			return (has && std::isfinite(v)) ? Json::Value(static_cast<double>(v)) : Json::Value(Json::nullValue);
		}
	}

	DeltaCardState::DeltaCardState(const CellHistoryRecorder& recorder, const HistoryViewState& view)
		: mRecorder(recorder), mView(view), mBuilder(recorder), mCells(recorder)
	{
	}

	DeltaCardState::~DeltaCardState() { delete[] mMask; }

	void DeltaCardState::Clear()
	{
		mCard = Json::Value(Json::nullValue);
		mGroupsJson = Json::Value(Json::nullValue);
		mHoverJson = Json::Value(Json::nullValue);
		mGroupsKeyValid = false;
		mHoverKeyValid = false;
	}

	uint64_t DeltaCardState::LiveVersionHash(const Dia::CellLayers::CellLayerDesc& base) const
	{
		const DeltaSpec& d = mView.Delta();
		if (d.a.kind != SideKind::Now && d.b.kind != SideKind::Now) { return 0; }
		uint64_t h = 1469598103934665603ull;
		auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
		mix(base.version ? base.version() : 0);
		for (uint32_t i = 0; i < mGroupCount; ++i) { mix(mGroupDescs[i].version ? mGroupDescs[i].version() : 0); }
		return h;
	}

	void DeltaCardState::Update(Dia::CellLayers::CellLayerRegistry* registry, const Dia::ValueSchema::ValueSchemaService* schema, int32_t hoverQ, int32_t hoverR)
	{
		if (!mView.InDelta())
		{
			if (!mCard.isNull()) { Clear(); }
			return;
		}
		const DeltaSpec& d = mView.Delta();
		const Dia::CellLayers::CellLayerDesc* base = registry != nullptr ? registry->Find(d.layer) : nullptr;

		bool changed = mCard.isNull();
		if (base != nullptr)
		{
			const Dia::CellLayers::ICellSelectionView* sel = registry->SelectionView(base->grid);
			const uint32_t selVersion = sel != nullptr ? sel->Version() : 0;
			const uint32_t selCount = sel != nullptr ? sel->Count() : 0;
			const uint32_t firstRetained = mRecorder.FirstRetained();
			const uint32_t layersVersion = registry->LayersVersion();
			const uint64_t live = LiveVersionHash(*base);
			if (!mGroupsKeyValid || mKeyEpoch != mView.Epoch() || mKeySelVersion != selVersion || mKeySelCount != selCount
				|| mKeyFirstRetained != firstRetained || mKeyLayersVersion != layersVersion || mKeyLive != live)
			{
				RebuildGroups(*registry, *base, schema);
				mGroupsKeyValid = true;
				mKeyEpoch = mView.Epoch();
				mKeySelVersion = selVersion;
				mKeySelCount = selCount;
				mKeyFirstRetained = firstRetained;
				mKeyLayersVersion = layersVersion;
				mKeyLive = LiveVersionHash(*base);	// the grouping layer list may have changed in RebuildGroups
				changed = true;
			}
		}
		else if (mGroupsKeyValid)
		{
			mGroupsJson = Json::Value(Json::nullValue);
			mGroupsKeyValid = false;
			changed = true;
		}

		const uint64_t hoverVersion = (base != nullptr && base->version) ? base->version() : 0;
		if (!mHoverKeyValid || mKeyQ != hoverQ || mKeyR != hoverR || mHoverEpoch != mView.Epoch() || mHoverVersion != hoverVersion
			|| mHoverFirstRetained != mRecorder.FirstRetained())
		{
			RebuildHover(base, hoverQ, hoverR);
			mHoverKeyValid = true;
			mKeyQ = hoverQ;
			mKeyR = hoverR;
			mHoverEpoch = mView.Epoch();
			mHoverVersion = hoverVersion;
			mHoverFirstRetained = mRecorder.FirstRetained();
			changed = true;
		}

		if (changed)
		{
			Json::Value card(Json::objectValue);
			char text[80];
			card["layer"] = d.layer.AsChar();
			FormatSide(d.a, text, sizeof(text));
			card["a"] = text;
			FormatSide(d.b, text, sizeof(text));
			card["b"] = text;
			if (!mGroupsJson.isNull()) { card["groups"] = mGroupsJson; }
			card["hover"] = mHoverJson;
			mCard = card;
		}
	}

	void DeltaCardState::RebuildGroups(Dia::CellLayers::CellLayerRegistry& registry, const Dia::CellLayers::CellLayerDesc& base, const Dia::ValueSchema::ValueSchemaService* schema)
	{
		DIA_PROFILE_SCOPE("cellhistory.delta_card_groups", ::Dia::Observation::Profile::Category::kDiaGraphics);
		const DeltaSpec& d = mView.Delta();
		if (!mTable) { mTable = std::make_unique<GroupDeltaTable>(); }
		if (!mGroupDescs) { mGroupDescs = std::make_unique<Dia::CellLayers::CellLayerDesc[]>(GroupDeltaTable::kMaxGroupLayers); }

		mGroupCount = 0;
		const uint32_t layerCount = registry.LayerCountForGrid(base.grid);
		for (uint32_t i = 0; schema != nullptr && i < layerCount && mGroupCount < GroupDeltaTable::kMaxGroupLayers; ++i)
		{
			const Dia::CellLayers::CellLayerDesc* l = registry.LayerOfGrid(base.grid, i);
			if (l == nullptr || l->kind != Dia::CellLayers::CellValueKind::Category) { continue; }
			const Dia::ValueSchema::ValueInfo* info = schema->Find(l->schemaKey);
			if (info == nullptr || !HistoryGroup(*info)) { continue; }
			mGroupDescs[mGroupCount++] = *l;
		}

		const Dia::CellLayers::ICellSelectionView* sel = registry.SelectionView(base.grid);
		const uint32_t selected = sel != nullptr ? sel->Count() : 0;
		const uint8_t* mask = nullptr;
		const uint32_t n = mRecorder.CellCount();
		if (selected > 0 && n > 0)
		{
			if (mMaskCount != n)
			{
				delete[] mMask;
				mMask = new uint8_t[n];
				mMaskCount = n;
			}
			for (uint32_t i = 0; i < n; ++i) { mMask[i] = 0; }
			MaskCtx ctx = { mMask, n };
			sel->ForEach(&MarkSelected, &ctx);
			mask = mMask;
		}

		mBuilder.ResetWarnings();
		mBuilder.Build(base, d, mGroupDescs.get(), mGroupCount, mask, *mTable);
		++mBuilds;

		Json::Value g(Json::objectValue);
		g["side"] = d.groupSideB ? "B" : "A";
		g["selected"] = selected;
		g["baseNoData"] = mTable->baseNoData;
		g["truncated"] = mTable->truncated;
		Json::Value rows(Json::arrayValue);
		for (uint32_t i = 0; i < mTable->rowCount; ++i)
		{
			const GroupDeltaRow& row = mTable->rows[i];
			const Dia::CellLayers::CellLayerDesc& gl = mGroupDescs[row.groupLayerIndex];
			Json::Value o(Json::objectValue);
			o["layer"] = gl.id.AsChar();
			o["value"] = row.groupValue;
			const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(gl.schemaKey) : nullptr;
			if (info != nullptr && row.groupValue < info->mEnumValues.Size()) { o["label"] = info->mEnumValues[row.groupValue].mLabel.AsCStr(); }
			o["count"] = row.count;
			o["meanA"] = row.meanA;
			o["meanB"] = row.meanB;
			o["change"] = row.change;
			rows.append(o);
		}
		g["groups"] = rows;
		Json::Value live(Json::arrayValue);
		for (uint32_t i = 0; i < mGroupCount; ++i)
		{
			if (mTable->usedLive[i]) { live.append(mGroupDescs[i].id.AsChar()); }
		}
		g["usedLive"] = live;
		mGroupsJson = g;
	}

	void DeltaCardState::RebuildHover(const Dia::CellLayers::CellLayerDesc* base, int32_t q, int32_t r)
	{
		mHoverJson = Json::Value(Json::nullValue);
		if (base == nullptr || base->grid == nullptr || q < 0 || r < 0) { return; }
		const Dia::Graphics::CellGridDesc& grid = *base->grid;
		if (q >= static_cast<int32_t>(grid.width) || r >= static_cast<int32_t>(grid.height)) { return; }
		const uint32_t cell = static_cast<uint32_t>(r) * grid.width + static_cast<uint32_t>(q);
		DeltaCellInfo info;
		if (!mCells.Get(*base, mView.Delta(), mView.Epoch(), cell, info)) { return; }
		Json::Value h(Json::objectValue);
		h["cell"] = cell;
		h["q"] = q;
		h["r"] = r;
		h["a"] = Number(info.a, info.aHasData);
		h["b"] = Number(info.b, info.bHasData);
		h["delta"] = Number(info.delta, true);
		h["aHasData"] = info.aHasData;
		h["bHasData"] = info.bHasData;
		mHoverJson = h;
	}
}
