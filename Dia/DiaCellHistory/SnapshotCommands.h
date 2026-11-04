#pragma once
#ifdef DIA_DEBUG

// SnapshotCommands: the snapshot commands and the editor query of DiaCellHistory, sim-hosted exactly like
// CellHistoryCommands / HistoryViewCommands (IMirrorQueryRegistrar; handlers run on SimPU beside the recorder, the
// SnapshotManager and the store, no locks; Register/Unregister also on SimPU). Recorder, manager and store are borrowed and
// must outlive the registration. Never registered into the DiaAPI CommandRegistry. Errors: { "error": "<message>" }.
//
//   cells.snapshot.take    { name, overwrite? }  -> { ok:true, pending:true }   (SnapshotManager::Take; the store finishes at the next frame boundary)
//   cells.snapshot.restore { name }              -> { ok:true, pending:true }   (SnapshotManager::Restore)
//   cells.snapshot.list    (no args)             -> { snapshots:[{ name, sample, label, createdUnix, world, bytes, sameRun, status, reason? }],
//                                                     truncated? }
//   cells.snapshot.delete  { name }              -> { ok:true }  removes the store's state AND the cell file AND the record
//   cells.history.snapshot { name, layer, offset?, count? }
//                          -> { name, layer, kind:"float"|"category", min, max, width, height, encoding, offset, count, total, data }
//
// list: status is pending | ready | failed (reason when failed). Sources: this session's records (pending, ready, failed) and the
// `snapshot_*.cells.json` files of the store folder (earlier sessions). A record and a file of the same name merge: the record wins
// for pending/failed, a ready record shows the file's data. sameRun = the header run id equals the recorder's current run id.
// A cell file without store state is failed "state file missing"; a record whose cell file is gone is failed "cell file missing";
// an unreadable cell file is failed with the read error (name from the file name). LIMIT: a store state with NO cell file that the
// manager has no record of cannot be found, because ISnapshotStore cannot enumerate its states. At most kMaxListEntries entries
// (16 snapshots + failed records) are returned, sorted by createdUnix then name; more set truncated:true.
// delete: refused while a take/restore is in flight and for invalid names; a half-missing snapshot is deletable (removes what exists);
// a name with neither half and no record is an error.
// cells.history.snapshot: the quantised payload as stored (float: little-endian uint16 codes, min..max; category: uint8 ids, 255 =
// none), base64. A sim-hosted reply is limited to 16 KB (kSimPayloadBytes), so, like cells.layer.values, a layer is returned in
// chunks of at most MaxChunk() cells (offset/count/total; count defaults to min(total-offset, chunk); a larger count is clamped).
// Unknown/invalid name, unreadable file, unknown layer (layer 'x' not in snapshot 'y') and bad offset/count are errors.

#include <DiaCellLayers/MirrorQueries.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class ISnapshotStore;
	class SnapshotManager;
	class SnapshotObservation;
	class SnapshotCellCache;

	class SnapshotCommands
	{
	public:
		static constexpr uint32_t kMaxListEntries = 32;
		static constexpr uint32_t kMaxChunkCells = 4096;	// uint16 payload: 8,192 bytes -> about 10.9 KB of base64

		SnapshotCommands(const CellHistoryRecorder& recorder, SnapshotManager& manager, ISnapshotStore& store);

		// false (and nothing left registered by this call) if the registrar rejects a name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		static constexpr uint32_t kNameCount = 5;

		// Optional metrics (borrowed; null = none): delete failures, queries served, and the ready-snapshot count after a list scan.
		void SetObservation(SnapshotObservation* obs) { mObs = obs; }

		// Optional, borrowed: invalidated (Invalidate(name)) when a snapshot is deleted, so a delta never reads a deleted snapshot stale.
		void SetSnapshotCache(SnapshotCellCache* cache) { mCache = cache; }

		Json::Value Take(const Json::Value& args);
		Json::Value Restore(const Json::Value& args);
		Json::Value List();
		Json::Value Delete(const Json::Value& args);
		Json::Value Layer(const Json::Value& args);

		// Test seam: lowers the per-call cell limit (clamped to [1, kMaxChunkCells]).
		void		SetMaxChunk(uint32_t cells);
		uint32_t	MaxChunk() const { return mMaxChunk; }

	private:
		Json::Value DeleteImpl(const Json::Value& args);
		Json::Value LayerImpl(const Json::Value& args);
		void WarnOrphanOnce(const char* name, const char* why);
		Json::Value Fail(const char* command, const char* message) const;

		const CellHistoryRecorder&	mRecorder;
		SnapshotManager&			mManager;
		ISnapshotStore&				mStore;
		SnapshotObservation*		mObs = nullptr;
		SnapshotCellCache*			mCache = nullptr;
		static constexpr uint32_t	kOrphanMemory = 32;
		char						mOrphansWarned[kOrphanMemory][33] = {};		// orphan names already warned about (once per name per lifetime)
		uint32_t					mOrphanCount = 0;
		uint32_t					mMaxChunk = kMaxChunkCells;
		bool						mRegistered = false;
	};
}

#endif // DIA_DEBUG
