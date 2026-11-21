#include <DiaCellLayers/SelectionInput.h>

#include <DiaValueSchema/ValueSchemaService.h>

#include <math.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	namespace
	{
		// Mirrors CellLayerRegistry::HasDefaultBase for ui.select_group.
		bool HasSelectGroup(const Dia::ValueSchema::ValueSchemaService* schema, const CellLayerDesc& layer)
		{
			if (schema == nullptr) { return false; }
			const Dia::ValueSchema::ValueInfo* info = schema->Find(layer.schemaKey);
			if (info == nullptr) { return false; }
			const Json::Value& flag = info->mUi["select_group"];
			return flag.isBool() && flag.asBool();
		}
	}

	void SelectionInput::SetTool(Tool tool)
	{
		if (tool != mTool) { Cancel(); }
		mTool = tool;
	}

	void SelectionInput::Cancel()
	{
		mActive = false;
		mPathCount = 0;
	}

	void SelectionInput::AddSample(float wx, float wy)
	{
		const uint32_t slot = mPathCount < kMaxPathPoints ? mPathCount++ : kMaxPathPoints - 1;
		mPath[slot] = { wx, wy };
	}

	void SelectionInput::Process(const PointerFrame& f, const CellPickerView& view, const Dia::Graphics::CellGridDesc* grid,
		ISelectionCommands& sink, const float* groupValues, uint32_t groupValueCount, StringCRC groupLayerId)
	{
		if (!f.valid) { return; }
		const bool kb = f.hasKeyboard;
		const bool pressed = f.left && !mPrevLeft;
		const bool released = !f.left && mPrevLeft;
		mPrevLeft = f.left;

		if (kb && f.escPressed)
		{
			Cancel();
			sink.Clear();
			return;
		}
		if (mTool != Tool::Select || grid == nullptr) { Cancel(); return; }

		const PickedCell cell = CellPicker::Pick(view, f.x, f.y, *grid);
		float wx = 0.0f, wy = 0.0f;
		CellPicker::ScreenToWorld(view, f.x, f.y, wx, wy);

		if (pressed && !mActive)
		{
			if (!cell.valid) { return; }
			mActive = true;
			mStartQ = mLastQ = mSampledQ = cell.q;
			mStartR = mLastR = mSampledR = cell.r;
			mStartPx = f.x;
			mStartPy = f.y;
			mPrevX = f.x;
			mPrevY = f.y;
			mPathCount = 0;
			AddSample(wx, wy);
			return;
		}
		if (!mActive) { return; }

		// Walk the segment from the previous frame position to this one (fast mouse: interpolate).
		{
			float px = 0.0f, py = 0.0f;
			CellPicker::ScreenToWorld(view, mPrevX, mPrevY, px, py);
			const float dx = wx - px, dy = wy - py;
			const float dist = sqrtf(dx * dx + dy * dy);
			const float step = grid->cellSize > 0.0f ? grid->cellSize : 1.0f;
			uint32_t n = dist > 0.0f ? static_cast<uint32_t>(ceilf(dist / step)) : 0;
			if (n > 512) { n = 512; }
			for (uint32_t i = 1; i <= n; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(n);
				const PickedCell sc = CellPicker::Pick(view, mPrevX + (f.x - mPrevX) * t, mPrevY + (f.y - mPrevY) * t, *grid);
				if (!sc.valid) { continue; }
				mLastQ = sc.q;
				mLastR = sc.r;
				if (sc.q != mSampledQ || sc.r != mSampledR)
				{
					AddSample(px + dx * t, py + dy * t);
					mSampledQ = sc.q;
					mSampledR = sc.r;
				}
			}
			mPrevX = f.x;
			mPrevY = f.y;
		}
		if (cell.valid) { mLastQ = cell.q; mLastR = cell.r; }

		if (!released) { return; }

		// ---- release: modifiers are read now ----
		const bool shift = kb && f.shift, ctrl = kb && f.ctrl, alt = kb && f.alt;
		const bool click = cell.valid && cell.q == mStartQ && cell.r == mStartR;
		mActive = false;

		if (click)
		{
			if (ctrl)
			{
				if (groupValues != nullptr && groupLayerId != StringCRC::kZero && cell.index < groupValueCount)
				{
					sink.SelectCategory(groupLayerId, groupValues[cell.index]);
				}
			}
			else if (shift) { sink.ToggleCell(mStartQ, mStartR); }
			else { sink.SelectCell(mStartQ, mStartR); }
		}
		else if (alt)
		{
			AddSample(wx, wy);	// the final position is always the last vertex
			while (mPathCount < 3) { mPath[mPathCount] = mPath[mPathCount - 1]; ++mPathCount; }
			sink.SelectPolygon(mPath, mPathCount);
		}
		else
		{
			sink.SelectBox(mStartQ, mStartR, mLastQ, mLastR);
		}
		mPathCount = 0;
	}

	uint32_t SelectionInput::ListGroups(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService*,
		const Dia::Graphics::CellGridDesc* grid, StringCRC* outIds, uint32_t capacity)
	{
		uint32_t total = 0;
		const uint32_t count = registry.LayerCountForGrid(grid);
		for (uint32_t i = 0; i < count; ++i)
		{
			const CellLayerDesc* d = registry.LayerOfGrid(grid, i);
			if (d == nullptr || d->kind != CellValueKind::Category) { continue; }
			if (outIds != nullptr && total < capacity) { outIds[total] = d->id; }
			++total;
		}
		return total;
	}

	StringCRC SelectionInput::DefaultGroup(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema,
		const Dia::Graphics::CellGridDesc* grid)
	{
		StringCRC first = StringCRC::kZero;
		const uint32_t count = registry.LayerCountForGrid(grid);
		for (uint32_t i = 0; i < count; ++i)
		{
			const CellLayerDesc* d = registry.LayerOfGrid(grid, i);
			if (d == nullptr || d->kind != CellValueKind::Category) { continue; }
			if (HasSelectGroup(schema, *d)) { return d->id; }
			if (first == StringCRC::kZero) { first = d->id; }
		}
		return first;
	}

	bool SelectionInput::IsGroupOf(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, StringCRC id)
	{
		const CellLayerDesc* d = registry.Find(id);
		return d != nullptr && d->grid == grid && d->kind == CellValueKind::Category;
	}
}
