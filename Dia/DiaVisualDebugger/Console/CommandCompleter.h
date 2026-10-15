////////////////////////////////////////////////////////////////////////////////
// Filename: CommandCompleter.h
// Description: Completes the first token of a console line against the live
//              command registry. Snapshots the registry on every call so
//              register/unregister is reflected immediately. Thread-safe.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaVisualDebugger/Console/FuzzyMatcher.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			struct Completion
			{
				char name[64];
				MatchRank rank;
			};

			class CommandCompleter
			{
			public:
				static const unsigned kDefaultMaxResults = 5;

				// Completes the command name in text (only the first token). Returns 0 if text
				// contains a space. Writes up to maxOut completions, best first, and returns the
				// number written.
				static unsigned Complete(const char* text, Completion* out, unsigned maxOut = kDefaultMaxResults);
			};
		}
	}
}
