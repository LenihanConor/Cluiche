#pragma once
#ifdef DIA_DEBUG

// CellLayerViewerDomain: the IDebugDomain that drives the cell-layer viewer. Runs on SimPU (game logic never on main).
//
// Per Update(draw), each frame:
//  1 resolve the active base  (last chosen; start = registry default for the first grid; if the base is unregistered, fall
//    back to its grid's default; no layer left on the grid -> empty state, map cleared)
//  2 LayerUploader::Update (version-gated read + upload), 3 legend refresh, 4 draw.RequestDrawCellField(DrawRequest()),
//  5 every enabled overlay's draw callback (selection argument = the active grid's const ICellSelectionView*, null with no active grid), 6 hover.
//
// Hover seam (Open Q2): the cursor comes from IDebugDraw::GetMousePixel() (the existing path; pixel space, top-left origin).
// There is no camera on IDebugDraw (the camera lives on DebugLayerManager::SetViewport), so the application injects a
// ViewFn that returns the CellPickerView (canvas size + camera centre + zoom). With no ViewFn there is no hover.
//
// Commands (DiaAPI, RegisterCommands): cells.layer.list (JSON), cells.layer.base <id>, cells.overlay.toggle <id>,
// plus the JSON forms (E2E / DebugServer channel) cells.layer.base_by_id {"id"} and cells.overlay.toggle_by_id {"id"}. An unknown
// id returns a non-zero code and changes nothing. Selection commands (JSON only; act on ActiveSelection()): cells.select.cell
// {q,r}, .toggle {q,r}, .box {q0,r0,q1,r1}, .poly {points:[[x,y]..]} (WORLD coords), .category {layer,value}, .cells
// {cells:[[q,r]..]}, .clear {}, plus .tool {tool:"select"|"pan"} and .group {layer} (selection tool / ctrl-click group). Success {ok,count}; failure = error text (also logged on CEL at Info). Panel commands (OnCommand): "base" {id}, "grid" {index}, "toggle" {drawer}, and the selection card's "select_tool" {tool},
// "select_group" {layer}, "select_clear" {} which run the SAME code as cells.select.tool / .group / .clear.
// Query commands (JSON only; act on ActiveSelection()): cells.select.query {"expr": "<query>"} (parse + resolve + evaluate, replace selection if ok),
// cells.query.check {"expr": "<query>"} (parse + resolve + evaluate, never touch selection). Both return {ok, count} or {ok:false, error:{message, column, length, suggestion?}}.
// cells.inspect {} | {group} | {layers:[id..]} | {layerId} | {all:true} (read-only, at most one form): the Selection Inspector stats
// JSON of the active grid's selection (InspectorModel::BuildStatsJson; shape in InspectorModel.h). Errors as for the other commands.
// cells.inspector.expand {group, expanded:bool}: expands / collapses a layer group of the inspector card (panel command "inspector_expand"; the panel
// commands "select_pin" {freeze?} and "select_unpin" {} run cells.select.pin / .unpin). GetJSONState "inspector": built from the InspectorState cache
// by InspectorModel::BuildCardJson (shape in InspectorModel.h); the card is treated as open, so a single selected cell adds its "cell" table.
// GetJSONState "query": {available:true, check?:{expr, ok, count | error{message,column,length,suggestion?}}, last?:{same, from cells.select.query}}.
// Panel commands "query_check" / "query_run" {e: "<expr>"} (short key: the panel bridge caps args at 63 chars) run the same code as the JSON commands.
// Longer queries (up to 512 chars) arrive as "query_part" {i, s} chunks (i == 0 restarts the buffer; out-of-order or overflowing chunks are ignored,
// logged on CEL at Warning), then query_check / query_run {n: partCount}; a part count mismatch records "incomplete query (panel transport)".
// GetJSONState "selection": {count, tool, group, groups[{id,label,selected}], tooLarge, tooLargeText (only when tooLarge)}.
// The registry is borrowed; CellLayersModule owns and publishes it on a ServiceStream (CL-006). A consumer module resolves it
// with FindCellLayerRegistry(reader) (CellLayersModule.h) and passes *registry to the constructor.
//
// Health "dia.celllayers.viewer": Degraded while the active base's upload fails (UploadResult::ReadFailed, including a
// CellField-rejected command). Metrics "dia.celllayers.viewer.*" and "dia.celllayers.query.*". Log channel CEL.
// Generic: no game types (CL-005), no singletons (CL-006). Thread-safe between Update and command/panel calls.

#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/InspectorState.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/SelectionInput.h>
#include <DiaCellLayers/SelectionOutline.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/Threading/Mutex.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaValueSchema/RampLegend.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::CellLayers
{
	class CellLayerViewerDomain : public Dia::VisualDebugger::IDebugDomain
	{
	public:
		// Returns the canvas size (pixels) and camera centre/zoom for picking.
		typedef fastdelegate::FastDelegate0<CellPickerView> ViewFn;

		struct HoverInfo
		{
			bool		valid = false;
			int32_t		q = -1;
			int32_t		r = -1;
			float		value = 0.0f;
			Dia::Core::StringCRC	layer;
		};

		// registry, uploader and schema are borrowed and must outlive the domain. schema may be null.
		CellLayerViewerDomain(CellLayerRegistry& registry, LayerUploader& uploader, const Dia::ValueSchema::ValueSchemaService* schema, ViewFn view = ViewFn());
		~CellLayerViewerDomain() override;
		CellLayerViewerDomain(const CellLayerViewerDomain&) = delete;
		CellLayerViewerDomain& operator=(const CellLayerViewerDomain&) = delete;

		// ---- IDebugDomain ----
		Dia::Core::StringCRC GetDomainId()     const override;
		const char*          GetDisplayName()  const override;
		const char*          GetDescription()  const override;
		Dia::Core::StringCRC GetGroup()        const override;
		Dia::Core::RGBA      GetAccentColour() const override;
		bool                 HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value& out) override;
		void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

		// Per-frame driver (SimPU).
		void Update(Dia::Core::IDebugDraw& draw);

		// Makes id the active base (and its grid the active grid). False and unchanged if id is not registered.
		bool SetBase(Dia::Core::StringCRC id);
		// Switches to the default base of the grid at registry.GridAt(index). False and unchanged if out of range.
		bool SetGrid(uint32_t index);
		// False and unchanged if id is not a registered overlay.
		bool ToggleOverlay(Dia::Core::StringCRC id);
		bool IsOverlayEnabled(Dia::Core::StringCRC id) const;

		Dia::Core::StringCRC	ActiveBase() const;	// kZero in the empty state
		const Dia::Graphics::CellGridDesc*	ActiveGrid() const;
		HoverInfo				Hover() const;
		bool					IsEmptyState() const;

		// The selection of the active grid (null when there is no active grid). Switching the active grid clears the
		// selection of the grid left and of the grid entered. Overlay draw callbacks receive ActiveSelectionView().
		const ICellSelectionView*	ActiveSelectionView() const;
		CellSelection*			ActiveSelection();

		// Mouse gestures: the host submits the pointer state for the coming frame (replaces any unconsumed frame); Update runs
		// SelectionInput once per submitted frame, and each resulting command goes through the same path as the cells.select.*
		// JSON commands (same metrics and CEL logging). No submit this frame -> no gesture processing. The selection group is
		// resolved to the grid default when the active grid changes or the stored group is not a category layer of it.
		void	SubmitPointerFrame(const SelectionInput::PointerFrame& frame, const CellPickerView& view);
		// Selection Inspector compute state, ticked once per Update (SimPU) with the active grid's selection.
		InspectorState&		Inspector() { return mInspector; }
		const InspectorState&	Inspector() const { return mInspector; }
		SelectionInput&	Input() { return mInput; }	// tool choice / selection group (call on SimPU, like Update)
		const SelectionInput&	Input() const { return mInput; }

		// The state mutex guarding viewer + selection state against SimPU's Update. Cross-thread readers (MirrorQueries) hold it
		// while reading the selection. Non-recursive: do not call the locking accessors above while holding it.
		Dia::Core::Mutex&	StateMutex() const { return mMutex; }

		// Layers and overlays grouped by grid (the cells.layer.list payload).
		void ListLayers(Json::Value& out) const;

		// DiaAPI registration; idempotent. Unregistered in the destructor.
		void RegisterCommands();
		void UnregisterCommands();

		const Dia::Observation::Health::IHealthReporter&	HealthReporter() const { return mHealth; }

	private:
		void	RegisterJsonCommands();	// called by RegisterCommands
		void	RegisterSelectionCommands();	// called by RegisterCommands
		Json::Value	ExecuteSetTool(const Json::Value& params);	// cells.select.tool (locks mMutex)
		Json::Value	ExecuteSetGroup(const Json::Value& params);	// cells.select.group (locks mMutex)
		Json::Value	ExecutePin(const Json::Value& params);	// cells.select.pin (locks mMutex)
		Json::Value	ExecuteUnpin(const Json::Value& params);	// cells.select.unpin (locks mMutex)
		Json::Value	ExecuteExpand(const Json::Value& params);	// cells.inspector.expand (locks mMutex)
		Json::Value	ExecuteInspect(const Json::Value& params);	// cells.inspect (locks mMutex; read-only)
		Json::Value	FinishSelectCommand(const char* name, const char* error, Json::Value out);	// metrics + CEL log, returns out or {"error"}
		Dia::Core::StringCRC	EffectiveGroup() const;	// the stored group if valid for the active grid, else the grid default
		void	WriteSelection(Json::Value& out) const;

		enum class SelectOp { Cell, Toggle, Box, Poly, Category, Cells, Clear };
		// Runs one cells.select.* command on the active grid's selection (locks mMutex). Failure = {"error": "..."} and
		// the selection is unchanged; the reason is logged on CEL at Info.
		Json::Value	ExecuteSelect(SelectOp op, const char* name, const Json::Value& params);
		Json::Value	ExecuteSelectLocked(SelectOp op, const char* name, const Json::Value& params);	// caller holds mMutex
		// cells.select.query (apply = true) / cells.query.check (apply = false): params {"expr": "<query>"}. Returns {ok, count}
		// or {ok:false, error:{message, column, length, suggestion?}}; the selection is untouched on error and by check.
		Json::Value	ExecuteQuery(bool apply, const char* name, const Json::Value& params);	// locks mMutex
		// Same as ExecuteQuery, then records the outcome for the card ("query" in GetJSONState): the check slot for check, the last-run slot for select.
		Json::Value	ExecuteQueryRecorded(bool apply, const char* name, const Json::Value& params);	// locks mMutex (twice, not nested)
		struct QueryState;	// parser + resolver + evaluator (large, fixed capacity: heap)
		class InputSink;
		void	ProcessPointer();	// called from Update with mMutex held

		struct OverlayState
		{
			Dia::Core::StringCRC	id;
			bool					enabled = false;
		};

		void	ResolveBase();
	void	Activate(const CellLayerDesc& layer, const char* why);
	void	SetActiveGrid(const Dia::Graphics::CellGridDesc* grid);
		void	UpdateHover(Dia::Core::IDebugDraw& draw, const CellLayerDesc* base, UploadResult result);
		void	RefreshLegend(const CellLayerDesc* base);
		OverlayState*	FindOverlayState(Dia::Core::StringCRC id);
		const OverlayState*	FindOverlayState(Dia::Core::StringCRC id) const;
		uint32_t	GridIndexOf(const Dia::Graphics::CellGridDesc* grid) const;
		void	LayerText(const CellLayerDesc& layer, const char*& label, const char*& unit) const;
		void	WriteLegend(Json::Value& out) const;
		void	WriteHover(Json::Value& out) const;

		class HealthImpl : public Dia::Observation::Health::HealthReporterBase
		{
		public:
			Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("dia.celllayers.viewer"); }
		};

		CellLayerRegistry&		mRegistry;
		LayerUploader&			mUploader;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		ViewFn					mView;
		LayerReader				mReader;	// category selection reads (used under mMutex)
		HotkeyTable				mHotkeys;
		CellLayerLegend			mLegendBuilder;
		Dia::ValueSchema::RampResolver*	mResolver = nullptr;	// owned; null without a schema
		Dia::ValueSchema::LegendData	mLegend;
		char					mLegendPrefix[8] = {};
		bool					mNoData = false;		// the view source is not live and has no data for the base
		bool					mHasLegend = false;
		Dia::Core::StringCRC	mLegendBase;
		uint64_t				mLegendReads = 0;

		mutable Dia::Core::Mutex	mMutex;
		bool					mStarted = false;		// the initial base has been chosen
		Dia::Core::StringCRC	mActiveBase;			// kZero = none
		const Dia::Graphics::CellGridDesc*	mActiveGrid = nullptr;
		const Dia::Graphics::CellGridDesc*	mHotkeyGrid = nullptr;
		uint32_t				mHotkeyLayerCount = 0;
		Dia::Core::StringCRC	mLoggedBase;
		OverlayState			mOverlays[CellLayerRegistry::kMaxOverlays];
		uint32_t				mOverlayCount = 0;
		HoverInfo				mHover;
		bool					mCommandsRegistered = false;
		HealthImpl				mHealth;

		Dia::Observation::Metric::Gauge*	mMetricRegistered = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricVisible = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricReadMs = nullptr;
		Dia::Observation::Metric::Counter*	mMetricReads = nullptr;
		Dia::Observation::Metric::Counter*	mMetricUploads = nullptr;
		Dia::Observation::Metric::Counter*	mMetricSkipped = nullptr;
		uint64_t				mSeenReads = 0;
		uint64_t				mSeenUploads = 0;
		uint64_t				mSeenSkipped = 0;

		InspectorState				mInspector;
		SelectionInput				mInput;
		SelectionInput::PointerFrame	mPointerFrame;		// pending frame (valid = false when none)
		CellPickerView				mPointerView;
		const Dia::Graphics::CellGridDesc*	mInputGrid = nullptr;
		SelectionOutline			mOutline;
		Dia::Observation::Metric::Gauge*	mMetricSelectionSize = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricOutlineEdges = nullptr;
		Dia::Observation::Metric::Counter*	mMetricSelectCommands = nullptr;
		Dia::Observation::Metric::Counter*	mMetricSelectRejected = nullptr;
		Dia::Observation::Metric::Counter*	mMetricQueryRun = nullptr;			// incremented on cells.select.query success only
		Dia::Observation::Metric::Counter*	mMetricQueryRejected = nullptr;		// incremented on cells.select.query reject only
		Dia::Observation::Metric::Gauge*	mMetricQueryEvaluateMs = nullptr;	// last evaluate duration in ms
		Dia::Observation::Metric::Gauge*	mMetricQueryLastMatchCount = nullptr;	// last match count from either command
		QueryState*				mQuery = nullptr;	// owned
		Json::Value				mQueryCheck;		// last cells.query.check outcome {expr, ok, count | error}; null until one ran
		Json::Value				mQueryLast;			// last cells.select.query outcome, same shape
		static const uint32_t	kQueryPartCapacity = 512;
		char					mQueryParts[kQueryPartCapacity + 1] = {};	// panel transport: assembled query_part text (guarded by mMutex)
		uint32_t				mQueryPartLength = 0;
		uint32_t				mQueryPartCount = 0;
		void	AddQueryPart(const Json::Value& args);	// locks mMutex
		void	RunPanelQuery(bool run, const Json::Value& args);	// query_check / query_run {e} or {n}
	};
}

#endif // DIA_DEBUG
