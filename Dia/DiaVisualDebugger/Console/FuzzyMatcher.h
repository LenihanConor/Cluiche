////////////////////////////////////////////////////////////////////////////////
// Filename: FuzzyMatcher.h
// Description: Case-insensitive fuzzy string matching and ranking. Generic
//              (strings in, indices out); shared by the console completer and
//              the Palette. Pure logic, thread-safe, no allocation.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			enum class MatchRank
			{
				None,			// query does not match
				Subsequence,	// all query chars appear in order
				Contains,		// query is a substring
				Prefix,			// candidate starts with query
				Exact			// candidate equals query
			};

			class FuzzyMatcher
			{
			public:
				// Case-insensitive. outMatchedMask (may be null) receives 1 for each candidate
				// position that matched, 0 otherwise; at most maskSize entries are written
				// (all maskSize entries are cleared). An empty query yields Prefix.
				static MatchRank Match(const char* query, const char* candidate,
									   unsigned char* outMatchedMask, unsigned maskSize);

				// Writes indices of candidates with rank > None to outIndices, ordered by rank
				// descending then candidate string ascending (case-insensitive), stable.
				// Returns the number written (<= maxOut).
				static unsigned Rank(const char* query, const char* const* candidates,
									 unsigned candidateCount, unsigned* outIndices, unsigned maxOut);
			};
		}
	}
}
