#pragma once
#ifdef DIA_DEBUG

// HistoryExportCommands: the `cells.history.export` command of DiaCellHistory (Feature 5), sim-hosted like SnapshotCommands
// (IMirrorQueryRegistrar; the handler runs on SimPU beside the recorder; Register/Unregister also on SimPU). The recorder (and
// the optional SnapshotManager / observation) are borrowed and must outlive the registration. Never registered into the DiaAPI
// CommandRegistry. Errors: { "error": "<message>" }.
//
//   cells.history.export { path, cells?, overwrite? } -> { ok:true, path, bytes, samples }
//
// The write runs on the caller's thread (SimPU) and BLOCKS the simulation for its duration (export spec Open Q1): a Large
// 40-year run with cells is the worst case. A relative path resolves to <outDir>/<app>/history/<path> (HistoryExporter::
// ResolvePath, PD-009; the history folder is created on demand); an absolute path is used as is. The host supplies the app
// name, the out folder, the git commit, a world id (via the recorder's world-id provider) and optionally a clock (unix
// seconds, null = system clock; header.generated_at is ISO 8601 UTC).

#include <DiaCellLayers/MirrorQueries.h>

#include <stdint.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class SnapshotManager;
	class ExportObservation;

	class HistoryExportCommands
	{
	public:
		using UnixTimeFn = int64_t(*)(void* user);
		static constexpr uint32_t kNameCount = 1;

		explicit HistoryExportCommands(const CellHistoryRecorder& recorder);

		// false (nothing left registered) if the registrar rejects the name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		void SetAppName(const char* app);
		void SetBuildInfo(const char* gitCommit);
		void SetOutDir(const char* outDir);							// the `Cluiche/out` folder; relative paths resolve under <outDir>/<app>/history
		void SetSnapshots(const SnapshotManager* manager) { mSnapshots = manager; }
		void SetTimeProvider(UnixTimeFn fn, void* user) { mTimeFn = fn; mTimeUser = user; }
		void SetObservation(ExportObservation* obs) { mObs = obs; }

		Json::Value Export(const Json::Value& args);

		// "YYYY-MM-DDTHH:MM:SSZ" for a unix time (UTC). Writes "" when the buffer is under 21 bytes.
		static void FormatUtc(int64_t unixSeconds, char* buf, uint32_t capacity);

	private:
		Json::Value Fail(const char* message) const;

		const CellHistoryRecorder&	mRecorder;
		const SnapshotManager*		mSnapshots = nullptr;
		ExportObservation*			mObs = nullptr;
		UnixTimeFn					mTimeFn = nullptr;
		void*						mTimeUser = nullptr;
		char						mApp[64] = {};
		char						mGit[64] = {};
		char						mOutDir[520] = {};
		bool						mRegistered = false;
	};
}

#endif // DIA_DEBUG
