#include <DiaCellLayers/InspectorModel.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <math.h>

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
	}
}
