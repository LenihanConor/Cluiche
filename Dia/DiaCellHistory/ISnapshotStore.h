#pragma once

// ISnapshotStore: the game-side half of DiaCellHistory snapshots. The game saves and loads its own state by name;
// DiaCellHistory keeps the per-cell copies (SnapshotCellFile) next to the store's files and never touches the game's
// state files itself (spec AC 14).
//
// Requests are asynchronous: the game runs them at the next simulation frame boundary and calls `cb` on the same
// thread at that same boundary, with ok = false and a reason on failure. `message` is only valid during the callback.
// The store must serialise with its own other traffic (it may defer a request to a later boundary but must not
// drop or overwrite a pending user request; if it cannot run, it reports ok = false).
// Exists / Delete / Folder are synchronous and are called from the same PU as the requests.
// No STL in this header (PD-004).

namespace Dia::CellHistory
{
	using SnapshotCallback = void(*)(bool ok, const char* message, void* user);

	class ISnapshotStore
	{
	public:
		virtual ~ISnapshotStore() = default;
		virtual void RequestTake   (const char* name, SnapshotCallback cb, void* user) = 0;	// runs at the next frame boundary
		virtual void RequestRestore(const char* name, SnapshotCallback cb, void* user) = 0;	// runs at the next frame boundary
		virtual bool Exists(const char* name) const = 0;
		virtual bool Delete(const char* name) = 0;
		virtual const char* Folder() const = 0;		// where this store keeps files; the cell files go next to them
	};
}
