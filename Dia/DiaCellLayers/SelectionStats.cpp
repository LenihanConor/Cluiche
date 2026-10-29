#include <DiaCellLayers/SelectionStats.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <math.h>

namespace Dia::CellLayers::SelectionStats
{
	namespace
	{
		struct Accum
		{
			const LayerValues*	values = nullptr;
			uint32_t			count = 0;
			uint32_t			nanCount = 0;
			float				min = 0.0f;
			float				max = 0.0f;
			double				sum = 0.0;
		};

		struct CatAccum
		{
			const LayerValues*	values = nullptr;
			LayerStats*			out = nullptr;
			uint32_t			enumCount = 0;
			uint32_t			count = 0;
		};

		void VisitCategory(uint32_t cellIndex, void* user)
		{
			CatAccum& a = *static_cast<CatAccum*>(user);
			if (cellIndex >= a.values->count) { return; }
			const float v = a.values->values[cellIndex];
			if (v != v) { ++a.out->nanCount; return; }
			++a.count;
			if (isfinite(v) && v >= 0.0f && v == floorf(v) && v < static_cast<float>(a.enumCount))
			{
				const uint32_t idx = static_cast<uint32_t>(v);
				if (idx < LayerStats::kMaxCategories) { ++a.out->categoryCounts[idx]; return; }
			}
			++a.out->otherCount;
		}

		struct HistAccum
		{
			const LayerValues*	values = nullptr;
			Histogram*			out = nullptr;
		};

		void VisitHistogram(uint32_t cellIndex, void* user)
		{
			HistAccum& a = *static_cast<HistAccum*>(user);
			Histogram& h = *a.out;
			if (cellIndex >= a.values->count) { return; }
			const float v = a.values->values[cellIndex];
			if (v != v) { ++h.nanCount; return; }
			++h.count;
			if (!h.valid) { ++h.bins[0]; return; }
			if (v < h.minRange) { ++h.outOfRangeLow; ++h.bins[0]; return; }
			if (v > h.maxRange) { ++h.outOfRangeHigh; ++h.bins[Histogram::kBins - 1]; return; }
			const double t = (static_cast<double>(v) - static_cast<double>(h.minRange)) /
				(static_cast<double>(h.maxRange) - static_cast<double>(h.minRange));
			uint32_t bin = static_cast<uint32_t>(t * static_cast<double>(Histogram::kBins));
			if (bin >= Histogram::kBins) { bin = Histogram::kBins - 1; }
			++h.bins[bin];
		}

		void Visit(uint32_t cellIndex, void* user)
		{
			Accum& a = *static_cast<Accum*>(user);
			if (cellIndex >= a.values->count) { return; }
			const float v = a.values->values[cellIndex];
			if (v != v) { ++a.nanCount; return; }
			if (a.count == 0) { a.min = v; a.max = v; }
			else
			{
				if (v < a.min) { a.min = v; }
				if (v > a.max) { a.max = v; }
			}
			a.sum += static_cast<double>(v);
			++a.count;
		}
	}

	void ComputeFloat(const ICellSelectionView& selection, const LayerValues& values, LayerStats& out)
	{
		out.hasData = false;
		out.count = 0;
		out.nanCount = 0;
		out.hasRange = false;
		out.min = 0.0f;
		out.max = 0.0f;
		out.mean = 0.0;
		if (!values.hasData || values.values == nullptr) { return; }

		Accum a;
		a.values = &values;
		selection.ForEach(&Visit, &a);

		out.hasData = true;
		out.count = a.count;
		out.nanCount = a.nanCount;
		if (a.count > 0)
		{
			out.hasRange = true;
			out.min = a.min;
			out.max = a.max;
			out.mean = a.sum / static_cast<double>(a.count);
		}
	}

	void ComputeCategory(const ICellSelectionView& selection, const LayerValues& values, uint32_t enumValueCount, LayerStats& out)
	{
		out.hasData = false;
		out.count = 0;
		out.nanCount = 0;
		out.hasRange = false;
		out.min = 0.0f;
		out.max = 0.0f;
		out.mean = 0.0;
		out.otherCount = 0;
		out.categoryTruncated = enumValueCount > LayerStats::kMaxCategories;
		out.categoryCount = out.categoryTruncated ? LayerStats::kMaxCategories : enumValueCount;
		for (uint32_t i = 0; i < LayerStats::kMaxCategories; ++i) { out.categoryCounts[i] = 0; }
		if (!values.hasData || values.values == nullptr) { return; }

		CatAccum a;
		a.values = &values;
		a.out = &out;
		a.enumCount = enumValueCount;
		selection.ForEach(&VisitCategory, &a);
		out.hasData = true;
		out.count = a.count;
	}

	void ComputeHistogram(const ICellSelectionView& selection, const LayerValues& values, float minRange, float maxRange, Histogram& out)
	{
		out = Histogram();
		out.minRange = minRange;
		out.maxRange = maxRange;
		out.valid = isfinite(minRange) && isfinite(maxRange) && maxRange > minRange;
		if (!values.hasData || values.values == nullptr) { return; }

		HistAccum a;
		a.values = &values;
		a.out = &out;
		selection.ForEach(&VisitHistogram, &a);
		out.hasData = true;
	}

	bool ComputeHistogram(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
		const ICellSelectionView& selection, Dia::Core::StringCRC layerId, Histogram& out)
	{
		out = Histogram();
		const CellLayerDesc* desc = registry.Find(layerId);
		if (desc == nullptr || desc->kind != CellValueKind::Float) { return false; }
		const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
		if (info == nullptr || !info->mHasMin || !info->mHasMax) { return false; }
		ComputeHistogram(selection, reader.Read(layerId), static_cast<float>(info->mMin), static_cast<float>(info->mMax), out);
		return out.hasData;
	}

	bool Compute(const CellLayerRegistry& registry, LayerReader& reader, const Dia::ValueSchema::ValueSchemaService* schema,
		const ICellSelectionView& selection, Dia::Core::StringCRC layerId, LayerStats& out)
	{
		out = LayerStats();
		out.layerId = layerId;
		const CellLayerDesc* desc = registry.Find(layerId);
		if (desc == nullptr) { return false; }

		const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
		if (info != nullptr)
		{
			Dia::ValueSchema::SetText(out.label, info->mLabel.AsCStr());
			Dia::ValueSchema::SetText(out.unit, info->mUnit.AsCStr());
		}

		if (desc->kind == CellValueKind::Category)
		{
			uint32_t enumCount = 0;
			if (info != nullptr)
			{
				enumCount = static_cast<uint32_t>(info->mEnumValues.Size());
				for (uint32_t i = 0; i < enumCount && i < LayerStats::kMaxCategories; ++i)
				{
					Dia::ValueSchema::SetText(out.categoryIds[i], info->mEnumValues[i].mId.AsCStr());
					Dia::ValueSchema::SetText(out.categoryLabels[i], info->mEnumValues[i].mLabel.AsCStr());
				}
			}
			ComputeCategory(selection, reader.Read(layerId), enumCount, out);
			return out.hasData;
		}

		ComputeFloat(selection, reader.Read(layerId), out);
		return out.hasData;
	}
}
