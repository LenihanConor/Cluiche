#pragma once

#include <Types/StageResult.h>

namespace Cluiche { namespace AppFlow {

enum class StageResultsReadResult : uint8_t { kOk, kMissing, kMalformed, kWrongFormat, kUnsupportedVersion };
const char* StageResultsReadResultName(StageResultsReadResult result);

// File: {format:"dia.stage_results", version:1, results:{<stage name>:{severity,label,utc,session_id,log_path}}}.
// Shared with Dia/DiaCLI/dia_cli/utils/stage_catalogue_view.py (load_results).
class StageResultsFile {
public:
    static constexpr const char* kFormat = "dia.stage_results";
    static constexpr int kVersion = 1;

    // `out` is cleared first and only populated when the result is kOk.
    static StageResultsReadResult Read(const char* path, StageResultsTable& table);

    // Merges with the file on disk (per stage the larger utcMs wins, tie => table; disk-only stages kept), writes
    // <path>.tmp and renames it over path. Creates the parent directory. The table is never modified.
    // A disk file that is unreadable/invalid is treated as empty. Returns false on I/O failure.
    static bool Write(const char* path, const StageResultsTable& table);
};

}} // namespace Cluiche::AppFlow
