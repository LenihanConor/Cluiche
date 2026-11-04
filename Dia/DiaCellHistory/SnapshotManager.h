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
// Documented deviation: a take with no samples recorded yet is refused ("no samples recorded yet").
// Same-PU contract (SimPU); no locking; no singletons; borrowed objects must outlive the manager. No STL (PD-004).

#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <stdint.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class ISnapshotStore;

	struct TakeResult
	{
		bool	ok = false;			// accepted (true) or refused (false, see error)
		bool	pending = false;	// accepted and waiting for the store callback
		char	error[192] = {};
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

	class SnapshotManager
	{
	public:
		static constexpr uint32_t kMaxRecords = kMaxSnapshots + 8;		// 16 snapshots + room for failed records

		using UnixTimeFn = int64_t(*)(void* user);

		SnapshotManager(CellHistoryRecorder& recorder, Dia::CellLayers::CellLayerRegistry& registry,
			const Dia::ValueSchema::ValueSchemaService* schema, ISnapshotStore& store);
		SnapshotManager(const SnapshotManager&) = delete;
		SnapshotManager& operator=(const SnapshotManager&) = delete;

		void SetTimeProvider(UnixTimeFn fn, void* user) { mTimeFn = fn; mTimeUser = user; }	// null fn = system clock
		void SetWorld(const char* world);		// copied (truncated to 63 chars); null clears

		bool Take(const char* name, bool overwrite, TakeResult& out);

		bool OperationInFlight() const { return mGate.InFlight(); }
		uint32_t Count() const { return kMaxRecords; }		// slots; check mUsed
		const SnapshotRecord* Records() const { return mRecords; }

	private:
		static void OnTakeDone(bool ok, const char* message, void* user);
		void FinishTake(bool ok, const char* message);
		SnapshotRecord* SlotFor(const char* name);
		void Refuse(TakeResult& out, const char* msg) const;

		CellHistoryRecorder&						mRecorder;
		Dia::CellLayers::CellLayerRegistry&			mRegistry;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		ISnapshotStore&								mStore;
		UnixTimeFn									mTimeFn = nullptr;
		void*										mTimeUser = nullptr;
		char										mWorld[64] = {};
		SnapshotGate								mGate;
		SnapshotCellData							mPending;		// held until the store callback
		SnapshotRecord*								mPendingRecord = nullptr;
		SnapshotRecord								mRecords[kMaxRecords];
	};
}
