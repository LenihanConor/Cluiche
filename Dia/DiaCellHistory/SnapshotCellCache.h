#pragma once

// SnapshotCellCache (Feature 4, task 4.9): loads snapshot cell files (SnapshotCellFile) on demand for the `snapshot:<name>` side of a
// delta, keeping the 2 most recently used snapshots in memory (about 1 MB; spec "keeps the 2 most recent snapshots").
//
// Get reads synchronously on the calling thread when the snapshot is not cached (a miss is a load, `SnapshotCellFile::Read` of
// `<store folder>/snapshot_<name>.cells.json`), and bumps the recency on a hit. A failed load leaves the cache unchanged, returns
// nullptr with the reason in the caller's buffer, and logs ONE Error (channel HIS) per failing name until that name loads again,
// is invalidated, or the cache is cleared (so a per-frame reader does not flood the log).
// The cached data is read-only to users. A pointer returned by Get stays valid until the next Get, Invalidate or Clear.
// Invalidate(name) must be called when a snapshot is overwritten or deleted; it drops the entry and, when a HistoryViewState is
// set and its delta uses that snapshot on either side, bumps the view epoch so the viewer re-reads (NotifySnapshotChanged).
// The grid check (id, width, height) is the CALLER's job: GridMatches does it against a CellGridDesc.
// Same-PU contract (SimPU); no locking; no singletons; the store and the view are borrowed and must outlive the cache.
// No STL in this header (PD-004).

#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <stdint.h>

namespace Dia::Graphics { struct CellGridDesc; }

namespace Dia::CellHistory
{
	class HistoryViewState;
	class ISnapshotStore;

	class SnapshotCellCache
	{
	public:
		static constexpr uint32_t kCapacity = 2;

		explicit SnapshotCellCache(const ISnapshotStore& store);
		~SnapshotCellCache();
		SnapshotCellCache(const SnapshotCellCache&) = delete;
		SnapshotCellCache& operator=(const SnapshotCellCache&) = delete;

		// The snapshot's cell data, loading it on a miss. nullptr (and `err` set) for an invalid name or a load failure.
		const SnapshotCellData* Get(const char* name, char* err, uint32_t errCapacity);

		// True when the cell file exists in the store folder (a cheap check; does not load). False for an invalid name.
		bool Exists(const char* name) const;
		// True when `name` is cached. Does not touch recency or the counters.
		bool Peek(const char* name) const;

		// Drops the entry (if any) and notifies the view. Harmless for an unknown name or null.
		void Invalidate(const char* name);
		void Clear();

		// Optional (borrowed): the view whose epoch bumps when an invalidated snapshot is used by the delta in force.
		void SetViewState(HistoryViewState* view) { mView = view; }

		uint32_t Count() const;
		uint64_t Hits() const { return mHits; }
		uint64_t Misses() const { return mMisses; }
		uint64_t Loads() const { return mLoads; }				// successful loads
		uint64_t LoadFailures() const { return mLoadFailures; }	// failed Get calls (invalid name or load failure)

		// True when the snapshot's grid (id, width, height) equals `grid`.
		static bool GridMatches(const SnapshotCellData& data, const Dia::Graphics::CellGridDesc& grid);

	private:
		struct Entry
		{
			SnapshotCellData*	mData = nullptr;
			char				mName[kMaxSnapshotNameChars + 1] = {};
			uint64_t			mStamp = 0;		// recency; larger = more recent
		};

		int		Find(const char* name) const;
		void	Drop(Entry& e);

		const ISnapshotStore&	mStore;
		HistoryViewState*		mView = nullptr;
		Entry					mEntries[kCapacity];
		uint64_t				mClock = 0;
		uint64_t				mHits = 0;
		uint64_t				mMisses = 0;
		uint64_t				mLoads = 0;
		uint64_t				mLoadFailures = 0;
		char					mFailedName[kMaxSnapshotNameChars + 1] = {};	// the name whose failure was last logged
	};
}
