#pragma once

// SnapshotManager: takes (and, in later tasks, restores/lists/deletes) snapshots. A snapshot is two halves: the game's
// state, saved by name through ISnapshotStore, and the per-cell copies (SnapshotCellFile) written next to it. Both are
// kept or neither. DiaCellHistory never reads or writes the game's state files itself (spec AC 14).
//
// Take: the command arrives on the sim PU between frames, so capturing the in-memory cell data inside Take() IS the
// frame-boundary capture (live `read` values of every recorded layer, quantised). The store request is queued and the
// store calls back at the next boundary; only then is the cell file written (atomically). Take returns pending at once.
// On a store failure the old cell file (overwrite) is untouched and the store's own old state is the store's business;
// a partial store state is removed with Delete only if it exists. On a cell file write failure the store's half is
// removed with Delete. The one-operation-in-flight gate is released in the callback whatever happened.
//
// Restore: refuses (error text, nothing changed, store not called) on an invalid name, an operation in flight, a missing
// state or cell file, an unreadable cell file, or a grid (id, width, height) different from the recorder's. Otherwise it
// begins the gate and calls store.RequestRestore; the callback (same frame as the game restore) then, on success:
//   same run (header run id == recorder's AND header sample < SampleCount): recorder.TruncateAfter(sample) (every tier,
//     headers, events attached later AND emitted-not-attached), Warning "dropped N samples after sample S" when N > 0;
//   otherwise (foreign run, or a sample the history does not contain): recorder.ResetHistory(seed), Warning "history reset".
//   Then the view (if set) goes live with an epoch bump. On store failure nothing in history or the view changes (Error log,
//   LastRestore() holds the message). The gate is released in every path. Selection and snapshot records are never touched.
// The reset seed comes from SetResetSeedProvider (default: the time provider mixed with an increasing counter).
//
// Documented deviation: a take with no samples recorded yet is refused ("no samples recorded yet").
// Same-PU contract (SimPU); no locking; no singletons; borrowed objects must outlive the manager. No STL (PD-004).

#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <stdint.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class ISnapshotStore;
	class SnapshotObservation;
	class SnapshotCellCache;

	struct TakeResult
	{
		bool	ok = false;			// accepted (true) or refused (false, see error)
		bool	pending = false;	// accepted and waiting for the store callback
		char	error[192] = {};
	};

	class HistoryViewState;

	struct RestoreResult
	{
		bool	ok = false;			// accepted (true) or refused (false, see error)
		bool	pending = false;	// accepted and waiting for the store callback
		char	error[192] = {};
	};

	// Outcome of the most recent restore that reached the store callback.
	struct RestoreOutcome
	{
		bool		mDone = false;		// a restore has completed (successfully or not)
		bool		mOk = false;
		bool		mReset = false;		// foreign run: history was reset
		uint32_t	mDropped = 0;		// same run: samples dropped
		char		mName[33] = {};
		char		mMessage[192] = {};	// the store's message on failure, a short summary on success
	};

	enum class SnapshotStatus : uint8_t { Pending, Ready, Failed };

	struct SnapshotRecord
	{
		bool			mUsed = false;
		char			mName[33] = {};
		SnapshotStatus	mStatus = SnapshotStatus::Pending;
		char			mReason[192] = {};
		uint32_t		mSample = 0;
		char			mLabel[32] = {};
		int64_t			mCreatedUnix = 0;
		char			mWorld[64] = {};
		uint64_t		mBytes = 0;			// size of the cell file written
		uint64_t		mRunId = 0;
	};

	// What cells.history.meta and the export list for each ready snapshot (Feature 5 wires it).
	struct SnapshotSummary
	{
		char		mName[33] = {};
		uint32_t	mSample = 0;
		char		mLabel[32] = {};
	};

	class SnapshotManager
	{
	public:
		static constexpr uint32_t kMaxRecords = kMaxSnapshots + 8;		// 16 snapshots + room for failed records

		using UnixTimeFn = int64_t(*)(void* user);
		using SeedFn = uint64_t(*)(void* user);

		SnapshotManager(CellHistoryRecorder& recorder, Dia::CellLayers::CellLayerRegistry& registry,
			const Dia::ValueSchema::ValueSchemaService* schema, ISnapshotStore& store);
		SnapshotManager(const SnapshotManager&) = delete;
		SnapshotManager& operator=(const SnapshotManager&) = delete;

		void SetTimeProvider(UnixTimeFn fn, void* user) { mTimeFn = fn; mTimeUser = user; }	// null fn = system clock
		void SetResetSeedProvider(SeedFn fn, void* user) { mSeedFn = fn; mSeedUser = user; }	// null fn = default
		void SetViewState(HistoryViewState* view) { mView = view; }		// optional; borrowed
		// Optional, borrowed: invalidated (Invalidate(name)) when a take completes and writes the cell file, so an overwritten
		// snapshot is never read stale by a delta.
		void SetSnapshotCache(SnapshotCellCache* cache) { mCache = cache; }
		void SetWorld(const char* world);		// copied (truncated to 63 chars); null clears

		// Optional metrics (borrowed; null = none). Failures counted: every refused/failed take, restore and store/file failure.
		void SetObservation(SnapshotObservation* obs) { mObs = obs; }

		bool Take(const char* name, bool overwrite, TakeResult& out);

		bool Restore(const char* name, RestoreResult& out);
		const RestoreOutcome& LastRestore() const { return mLastRestore; }

		// Ready snapshots of this session's records, in record order: fills up to `cap` entries of `out`, returns the count.
		uint32_t ListSummaries(SnapshotSummary* out, uint32_t cap) const;

		// Drops the record of `name` (no files, no store). False when there is none or an operation is in flight.
		bool RemoveRecord(const char* name);

		bool OperationInFlight() const { return mGate.InFlight(); }
		uint32_t Count() const { return kMaxRecords; }		// slots; check mUsed
		const SnapshotRecord* Records() const { return mRecords; }

	private:
		bool TakeImpl(const char* name, bool overwrite, TakeResult& out);
		bool RestoreImpl(const char* name, RestoreResult& out);
		void PublishCount() const;
		static void OnTakeDone(bool ok, const char* message, void* user);
		void FinishTake(bool ok, const char* message);
		static void OnRestoreDone(bool ok, const char* message, void* user);
		void FinishRestore(bool ok, const char* message);
		uint64_t NextResetSeed();
		void RefuseRestore(RestoreResult& out, const char* msg) const;
		SnapshotRecord* SlotFor(const char* name);
		void Refuse(TakeResult& out, const char* msg) const;

		CellHistoryRecorder&						mRecorder;
		Dia::CellLayers::CellLayerRegistry&			mRegistry;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		ISnapshotStore&								mStore;
		UnixTimeFn									mTimeFn = nullptr;
		void*										mTimeUser = nullptr;
		SeedFn										mSeedFn = nullptr;
		void*										mSeedUser = nullptr;
		uint64_t									mResetCounter = 0;
		HistoryViewState*							mView = nullptr;
		SnapshotCellCache*							mCache = nullptr;
		SnapshotHeader								mRestoreHeader;
		char										mRestoreName[33] = {};
		RestoreOutcome								mLastRestore;
		char										mWorld[64] = {};
		SnapshotGate								mGate;
		SnapshotCellData							mPending;		// held until the store callback
		SnapshotRecord*								mPendingRecord = nullptr;
		SnapshotObservation*						mObs = nullptr;
		double										mTakeCaptureMs = 0.0;
		double										mRestoreStartMs = 0.0;
		SnapshotRecord								mRecords[kMaxRecords];
	};
}
