#pragma once

// Snapshot rules that are independent of any recorder: name validation, the 16-snapshot cap, and the
// one-operation-in-flight gate. Used by SnapshotManager (take/restore) and the snapshot commands.
// No STL in this header (PD-004).

#include <stdint.h>

namespace Dia::CellHistory
{
	class ISnapshotStore;

	static constexpr uint32_t kMaxSnapshots = 16;
	static constexpr uint32_t kMaxSnapshotNameChars = 32;

	// 1 to 32 characters from A-Z a-z 0-9 _ - ; null and everything else is rejected (no paths, no traversal).
	bool IsValidSnapshotName(const char* name);

	// Number of `snapshot_*.cells.json` files in store.Folder(), so snapshots left by earlier sessions count. State files
	// without a cell file (orphans) are NOT counted here; the snapshot list reports them as failed.
	uint32_t CountSnapshots(const ISnapshotStore& store);

	// One take or restore at a time. TryBegin fails with "snapshot operation in progress" (and changes nothing) when an
	// operation is already in flight; End releases it.
	class SnapshotGate
	{
	public:
		bool TryBegin(char* err, uint32_t errCapacity);
		void End() { mInFlight = false; }
		bool InFlight() const { return mInFlight; }
	private:
		bool mInFlight = false;
	};

	// Pre-flight check for a take, in this order: valid name, nothing in flight (gate is only read, not begun), name
	// already exists (store state or cell file) unless `overwrite`, cap of 16 (an overwrite of a name that already has a
	// cell file does not count against it). Returns false with the reason in `err` and changes nothing.
	bool CheckTake(const char* name, bool overwrite, const ISnapshotStore& store, const SnapshotGate& gate, char* err, uint32_t errCapacity);
}
