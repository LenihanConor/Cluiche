#pragma once
#ifdef DIA_DEBUG

// CellHistoryCommands: the sim-hosted commands of the history recorder, registered through the same registrar seam as the
// DiaCellLayers viewer commands (IMirrorQueryRegistrar; the host adapter forwards to SimQueryHostModule). Handlers run on
// SimPU beside the recorder, so they take no locks. Register/Unregister must also be called on SimPU. The recorder is
// borrowed and must outlive the registration. Never registered into the DiaAPI CommandRegistry.
//
//   cells.history.track   { layers:[id,...] }  (1..16 ids) -> { ok:true, windowLength }
//   cells.history.untrack { layers:[id,...] }  (1..16 ids) -> { ok:true, windowLength }
//   cells.history.status  (no args)            -> { sampleCount, runId (16 hex), recordedLayers, skippedLayers, versionGatedSkips,
//                              windowLength, aggregateWindow, tracked:[ids],
//                              retained:{cells,aggregates,events:{first,latest,count}},
//                              memory:{cells,aggregates,events} (bytes), budgets:{cells,aggregates,events} (bytes),
//                              gaps:{cells,aggregates,events:[{first,last}]}, gapCap }
// Errors use the DiaCellLayers sim-hosted convention: { "error": "<message>" } and nothing changes.

#include <DiaCellLayers/MirrorQueries.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	class CellHistoryCommands
	{
	public:
		static constexpr uint32_t kMaxLayersPerCall = 16;

		explicit CellHistoryCommands(CellHistoryRecorder& recorder);

		// false (and nothing left registered by this call) if the registrar rejects a name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		Json::Value Track(const Json::Value& args);
		Json::Value Untrack(const Json::Value& args);
		Json::Value Status() const;

	private:
		CellHistoryRecorder&	mRecorder;
		bool					mRegistered = false;
	};
}

#endif // DIA_DEBUG
