#pragma once

// QueryLexer: batch tokenizer for cell query strings. Identifiers [A-Za-z_][A-Za-z0-9_.]*, numbers [+-]digits[.digits]
// (no exponent/units/percent), operators < <= > >= == = != , parentheses, and lower-case keywords and/or/not.
// Every token carries a 0-based column and length into the source text. Input is limited to kMaxInput characters.
// The source text is borrowed only during Tokenize; tokens keep offsets, not pointers. Fixed capacity, no heap.
// Generic: no game types (CL-005), no singletons (CL-006).

#include <DiaCellLayers/QueryError.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	enum class QueryTokenKind : uint8_t
	{
		Identifier,
		Number,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,			// "==" or "="
		NotEqual,		// "!="
		LParen,
		RParen,
		And,
		Or,
		Not,
		End,
	};

	struct QueryToken
	{
		QueryTokenKind	mKind = QueryTokenKind::End;
		uint32_t		mColumn = 0;
		uint32_t		mLength = 0;
	};

	class QueryLexer
	{
	public:
		static constexpr uint32_t kMaxInput = 512;
		static constexpr uint32_t kMaxTokens = kMaxInput + 1;	// every char one token, plus End

		// Tokenizes null-terminated text. On success the last token is End. On failure Error() is set and the
		// token list is cleared.
		bool					Tokenize(const char* text);

		uint32_t				TokenCount() const { return mCount; }
		const QueryToken&		Token(uint32_t i) const { return mTokens[i]; }
		const QueryError&		Error() const { return mError; }

	private:
		bool	Fail(const char* message, uint32_t column, uint32_t length, const char* suggestion = nullptr);
		void	Push(QueryTokenKind kind, uint32_t column, uint32_t length);

		QueryToken	mTokens[kMaxTokens];
		uint32_t	mCount = 0;
		QueryError	mError;
	};
}
