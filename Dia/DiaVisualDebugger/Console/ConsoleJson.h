////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleJson.h
// Description: Builds the JSON strings the debug panel page receives from the
//              console (command result, log lines, completions). Pure logic,
//              fixed caller-provided buffers, no allocation, no STL.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
//
// All builders write a NUL-terminated string to out and return true on success.
// On failure (buffer too small) they return false and write "" (when outSize > 0).
// Strings are escaped per RFC 8259: quote, backslash, control characters
// (\b \f \n \r \t or \u00XX); bytes >= 0x80 are passed through unchanged (UTF-8).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaVisualDebugger/Console/CommandCompleter.h>
#include <DiaVisualDebugger/Console/ConsoleLogRing.h>
#ifdef DIA_DEBUG
#include <DiaVisualDebugger/Console/PaletteIndex.h>
#endif

#include <cstdint>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			class ConsoleJson
			{
			public:
				// {"seq":N,"exitCode":N,"parseError":N,"text":"..."}
				static bool BuildConsoleResult(uint32_t seq, int exitCode, unsigned parseError, const char* text,
											   char* out, unsigned outSize);

				// {"lines":[{"t":1.234,"level":"INFO","channel":"...","msg":"..."},...]}
				// level is DEBUG (trace/debug), INFO, WARN or ERROR; t is seconds.
				// If not all lines fit, the list is truncated after the last line that fits
				// (valid JSON is always produced). outLinesWritten (may be null) receives the
				// number of lines emitted. Returns false only if not even the empty wrapper fits.
				static bool BuildAppendLogLines(const ConsoleLogLine* lines, unsigned count,
												char* out, unsigned outSize, unsigned* outLinesWritten);

				// {"items":[{"name":"...","rank":"exact|prefix|contains|subsequence"},...]}
				// Returns false if the whole list does not fit (items are not truncated).
				static bool BuildCompletions(const Completion* items, unsigned count, char* out, unsigned outSize);

				// {"text":"...","restore":false|true}  (history recall: restore=true means "restore the draft")
				static bool BuildSetInput(const char* text, bool restore, char* out, unsigned outSize);

#ifdef DIA_DEBUG
				// Palette results (panel-palette.md):
				// {"total":N,"counts":{"all":N,"commands":N,"domains":N},"items":[{"type":"command|domain",
				//  "name","description","category","example","domainId","action":"fill|reveal",
				//  "actionText","hl":"0101.."}]}
				// counts are true totals for the query (masks 3,1,2); items are the ranked results for
				// typeFilterMask (<= 50). If items do not fit they are truncated (valid JSON, counts intact);
				// returns false only if the wrapper/counts do not fit.
				static bool BuildPaletteResults(const PaletteIndex& index, const char* query, int typeFilterMask,
												char* out, unsigned outSize);
#endif
			};
		}
	}
}
