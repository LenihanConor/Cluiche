#pragma once

// QueryParser: recursive-descent parser for cell query strings. Syntactic only: it builds an AST in a fixed-capacity
// node pool and does not know layer names or kinds (binding is the resolver's job).
//
//   query := or ; or := and { "or" and } ; and := not { "and" not }
//   not := "not" not | primary ; primary := "(" or ")" | comparison
//   comparison := name op number | name ( "=" | "==" | "!=" ) enumId
//
// Precedence: not > and > or; parentheses override. Nesting depth (parentheses and stacked "not") is limited to
// kMaxDepth. Lexer errors are propagated unchanged. Fixed capacity, no heap, no STL. Generic (CL-005), no singletons (CL-006).

#include <DiaCellLayers/QueryError.h>
#include <DiaCellLayers/QueryLexer.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	enum class QueryNodeKind : uint8_t { Compare, And, Or, Not };
	enum class QueryCompareOp : uint8_t { Lt, Le, Gt, Ge, Eq, Ne };	// Eq covers "=" and "=="
	enum class QueryRhsKind : uint8_t { Number, EnumId };

	struct QuerySpan
	{
		uint32_t mColumn = 0;
		uint32_t mLength = 0;
	};

	struct QueryNode
	{
		static constexpr uint32_t kNone = 0xFFFFFFFFu;

		QueryNodeKind	mKind = QueryNodeKind::Compare;
		uint32_t		mLeft = kNone;		// And/Or: left operand; Not: operand
		uint32_t		mRight = kNone;		// And/Or: right operand

		// Compare only
		QueryCompareOp	mOp = QueryCompareOp::Lt;
		QueryRhsKind	mRhsKind = QueryRhsKind::Number;
		double			mValue = 0.0;		// parsed number (Number rhs only)
		QuerySpan		mName;
		QuerySpan		mOpSpan;
		QuerySpan		mRhs;				// number or enum id text
	};

	class QueryParser
	{
	public:
		static constexpr uint32_t kMaxNodes = 512;
		static constexpr uint32_t kMaxDepth = 16;

		// Parses null-terminated text. On failure Error() is set and the node pool is cleared.
		bool				Parse(const char* text);

		uint32_t			RootIndex() const { return mRoot; }
		uint32_t			NodeCount() const { return mNodeCount; }
		const QueryNode&	Node(uint32_t i) const { return mNodes[i]; }
		const QueryError&	Error() const { return mError; }

	private:
		bool		Fail(const char* message, const QueryToken& at, const char* suggestion = nullptr);
		bool		FailAt(const char* message, uint32_t column, uint32_t length, const char* suggestion = nullptr);
		bool		ParseOr(uint32_t depth, uint32_t& out);
		bool		ParseAnd(uint32_t depth, uint32_t& out);
		bool		ParseNot(uint32_t depth, uint32_t& out);
		bool		ParsePrimary(uint32_t depth, uint32_t& out);
		bool		ParseComparison(uint32_t& out);
		bool		NewNode(QueryNodeKind kind, uint32_t& out);
		const QueryToken& Peek() const { return mLexer.Token(mPos); }

		QueryLexer	mLexer;
		QueryNode	mNodes[kMaxNodes];
		uint32_t	mNodeCount = 0;
		uint32_t	mRoot = QueryNode::kNone;
		uint32_t	mPos = 0;
		const char*	mText = nullptr;
		QueryError	mError;
	};
}
