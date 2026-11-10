#pragma once
#ifdef DIA_DEBUG

// HistoryViewCommands: the scrub commands of the history recorder, sim-hosted exactly like CellHistoryCommands
// (IMirrorQueryRegistrar; handlers run on SimPU beside the recorder and the HistoryViewState, no locks; Register/
// Unregister also on SimPU). The recorder, view state and optional config are borrowed and must outlive the
// registration. Never registered into the DiaAPI CommandRegistry.
//
//   cells.history.view  { sample }                   -> { ok, mode, sample, label }
//   cells.history.live  (no args)                    -> { ok, mode, sample, label }
//   cells.history.step  { n, band? }                 -> { ok, mode, sample, label }
//   cells.history.event { direction, type? }         -> { ok, mode, sample, label }
//   cells.history.delta  one of (mixing forms is an error):
//     { layer, a, b, scale? }  enter delta mode (a/b: now | sample:<i> | snapshot:<name>; scale: number > 0 or "auto").
//                              A snapshot side must name a loadable snapshot of the layer's grid ("unknown snapshot 'x'", the load error,
//                              "snapshot 'x' is for a different grid (<w> x <h>)"); needs SetSnapshotCache.
//     { scale }                fix the symmetric colour range to +/-scale, or "auto"
//     { groupside }            "A" or "B": which side's grouping defines the group table's cells (default B)
//     { off: true }            leave delta, back to the live / sample view held before it (result as view/live)
//                              -> in delta: { ok, mode:"delta", a, b, scale (null = auto), layer, groupside }
// Delta needs the layer registry (SetLayerRegistry); without it entering returns "layer registry not available".
// Integer args may be JSON integers or decimal strings (console-friendly). mode is "live" or "sample". While live,
// `sample` is the latest recorded sample (-1 when nothing is recorded yet) and label is that sample's label.
// Errors use the sim-hosted convention { "error": "<message>" } and change nothing (mode, sample, epoch).
//
// Interpretation choices:
//  (a) step while live starts from the latest sample and enters scrub mode (step 0 from live = view the latest).
//  (b) step n band: only the sign of n matters (n < 0 backwards, n > 0 forwards, n == 0 stays on the current sample).
//      Finds the nearest sample strictly before/after the current one whose band index is >= the given band. `band` is a
//      configured band name (HistoryConfig::mBands, needs the config passed to the constructor) or a band index (0..255,
//      and < the configured band count when bands are configured). None found clamps to sample 0 / the latest sample.
//  (c) event prev|next: the nearest sample strictly before/after the current one (latest when live) with an event,
//      optionally only events whose type is StringCRC(type). No such event is an error ("no earlier/later event") and
//      nothing changes; a type with no events in the log is treated the same way (no schema check).
//  (d) step/event results where clamping leaves the view on the same sample still return ok.
// A sample before the per-cell window start is accepted (HistoryViewState reports it evicted).

#include <DiaCellLayers/MirrorQueries.h>

namespace Dia::CellLayers { class CellLayerRegistry; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class HistoryViewState;
	class SnapshotCellCache;
	struct HistoryConfig;

	class HistoryViewCommands
	{
	public:
		HistoryViewCommands(const CellHistoryRecorder& recorder, HistoryViewState& view, const HistoryConfig* config = nullptr);

		// Borrowed; needed by cells.history.delta to resolve the layer and read its sides. Optional.
		void SetLayerRegistry(Dia::CellLayers::CellLayerRegistry* registry) { mLayers = registry; }

		// Borrowed; needed to enter a delta with a `snapshot:<name>` side (without it such a side is rejected with
		// "snapshot sides are not available yet"). The same cache the value source reads through. Optional.
		void SetSnapshotCache(SnapshotCellCache* cache) { mSnapshots = cache; }

		// false (and nothing left registered by this call) if the registrar rejects a name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		Json::Value View(const Json::Value& args);
		Json::Value Live();
		Json::Value Step(const Json::Value& args);
		Json::Value Event(const Json::Value& args);
		Json::Value Delta(const Json::Value& args);

	private:
		Json::Value Result() const;
		Json::Value Fail(const char* command, const char* message) const;
		Json::Value EnterDelta(const Json::Value& args);

		const CellHistoryRecorder&	mRecorder;
		HistoryViewState&			mView;
		const HistoryConfig*		mConfig;
		Dia::CellLayers::CellLayerRegistry*	mLayers = nullptr;
		SnapshotCellCache*			mSnapshots = nullptr;
		bool						mRegistered = false;
	};
}

#endif // DIA_DEBUG
