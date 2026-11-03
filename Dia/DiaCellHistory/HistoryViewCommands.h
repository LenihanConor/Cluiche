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

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class HistoryViewState;
	struct HistoryConfig;

	class HistoryViewCommands
	{
	public:
		HistoryViewCommands(const CellHistoryRecorder& recorder, HistoryViewState& view, const HistoryConfig* config = nullptr);

		// false (and nothing left registered by this call) if the registrar rejects a name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		Json::Value View(const Json::Value& args);
		Json::Value Live();
		Json::Value Step(const Json::Value& args);
		Json::Value Event(const Json::Value& args);

	private:
		Json::Value Result() const;
		Json::Value Fail(const char* command, const char* message) const;

		const CellHistoryRecorder&	mRecorder;
		HistoryViewState&			mView;
		const HistoryConfig*		mConfig;
		bool						mRegistered = false;
	};
}

#endif // DIA_DEBUG
