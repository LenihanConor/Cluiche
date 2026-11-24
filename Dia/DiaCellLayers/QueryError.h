#pragma once

// QueryError: a located error from lexing/parsing/binding a cell query string. Column is a 0-based character offset
// into the query text; length is the span of the offending text. Fixed buffers only (PD-004): no heap, no STL.
// Suggestion is empty when there is none.

#include <stdint.h>

namespace Dia::CellLayers
{
	struct QueryError
	{
		static constexpr uint32_t kMessageSize = 128;
		static constexpr uint32_t kSuggestionSize = 64;

		char		mMessage[kMessageSize] = {};
		uint32_t	mColumn = 0;
		uint32_t	mLength = 0;
		char		mSuggestion[kSuggestionSize] = {};

		bool		HasSuggestion() const { return mSuggestion[0] != '\0'; }
	};
}
