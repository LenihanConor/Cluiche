#pragma once

// SelectionInput: turns per-frame pointer state into selection commands (Selection feature, mouse gestures).
// Pure logic over plain data: it knows nothing about DiaInput or the viewer; the host fills a PointerFrame each frame
// and the commands go to an ISelectionCommands sink (the viewer domain routes them through the cells.select.* path).
//
// Select tool, LEFT button only (middle/right never start, complete or disturb a gesture; camera pan owns them):
//   press on a cell                       starts a gesture (press outside the grid starts nothing, so no command on release)
//   release on the start cell (click)     ctrl -> SelectCategory(group, value at cell); shift -> ToggleCell; else SelectCell
//   release on another cell (drag)        alt -> SelectPolygon(sampled path); else SelectBox(start cell, end cell)
//   Esc (escPressed)                      Clear (and cancels a gesture in progress)
// Modifier flags are read at RELEASE time. With hasKeyboard == false modifiers count as false and Esc is ignored.
// A drag released outside the grid ends at the last valid cell (box) / uses the path so far (lasso). Only a release ON
// the start cell is a click; leaving the grid and releasing outside it is a drag (box from the start to the last valid cell).
//
// Lasso sampling rule (world space, CellPicker::ScreenToWorld): the path holds the press position; each frame the segment
// from the previous frame position to the current one is walked in steps of at most one cell width (so a fast mouse that
// jumps many cells is interpolated), and a step's world position is added whenever the cell under it differs from the
// last sampled cell. On release the final position is always the last vertex. The path holds kMaxPathPoints vertices;
// when full, new samples overwrite the last slot (the final position is never lost). A path shorter than 3 vertices is
// padded with the final position to reach the 3 SelectPolygon needs (such a degenerate polygon selects nothing).
//
// Pan tool: Process ignores the left button; the camera pan code checks PanTool().
// Generic: no game types (CL-005), no singletons (CL-006).

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	// Typed command sink. Each call returns whether the command succeeded.
	class ISelectionCommands
	{
	public:
		virtual ~ISelectionCommands() {}
		virtual bool SelectCell(int32_t q, int32_t r) = 0;
		virtual bool ToggleCell(int32_t q, int32_t r) = 0;
		virtual bool SelectBox(int32_t q0, int32_t r0, int32_t q1, int32_t r1) = 0;
		virtual bool SelectPolygon(const SelectionTools::PolyPoint* points, uint32_t count) = 0;
		virtual bool SelectCategory(Dia::Core::StringCRC layerId, float value) = 0;
		virtual bool Clear() = 0;
	};

	class SelectionInput
	{
	public:
		struct PointerFrame
		{
			float	x = 0.0f;			// pixel, top-left origin, Y down (same space as CellPickerView)
			float	y = 0.0f;
			bool	left = false;		// current button state (edges are detected here)
			bool	middle = false;
			bool	right = false;
			bool	shift = false;
			bool	ctrl = false;
			bool	alt = false;
			bool	escPressed = false;	// true only on the frame Esc went down
			bool	hasKeyboard = true;
			bool	valid = false;		// false when no input was supplied this frame
		};

		enum class Tool { Select, Pan };

		static const uint32_t kMaxPathPoints = 256;

		void	SetTool(Tool tool);	// switching cancels a gesture in progress
		Tool	GetTool() const { return mTool; }
		bool	PanTool() const { return mTool == Tool::Pan; }

		// groupValues: values of the selection-group layer for the grid (may be null); groupLayerId: that layer (kZero = none).
		void	Process(const PointerFrame& frame, const CellPickerView& view, const Dia::Graphics::CellGridDesc* grid,
			ISelectionCommands& sink, const float* groupValues, uint32_t groupValueCount, Dia::Core::StringCRC groupLayerId);

		// Gesture feedback (for the viewer to draw later).
		bool	GestureActive() const { return mActive; }
		int32_t	StartQ() const { return mStartQ; }
		int32_t	StartR() const { return mStartR; }
		int32_t	CurrentQ() const { return mLastQ; }	// last valid cell under the pointer during the gesture
		int32_t	CurrentR() const { return mLastR; }
		float	StartPixelX() const { return mStartPx; }
		float	StartPixelY() const { return mStartPy; }
		uint32_t	PathCount() const { return mPathCount; }
		const SelectionTools::PolyPoint*	Path() const { return mPath; }

		// ---- selection group (the category layer ctrl-click selects by) ----
		Dia::Core::StringCRC	SelectionGroup() const { return mGroup; }
		void	SetSelectionGroup(Dia::Core::StringCRC id) { mGroup = id; }

		// Every category layer of the grid, in registration order. Returns the total count (writes at most capacity ids).
		static uint32_t	ListGroups(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema,
			const Dia::Graphics::CellGridDesc* grid, Dia::Core::StringCRC* outIds, uint32_t capacity);
		// First category layer of the grid whose schema has ui.select_group: true, else the first category layer, else kZero.
		static Dia::Core::StringCRC	DefaultGroup(const CellLayerRegistry& registry, const Dia::ValueSchema::ValueSchemaService* schema,
			const Dia::Graphics::CellGridDesc* grid);
		// True if id is a category layer of the grid.
		static bool	IsGroupOf(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, Dia::Core::StringCRC id);

	private:
		void	Cancel();
		void	AddSample(float wx, float wy);

		Tool	mTool = Tool::Select;
		bool	mPrevLeft = false;
		bool	mActive = false;
		int32_t	mStartQ = -1, mStartR = -1;
		int32_t	mLastQ = -1, mLastR = -1;			// last valid cell during the gesture
		int32_t	mSampledQ = -1, mSampledR = -1;		// cell of the last sampled vertex
		float	mStartPx = 0.0f, mStartPy = 0.0f;
		float	mPrevX = 0.0f, mPrevY = 0.0f;		// previous frame pixel during the gesture
		uint32_t	mPathCount = 0;
		SelectionTools::PolyPoint	mPath[kMaxPathPoints];
		Dia::Core::StringCRC	mGroup;
	};
}
