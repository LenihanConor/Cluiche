#pragma once

// AtomicFile: the temp-file-then-rename helper shared by the cell-file writer (SnapshotCellFile) and the history exporter.
// A writer streams into TempPath(path), then Commit() renames it over the final name; on any failure the temp file is removed
// and nothing appears under the final name. Same-folder rename keeps the move atomic on the same volume.

#include <stdint.h>

namespace Dia::CellHistory
{
	class AtomicFile
	{
	public:
		// "<path>.tmp" into buf. false when it does not fit.
		static bool TempPath(const char* path, char* buf, uint32_t capacity);

		// Renames tmp over path (replacing an existing file). On failure removes tmp and writes the reason to err.
		static bool Commit(const char* tmp, const char* path, char* err, uint32_t errCapacity);

		// Removes tmp, ignoring errors.
		static void Discard(const char* tmp);
	};
}
