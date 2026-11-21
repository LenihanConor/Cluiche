#include <DiaCellLayers/SelectionTools.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace Dia::CellLayers::SelectionTools
{
	namespace
	{
		ToolResult Result(const CellSelection& sel, bool changed)
		{
			ToolResult res;
			res.ok = true;
			res.changed = changed;
			res.count = sel.Count();
			return res;
		}

		bool InBounds(const CellSelection& sel, int32_t q, int32_t r)
		{
			return q >= 0 && r >= 0 && uint32_t(q) < sel.Width() && uint32_t(r) < sel.Height();
		}
	}

	ToolResult SelectCell(CellSelection& sel, int32_t q, int32_t r)
	{
		if (sel.CellCount() == 0 || !InBounds(sel, q, r))
		{
			return Result(sel, false);
		}
		const uint32_t index = uint32_t(r) * sel.Width() + uint32_t(q);
		sel.BeginEdit();
		sel.Clear();
		sel.Set(index, true);
		return Result(sel, sel.EndEdit());
	}

	ToolResult ToggleCell(CellSelection& sel, int32_t q, int32_t r)
	{
		if (sel.CellCount() == 0 || !InBounds(sel, q, r))
		{
			return Result(sel, false);
		}
		const uint32_t index = uint32_t(r) * sel.Width() + uint32_t(q);
		const bool changed = sel.Set(index, !sel.Contains(index));
		return Result(sel, changed);
	}

	ToolResult SelectBox(CellSelection& sel, int32_t q0, int32_t r0, int32_t q1, int32_t r1)
	{
		if (sel.CellCount() == 0)
		{
			return Result(sel, false);
		}
		int32_t qMin = q0 < q1 ? q0 : q1;
		int32_t qMax = q0 < q1 ? q1 : q0;
		int32_t rMin = r0 < r1 ? r0 : r1;
		int32_t rMax = r0 < r1 ? r1 : r0;
		const int32_t w = int32_t(sel.Width());
		const int32_t h = int32_t(sel.Height());
		if (qMax < 0 || rMax < 0 || qMin >= w || rMin >= h)
		{
			return Result(sel, false);
		}
		if (qMin < 0) { qMin = 0; }
		if (rMin < 0) { rMin = 0; }
		if (qMax >= w) { qMax = w - 1; }
		if (rMax >= h) { rMax = h - 1; }

		sel.BeginEdit();
		sel.Clear();
		for (int32_t r = rMin; r <= rMax; ++r)
		{
			for (int32_t q = qMin; q <= qMax; ++q)
			{
				sel.Set(uint32_t(r) * uint32_t(w) + uint32_t(q), true);
			}
		}
		return Result(sel, sel.EndEdit());
	}

	ToolResult ClearSelection(CellSelection& sel)
	{
		if (sel.CellCount() == 0)
		{
			return Result(sel, false);
		}
		return Result(sel, sel.Clear());
	}

	ToolResult SelectPolygon(CellSelection& sel, const PolyPoint* vertices, uint32_t vertexCount)
	{
		if (!vertices || vertexCount < 3 || sel.CellCount() == 0 || !(sel.CellSize() > 0.0f))
		{
			ToolResult res;
			res.count = sel.Count();
			return res;
		}
		double minX = vertices[0].x, maxX = vertices[0].x, minY = vertices[0].y, maxY = vertices[0].y;
		for (uint32_t i = 0; i < vertexCount; ++i)
		{
			if (!isfinite(vertices[i].x) || !isfinite(vertices[i].y))
			{
				ToolResult res;
				res.count = sel.Count();
				return res;
			}
			minX = std::min<double>(minX, vertices[i].x);
			maxX = std::max<double>(maxX, vertices[i].x);
			minY = std::min<double>(minY, vertices[i].y);
			maxY = std::max<double>(maxY, vertices[i].y);
		}

		DIA_PROFILE_SCOPE("celllayers.selection.polygon", ::Dia::Observation::Profile::Category::kDiaGraphics);

		const float cellSize = sel.CellSize();
		const int w = int(sel.Width());
		const int h = int(sel.Height());
		sel.BeginEdit();
		sel.Clear();
		for (int q = 0; q < w; ++q)
		{
			for (int r = 0; r < h; ++r)
			{
				const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter({ q, r }, cellSize);
				const double px = c.x;
				const double py = c.y;
				// Bounding-box prefilter; max edges are exclusive, matching the crossing rule below.
				if (px < minX || px >= maxX || py < minY || py >= maxY)
				{
					continue;
				}
				// Even-odd, half-open ray crossing toward +x.
				bool inside = false;
				for (uint32_t i = 0, j = vertexCount - 1; i < vertexCount; j = i++)
				{
					const double xi = vertices[i].x, yi = vertices[i].y;
					const double xj = vertices[j].x, yj = vertices[j].y;
					if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi)
					{
						inside = !inside;
					}
				}
				if (inside)
				{
					sel.Set(uint32_t(r) * uint32_t(w) + uint32_t(q), true);
				}
			}
		}
		return Result(sel, sel.EndEdit());
	}

	CellsResult SelectCells(CellSelection& sel, const int32_t* qr, uint32_t pairCount, uint32_t* acceptedQR, uint32_t acceptedCapacity)
	{
		CellsResult out;
		out.requested = pairCount;
		if (sel.CellCount() == 0 || (!qr && pairCount > 0))
		{
			out.count = sel.Count();
			out.ok = (sel.CellCount() == 0);
			return out;
		}
		out.overCap = pairCount > kMaxCellsPerCall;
		const uint32_t considered = out.overCap ? kMaxCellsPerCall : pairCount;

		sel.BeginEdit();
		sel.Clear();
		for (uint32_t i = 0; i < considered; ++i)
		{
			const int32_t q = qr[2 * i];
			const int32_t r = qr[2 * i + 1];
			if (!InBounds(sel, q, r))
			{
				continue;
			}
			if (sel.Set(uint32_t(r) * sel.Width() + uint32_t(q), true))
			{
				if (acceptedQR && out.accepted < acceptedCapacity)
				{
					acceptedQR[2 * out.accepted] = uint32_t(q);
					acceptedQR[2 * out.accepted + 1] = uint32_t(r);
				}
				++out.accepted;
			}
		}
		out.ok = true;
		out.changed = sel.EndEdit();
		out.count = sel.Count();
		return out;
	}

	CategoryResult SelectCategory(CellSelection& sel, LayerReader& reader, const CellLayerRegistry& registry,
		const Dia::ValueSchema::ValueSchemaService* schema, Dia::Core::StringCRC layerId, const char* value)
	{
		CategoryResult res;
		res.count = sel.Count();

		const CellLayerDesc* layer = registry.Find(layerId);
		if (layer == nullptr)
		{
			res.error = CategoryError::UnknownLayer;
			return res;
		}
		if (layer->kind != CellValueKind::Category)
		{
			res.error = CategoryError::NotCategoryLayer;
			return res;
		}
		if (layer->grid == nullptr || sel.CellCount() == 0 || layer->grid->width != sel.Width() || layer->grid->height != sel.Height())
		{
			res.error = CategoryError::GridMismatch;
			return res;
		}

		// Resolve the target: enum id first (numeric value = index in the enum list), else a finite number.
		double target = 0.0;
		bool resolved = false;
		if (value != nullptr)
		{
			const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(layer->schemaKey) : nullptr;
			if (info != nullptr)
			{
				for (unsigned i = 0; i < info->mEnumValues.Size(); ++i)
				{
					if (strcmp(info->mEnumValues[i].mId.AsCStr(), value) == 0)
					{
						target = static_cast<double>(i);
						resolved = true;
						break;
					}
				}
			}
			if (!resolved && value[0] != '\0')
			{
				char* end = nullptr;
				const double parsed = strtod(value, &end);
				if (end != value && *end == '\0' && isfinite(parsed))
				{
					target = parsed;
					resolved = true;
				}
			}
		}
		if (!resolved)
		{
			res.error = CategoryError::UnknownValue;
			return res;
		}

		DIA_PROFILE_SCOPE("celllayers.selection.category", ::Dia::Observation::Profile::Category::kDiaGraphics);

		const LayerValues values = reader.Read(layerId);
		if (!values.hasData || values.values == nullptr || values.count < sel.CellCount())
		{
			res.error = CategoryError::NoData;
			return res;
		}

		sel.BeginEdit();
		sel.Clear();
		const uint32_t cells = sel.CellCount();
		for (uint32_t i = 0; i < cells; ++i)
		{
			const float v = values.values[i];
			if (v == v && static_cast<double>(floorf(v + 0.5f)) == target)
			{
				sel.Set(i, true);
			}
		}
		const bool changed = sel.EndEdit();
		res.ok = true;
		res.changed = changed;
		res.count = sel.Count();
		return res;
	}
}
