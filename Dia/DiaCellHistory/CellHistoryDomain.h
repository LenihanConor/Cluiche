#pragma once
#ifdef DIA_DEBUG

// CellHistoryDomain: the debug-panel domain ("Cell History") that carries the history timeline card to the page. It is a
// panel-only IDebugDomain beside the DiaCellLayers viewer domain, so the viewer needs no change: the page reads this
// domain's state (TimelineCardState::Card()) to draw the timeline card and the viewer's not-live banner / untracked and
// evicted notices. Runs on SimPU with the recorder (same-PU contract; no locks).
//
// State: TimelineCardState::Card() verbatim (see TimelineCardState.h), written by GetJSONState from the cached card; the
// card is rebuilt only when Update() sees a new sample, a view change, a base-layer change, a layer-state change or a
// window-start change. Update() also drives HistoryViewState::Update() (eviction detection).
//
// Panel commands (OnCommand; the panel calls the SAME executors as the sim-hosted cells.history.* commands):
//   "view"  { sample }              -> HistoryViewCommands::View
//   "live"  {}                      -> HistoryViewCommands::Live
//   "step"  { n, band? }            -> HistoryViewCommands::Step
//   "event" { direction, type? }    -> HistoryViewCommands::Event
//   "track" { layer? }              -> CellHistoryCommands::Track({layers:[layer]}); layer defaults to the current base layer
// Failures log a Warning on HIS and change nothing. The last result is kept in LastResult() for tests and the console.
//
// Delta mode (Feature 4): when SetLayerRegistry has been called the domain adds a "delta" object to its state while the view is in
// delta mode (see DeltaCardState.h: group table restricted to the viewer's selection, hover cell with both side values) and keeps a
// "History delta" cell-detail provider registered for the selected-cell card (DeltaCellDetails). SetHoverFn supplies the hovered cell
// (q, r; false = none), e.g. from CellLayerViewerDomain::Hover(). The "timeline" object gains deltaMarkers (see TimelineModel).
//
// All collaborators are borrowed and must outlive the domain. baseLayer supplies the viewer's active base layer
// (CellLayerViewerDomain::ActiveBase via a delegate); kZero when unknown.

#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellHistory/DeltaCardState.h>
#include <DiaCellHistory/DeltaCellInfo.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/TimelineCardState.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::CellHistory
{
	class CellHistoryDomain : public Dia::VisualDebugger::IDebugDomain
	{
	public:
		typedef fastdelegate::FastDelegate0<Dia::Core::StringCRC> BaseLayerFn;

		CellHistoryDomain(CellHistoryRecorder& recorder, HistoryViewState& view, HistoryViewCommands& viewCommands,
			CellHistoryCommands& historyCommands, BaseLayerFn baseLayer,
			const Dia::ValueSchema::ValueSchemaService* schema = nullptr, const HistoryConfig* config = nullptr);

		// Returns true with the hovered cell's (q, r) when the pointer is over a cell.
		typedef fastdelegate::FastDelegate2<int32_t&, int32_t&, bool> HoverCellFn;

		// Optional, borrowed (must outlive the domain): the layer registry (selection, grouping layers, detail providers) and hover.
		void SetLayerRegistry(Dia::CellLayers::CellLayerRegistry* registry) { mLayers = registry; }
		void SetHoverFn(HoverCellFn fn) { mHover = fn; }
		const DeltaCardState& DeltaCard() const { return mDeltaCard; }
		// Bind to CellLayerViewerDomain::AddHoverExtraFn: A / B / Delta lines for the hovered cell in delta mode (see DeltaCardState::HoverExtra).
		void HoverExtra(const Dia::CellLayers::CellLayerViewerDomain::HoverInfo& hover, Dia::CellLayers::CellLayerViewerDomain::HoverExtraLines& out) { mDeltaCard.HoverExtra(hover, out); }

		Dia::Core::StringCRC GetDomainId()     const override;
		const char*          GetDisplayName()  const override;
		const char*          GetDescription()  const override;
		Dia::Core::StringCRC GetGroup()        const override;
		Dia::Core::RGBA      GetAccentColour() const override;
		bool                 HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value& out) override;
		void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

		// Per-frame (SimPU): re-evaluates eviction and refreshes the cached card.
		void Update();

		const TimelineCardState&	Card() const { return mCard; }
		const Json::Value&			LastResult() const { return mLastResult; }

	private:
		CellHistoryRecorder&		mRecorder;
		HistoryViewState&			mView;
		HistoryViewCommands&		mViewCommands;
		CellHistoryCommands&		mHistoryCommands;
		BaseLayerFn					mBaseLayer;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		const HistoryConfig*		mConfig;
		TimelineCardState			mCard;
		Json::Value					mLastResult;
		Dia::CellLayers::CellLayerRegistry*	mLayers = nullptr;
		HoverCellFn					mHover;
		DeltaCardState				mDeltaCard;
		DeltaCellDetails			mDeltaDetails;
	};
}

#endif // DIA_DEBUG
