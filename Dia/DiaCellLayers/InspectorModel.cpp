#include <DiaCellLayers/InspectorModel.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <math.h>
#include <memory>
#include <string.h>
#include <string>

namespace Dia::CellLayers
{
	SingleCellTable::SingleCellTable() : mRows(new SingleCellRow[InspectorState::kMaxShown]) {}
	SingleCellTable::~SingleCellTable() { delete[] mRows; }

	void SingleCellTable::Clear()
	{
		mRowCount = 0;
		cellIndex = 0;
		q = 0;
		r = 0;
		details.Clear();
	}

	SingleCellRow* SingleCellTable::AddRow()
	{
		if (mRowCount >= InspectorState::kMaxShown) { return nullptr; }
		SingleCellRow* row = &mRows[mRowCount++];
		*row = SingleCellRow();
		return row;
	}

	namespace InspectorModel
	{
		bool BuildSingleCell(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, bool cardOpen, SingleCellTable& out)
		{
			out.Clear();
			uint32_t cell = 0;
			const Dia::Graphics::CellGridDesc* grid = state.ActiveGrid();
			if (grid == nullptr || !ShouldRunProviders(selection, cardOpen, cell)) { return false; }

			out.cellIndex = cell;
			if (grid->width > 0)
			{
				out.q = cell % grid->width;
				out.r = cell / grid->width;
			}

			for (uint32_t i = 0; i < state.ShownLayerCount(); ++i)
			{
				const Dia::Core::StringCRC id = state.ShownLayerAt(i);
				const CellLayerDesc* desc = registry.Find(id);
				if (desc == nullptr) { continue; }
				SingleCellRow* row = out.AddRow();
				if (row == nullptr) { break; }
				row->layerId = id;
				row->isCategory = desc->kind == CellValueKind::Category;

				const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
				if (info != nullptr)
				{
					Dia::ValueSchema::SetText(row->label, info->mLabel.AsCStr());
					Dia::ValueSchema::SetText(row->unit, info->mUnit.AsCStr());
				}

				const LayerValues v = reader.Read(id);
				if (!v.hasData || v.values == nullptr || cell >= v.count) { continue; }
				row->hasData = true;
				row->value = v.values[cell];
				row->isNaN = isnan(row->value);
				if (row->isNaN || !row->isCategory) { continue; }

				const float f = row->value;
				const uint32_t enumCount = info != nullptr ? static_cast<uint32_t>(info->mEnumValues.Size()) : 0;
				const bool integral = isfinite(f) && f >= 0.0f && f == floorf(f);
				if (integral && f < static_cast<float>(enumCount))
				{
					const uint32_t idx = static_cast<uint32_t>(f);
					Dia::ValueSchema::SetText(row->enumId, info->mEnumValues[idx].mId.AsCStr());
					Dia::ValueSchema::SetText(row->enumLabel, info->mEnumValues[idx].mLabel.AsCStr());
				}
				else
				{
					row->otherValue = true;
				}
			}

			registry.Details().Build(grid, cell, schema, out.details);
			return true;
		}

		namespace
		{
			const char* KindText(CellValueKind k) { return k == CellValueKind::Category ? "category" : "float"; }

			std::string TextOf(const char* s) { return std::string(s != nullptr ? s : ""); }

			// Aggregates shared by B and A.
			void WriteStatsFields(Json::Value& o, const LayerStats& s, bool category)
			{
				o["hasData"] = s.hasData;
				o["count"] = s.count;
				o["nanCount"] = s.nanCount;
				if (category)
				{
					Json::Value cats(Json::arrayValue);
					for (uint32_t i = 0; i < s.categoryCount; ++i)
					{
						Json::Value c(Json::objectValue);
						c["id"] = TextOf(s.categoryIds[i].AsCStr());
						c["label"] = TextOf(s.categoryLabels[i].AsCStr());
						c["count"] = s.categoryCounts[i];
						cats.append(c);
					}
					o["categories"] = cats;
					o["other"] = s.otherCount;
					if (s.categoryTruncated) { o["truncated"] = true; }
				}
				else if (s.hasRange)
				{
					o["min"] = static_cast<double>(s.min);
					o["mean"] = s.mean;
					o["max"] = static_cast<double>(s.max);
				}
			}

			Json::Value HistogramJson(const Histogram& h)
			{
				Json::Value o(Json::objectValue);
				Json::Value bins(Json::arrayValue);
				for (uint32_t i = 0; i < Histogram::kBins; ++i) { bins.append(h.bins[i]); }
				o["bins"] = bins;
				o["minRange"] = static_cast<double>(h.minRange);
				o["maxRange"] = static_cast<double>(h.maxRange);
				o["valid"] = h.valid;
				o["outOfRangeLow"] = h.outOfRangeLow;
				o["outOfRangeHigh"] = h.outOfRangeHigh;
				o["count"] = h.count;
				o["nanCount"] = h.nanCount;
				return o;
			}

			// Card variant of WriteStatsFields: min / mean / max are always present (null without a range).
			void WriteCardStats(Json::Value& o, const LayerStats& s, bool category)
			{
				WriteStatsFields(o, s, category);
				if (!o.isMember("min")) { o["min"] = Json::Value(Json::nullValue); }
				if (!o.isMember("mean")) { o["mean"] = Json::Value(Json::nullValue); }
				if (!o.isMember("max")) { o["max"] = Json::Value(Json::nullValue); }
			}

			bool Fail(Json::Value& out, const std::string& msg)
			{
				out = Json::Value(Json::objectValue);
				out["error"] = msg;
				return false;
			}

			const char* SchemaKeyText(const CellLayerDesc& desc, const Dia::ValueSchema::ValueInfo* info)
			{
				return info != nullptr ? info->mFullKey.AsCStr() : desc.schemaKey.AsChar();
			}

			void WriteCell(const SingleCellTable& t, CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema, Json::Value& cell)
			{
				cell["index"] = t.cellIndex;
				cell["q"] = t.q;
				cell["r"] = t.r;
				Json::Value rows(Json::arrayValue);
				for (uint32_t i = 0; i < t.RowCount(); ++i)
				{
					const SingleCellRow& r = t.Row(i);
					Json::Value o(Json::objectValue);
					o["layer"] = TextOf(r.layerId.AsChar());
					const CellLayerDesc* desc = registry.Find(r.layerId);
					if (desc != nullptr)
					{
						o["schemaKey"] = TextOf(SchemaKeyText(*desc, schema != nullptr ? schema->Find(desc->schemaKey) : nullptr));
					}
					o["label"] = TextOf(r.label.AsCStr());
					o["unit"] = TextOf(r.unit.AsCStr());
					o["kind"] = r.isCategory ? "category" : "float";
					o["hasData"] = r.hasData;
					o["isNaN"] = r.isNaN;
					if (r.hasData && !r.isNaN) { o["value"] = static_cast<double>(r.value); }
					if (r.isCategory)
					{
						o["otherValue"] = r.otherValue;
						if (!r.otherValue && r.hasData && !r.isNaN)
						{
							o["enumId"] = TextOf(r.enumId.AsCStr());
							o["enumLabel"] = TextOf(r.enumLabel.AsCStr());
						}
					}
					rows.append(o);
				}
				cell["rows"] = rows;

				Json::Value details(Json::arrayValue);
				for (uint32_t i = 0; i < t.details.Count(); ++i)
				{
					const CellDetailRow& d = t.details.At(i);
					Json::Value o(Json::objectValue);
					switch (d.kind)
					{
					case CellDetailRowKind::Section:
						o["kind"] = "section";
						o["label"] = TextOf(d.label.AsCStr());
						break;
					case CellDetailRowKind::Value:
						o["kind"] = "value";
						o["schemaKey"] = TextOf(d.schemaKey.AsChar());
						o["resolved"] = d.resolved;
						o["label"] = TextOf(d.label.AsCStr());
						o["unit"] = TextOf(d.unit.AsCStr());
						o["tooltip"] = TextOf(d.tooltip.AsCStr());
						if (!isnan(d.value)) { o["value"] = static_cast<double>(d.value); }
						break;
					default:
						o["kind"] = "text";
						o["label"] = TextOf(d.label.AsCStr());
						o["text"] = TextOf(d.text.AsCStr());
						break;
					}
					details.append(o);
				}
				cell["details"] = details;
				if (t.details.Truncated()) { cell["detailsTruncated"] = true; }
			}
		}

		bool BuildStatsJson(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, const InspectRequest& request, Json::Value& out)
		{
			const Dia::Graphics::CellGridDesc* grid = state.ActiveGrid();
			if (grid == nullptr) { return Fail(out, "no active grid"); }

			// Resolve the layers to report.
			std::unique_ptr<Dia::Core::StringCRC[]> ids(new Dia::Core::StringCRC[InspectRequest::kMaxLayers]);
			uint32_t idCount = 0;
			switch (request.mode)
			{
			case InspectRequest::Mode::Shown:
				for (uint32_t i = 0; i < state.ShownLayerCount() && idCount < InspectRequest::kMaxLayers; ++i) { ids[idCount++] = state.ShownLayerAt(i); }
				break;
			case InspectRequest::Mode::Group:
			case InspectRequest::Mode::All:
				for (uint32_t i = 0; i < registry.LayerCountForGrid(grid) && idCount < InspectRequest::kMaxLayers; ++i)
				{
					const CellLayerDesc* d = registry.LayerOfGrid(grid, i);
					if (d == nullptr) { continue; }
					if (request.mode == InspectRequest::Mode::Group && strcmp(d->group.AsCStr(), request.group.AsCStr()) != 0) { continue; }
					ids[idCount++] = d->id;
				}
				if (request.mode == InspectRequest::Mode::Group && idCount == 0)
				{
					return Fail(out, std::string("no layers in group '") + request.group.AsCStr() + "' on the active grid");
				}
				break;
			case InspectRequest::Mode::Layers:
				for (uint32_t i = 0; i < request.layerCount; ++i)
				{
					const CellLayerDesc* d = registry.Find(request.layers[i]);
					if (d == nullptr) { return Fail(out, std::string("unknown layer '") + request.layers[i].AsChar() + "'"); }
					if (d->grid != grid) { return Fail(out, std::string("layer '") + request.layers[i].AsChar() + "' is not a layer of the active grid"); }
					ids[idCount++] = request.layers[i];
				}
				break;
			}

			out = Json::Value(Json::objectValue);
			const uint32_t selected = selection != nullptr ? selection->Count() : 0;
			out["ok"] = true;
			out["selected"] = selected;
			Json::Value g(Json::objectValue);
			g["width"] = static_cast<uint32_t>(grid->width);
			g["height"] = static_cast<uint32_t>(grid->height);
			out["grid"] = g;
			out["updating"] = state.IsUpdating();
			out["layers"] = Json::Value(Json::arrayValue);
			if (state.IsPinned())
			{
				Json::Value p(Json::objectValue);
				p["count"] = state.PinnedCount();
				p["frozen"] = state.IsFrozen();
				out["pinned"] = p;
			}
			if (selected == 0 || selection == nullptr) { return true; }

			const Dia::Core::StringCRC base = state.ShownLayerCount() > 0 ? state.ShownLayerAt(0) : Dia::Core::StringCRC::kZero;
			const CellLayerDesc* baseDesc = base != Dia::Core::StringCRC::kZero ? registry.Find(base) : nullptr;
			std::unique_ptr<LayerStats> b(new LayerStats());

			if (baseDesc != nullptr)
			{
				const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(baseDesc->schemaKey) : nullptr;
				out["base"] = TextOf(SchemaKeyText(*baseDesc, info));
				if (baseDesc->kind == CellValueKind::Float)
				{
					Histogram h;
					if (SelectionStats::ComputeHistogram(registry, reader, schema, *selection, base, h)) { out["histogram"] = HistogramJson(h); }
				}
				else if (SelectionStats::Compute(registry, reader, schema, *selection, base, *b))
				{
					Json::Value o(Json::objectValue);
					WriteStatsFields(o, *b, true);
					out["histogram"] = o;
				}
			}

			Json::Value& layers = out["layers"];
			for (uint32_t i = 0; i < idCount; ++i)
			{
				const CellLayerDesc* desc = registry.Find(ids[i]);
				if (desc == nullptr) { continue; }
				const bool category = desc->kind == CellValueKind::Category;
				const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
				*b = LayerStats();
				SelectionStats::Compute(registry, reader, schema, *selection, ids[i], *b);

				Json::Value o(Json::objectValue);
				o["layer"] = TextOf(ids[i].AsChar());
				o["schemaKey"] = TextOf(SchemaKeyText(*desc, info));
				o["kind"] = KindText(desc->kind);
				o["label"] = TextOf(info != nullptr ? info->mLabel.AsCStr() : "");
				o["unit"] = TextOf(info != nullptr ? info->mUnit.AsCStr() : "");
				o["base"] = ids[i] == base;
				WriteStatsFields(o, *b, category);

				if (state.IsPinned())
				{
					const LayerStats* a = state.StatsForA(ids[i]);
					Json::Value aj(Json::objectValue);
					if (a != nullptr)
					{
						WriteStatsFields(aj, *a, category);
						if (a->hasData && b->hasData)
						{
							LayerDelta d;
							ComputeDelta(*b, *a, d);
							Json::Value dj(Json::objectValue);
							dj["count"] = static_cast<Json::Int64>(d.countDelta);
							if (d.hasRange)
							{
								dj["min"] = d.minDelta;
								dj["mean"] = d.meanDelta;
								dj["max"] = d.maxDelta;
							}
							o["delta"] = dj;
						}
					}
					else if (state.IsFrozen() && !state.FrozenAvailable(ids[i])) { aj["notFrozen"] = true; }
					else { aj["notComputed"] = true; }
					o["a"] = aj;
				}
				layers.append(o);
			}

			if (selected == 1)
			{
				std::unique_ptr<SingleCellTable> table(new SingleCellTable());
				if (BuildSingleCell(registry, reader, schema, state, selection, true, *table))
				{
					Json::Value cell(Json::objectValue);
					WriteCell(*table, registry, schema, cell);
					out["cell"] = cell;
				}
			}
			return true;
		}

		void BuildCardJson(CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
			const InspectorState& state, const ICellSelectionView* selection, Json::Value& out)
		{
			out = Json::Value(Json::objectValue);
			const bool has = state.HasSelection() && selection != nullptr && selection->Count() > 0;
			out["hasSelection"] = has;
			out["updating"] = state.IsUpdating();
			Json::Value expanded(Json::arrayValue);
			for (uint32_t i = 0; i < state.ExpandedGroupCount(); ++i) { expanded.append(TextOf(state.ExpandedGroupAt(i))); }
			out["expandedGroups"] = expanded;
			out["layers"] = Json::Value(Json::arrayValue);
			if (state.IsPinned())
			{
				Json::Value p(Json::objectValue);
				p["count"] = state.PinnedCount();
				p["frozen"] = state.IsFrozen();
				out["pinned"] = p;
			}
			if (!has) { return; }

			const Dia::Core::StringCRC base = state.ShownLayerCount() > 0 ? state.ShownLayerAt(0) : Dia::Core::StringCRC::kZero;
			const CellLayerDesc* baseDesc = base != Dia::Core::StringCRC::kZero ? registry.Find(base) : nullptr;
			if (baseDesc != nullptr)
			{
				const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(baseDesc->schemaKey) : nullptr;
				out["base"] = TextOf(SchemaKeyText(*baseDesc, info));
				if (baseDesc->kind == CellValueKind::Float)
				{
					if (const Histogram* h = state.BaseHistogram()) { out["histogram"] = HistogramJson(*h); }
				}
				else if (const LayerStats* bs = state.StatsFor(base))
				{
					Json::Value o(Json::objectValue);
					WriteStatsFields(o, *bs, true);
					out["histogram"] = o;
				}
			}

			Json::Value& layers = out["layers"];
			for (uint32_t i = 0; i < state.ShownLayerCount(); ++i)
			{
				const Dia::Core::StringCRC id = state.ShownLayerAt(i);
				const CellLayerDesc* desc = registry.Find(id);
				if (desc == nullptr) { continue; }
				const bool category = desc->kind == CellValueKind::Category;
				const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
				const LayerStats* b = state.StatsFor(id);

				Json::Value o(Json::objectValue);
				o["id"] = TextOf(id.AsChar());
				o["schemaKey"] = TextOf(SchemaKeyText(*desc, info));
				o["label"] = TextOf(info != nullptr ? info->mLabel.AsCStr() : "");
				o["unit"] = TextOf(info != nullptr ? info->mUnit.AsCStr() : "");
				const char* tip = "";
				if (info != nullptr) { tip = info->mDescription.Length() > 0 ? info->mDescription.AsCStr() : info->mHint.AsCStr(); }
				o["tooltip"] = TextOf(tip);
				o["kind"] = KindText(desc->kind);
				o["group"] = TextOf(desc->group.AsCStr());
				if (info != nullptr && info->mHasMin && info->mHasMax)
				{
					o["rangeMin"] = info->mMin;
					o["rangeMax"] = info->mMax;
				}
				const LayerStats none;
				WriteCardStats(o, b != nullptr ? *b : none, category);

				if (state.IsPinned())
				{
					const LayerStats* a = state.StatsForA(id);
					Json::Value aj(Json::objectValue);
					if (a != nullptr)
					{
						WriteCardStats(aj, *a, category);
						if (b != nullptr && a->hasData && b->hasData)
						{
							LayerDelta d;
							ComputeDelta(*b, *a, d);
							Json::Value dj(Json::objectValue);
							dj["count"] = static_cast<Json::Int64>(d.countDelta);
							dj["min"] = d.hasRange ? Json::Value(d.minDelta) : Json::Value(Json::nullValue);
							dj["mean"] = d.hasRange ? Json::Value(d.meanDelta) : Json::Value(Json::nullValue);
							dj["max"] = d.hasRange ? Json::Value(d.maxDelta) : Json::Value(Json::nullValue);
							o["delta"] = dj;
						}
					}
					else if (state.IsFrozen() && !state.FrozenAvailable(id)) { aj["notFrozen"] = true; }
					else { aj["notComputed"] = true; }
					o["a"] = aj;
				}
				layers.append(o);
			}

			if (selection->Count() == 1)
			{
				std::unique_ptr<SingleCellTable> table(new SingleCellTable());
				if (BuildSingleCell(registry, reader, schema, state, selection, true, *table))
				{
					Json::Value cell(Json::objectValue);
					WriteCell(*table, registry, schema, cell);
					out["cell"] = cell;
				}
			}
		}
	}
}
