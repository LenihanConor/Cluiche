#pragma once

// HistoryExporter: writes a recorded run as a `dia.cell_history` v1 JSON file (Feature 5, Export).
// WriteFile is the one writer the export command and the headless runners call. It streams the stores section by
// section to the output (one layer's arrays at a time in reusable scratch buffers); it never builds the whole run as
// one JSON document. Always compiled (not DIA_DEBUG-only). Same-PU contract: call on the recorder's PU.
//
// Sections written: format, version, header, bands, samples, groups, groupSizes, aggregates, events, snapshots, tracked,
// gaps, and (options.cells) cells. The embedded `schema` section is added by a later task.
//
// Notes on content:
//  - `samples` lists the samples whose header is still retained (the recorder keeps headers for the aggregate window);
//    header.sampleFirst is the index of the first one. Arrays that cover "every sample" (groupSizes) start at
//    header.aggregateFirst.
//  - Aggregate arrays of a layer cover [first, sampleCount); a sample with no row inside that range is NaN (floats) or
//    zero counts (categories).
//  - Event types are stored as a CRC only, so their text is recovered from the recorder's schema service
//    (`kind: event` entries); an unresolved type is written as "crc:xxxxxxxx".
//  - Per-cell data (cells) is the ring's raw quantised payload (uint16 LE for float layers, uint8 for category layers).

#include <cstdint>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class SnapshotManager;

	struct ExportOptions
	{
		bool				cells = false;					// include per-cell data of tracked layers
		bool				overwrite = false;				// replace an existing file (enforced by the atomic write, task 5.5)
		const char*			app = "";
		const char*			gitCommit = "";
		const char*			generatedAt = "";				// ISO 8601, supplied by the caller
		const char*			world = "";						// world id (a provider fills it, task 5.4)
		const SnapshotManager*	snapshots = nullptr;		// optional: fills the `snapshots` list
	};

	struct ExportResult
	{
		bool		ok = false;
		uint64_t	bytes = 0;
		uint32_t	samples = 0;		// recorder sample count
		uint32_t	unresolvedEventTypes = 0;	// distinct event types with no `kind: event` schema entry (one Warning each)
		char		error[192] = {};
	};

	class HistoryExporter
	{
	public:
		static bool WriteFile(const CellHistoryRecorder& recorder, const ExportOptions& options, const char* path, ExportResult& result);

		// Resolves a user path (PD-009). An absolute (rooted) path is copied as is. A relative path must not contain a `..`
		// segment and resolves to `<outDir>/<app>/history/<path>` (outDir = the `Cluiche/out` folder, supplied by the host;
		// app empty -> no app segment). Returns false with a reason in err on an empty path, `..` in a relative path, a
		// missing outDir for a relative path, or an overflow of buf. Pure: creates nothing.
		static bool ResolvePath(const char* path, const char* app, const char* outDir, char* buf, uint32_t bufCapacity, char* err, uint32_t errCapacity);
	};
}
