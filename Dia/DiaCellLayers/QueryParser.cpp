#include <DiaCellLayers/QueryParser.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <stdlib.h>
#include <string.h>

namespace Dia::CellLayers
{
	bool QueryParser::FailAt(const char* message, uint32_t column, uint32_t length, const char* suggestion)
	{
		mError = QueryError();
		strncpy_s(mError.mMessage, QueryError::kMessageSize, message, _TRUNCATE);
		mError.mColumn = column;
		mError.mLength = length;
		if (suggestion != nullptr)
		{
			strncpy_s(mError.mSuggestion, QueryError::kSuggestionSize, suggestion, _TRUNCATE);
		}
		mNodeCount = 0;
		mRoot = QueryNode::kNone;
		return false;
	}

	bool QueryParser::Fail(const char* message, const QueryToken& at, const char* suggestion)
	{
		return FailAt(message, at.mColumn, at.mLength, suggestion);
	}

	bool QueryParser::NewNode(QueryNodeKind kind, uint32_t& out)
	{
		if (mNodeCount >= kMaxNodes)
		{
			return FailAt("query is too complex", Peek().mColumn, Peek().mLength);
		}
		out = mNodeCount++;
		mNodes[out] = QueryNode();
		mNodes[out].mKind = kind;
		return true;
	}

	bool QueryParser::Parse(const char* text)
	{
		DIA_PROFILE_SCOPE("celllayers.query.parse", ::Dia::Observation::Profile::Category::kDiaGraphics);

		mNodeCount = 0;
		mRoot = QueryNode::kNone;
		mPos = 0;
		mError = QueryError();
		mText = text;

		if (!mLexer.Tokenize(text))
		{
			mError = mLexer.Error();
			return false;
		}

		uint32_t root = QueryNode::kNone;
		if (!ParseOr(0, root))
		{
			return false;
		}
		const QueryToken& t = Peek();
		if (t.mKind == QueryTokenKind::RParen)
		{
			return Fail("unmatched ')'", t);
		}
		if (t.mKind != QueryTokenKind::End)
		{
			return Fail("unexpected token", t);
		}
		mRoot = root;
		return true;
	}

	bool QueryParser::ParseOr(uint32_t depth, uint32_t& out)
	{
		uint32_t left = QueryNode::kNone;
		if (!ParseAnd(depth, left)) { return false; }
		while (Peek().mKind == QueryTokenKind::Or)
		{
			++mPos;
			uint32_t right = QueryNode::kNone;
			if (!ParseAnd(depth, right)) { return false; }
			uint32_t node;
			if (!NewNode(QueryNodeKind::Or, node)) { return false; }
			mNodes[node].mLeft = left;
			mNodes[node].mRight = right;
			left = node;
		}
		out = left;
		return true;
	}

	bool QueryParser::ParseAnd(uint32_t depth, uint32_t& out)
	{
		uint32_t left = QueryNode::kNone;
		if (!ParseNot(depth, left)) { return false; }
		while (Peek().mKind == QueryTokenKind::And)
		{
			++mPos;
			uint32_t right = QueryNode::kNone;
			if (!ParseNot(depth, right)) { return false; }
			uint32_t node;
			if (!NewNode(QueryNodeKind::And, node)) { return false; }
			mNodes[node].mLeft = left;
			mNodes[node].mRight = right;
			left = node;
		}
		out = left;
		return true;
	}

	bool QueryParser::ParseNot(uint32_t depth, uint32_t& out)
	{
		if (Peek().mKind != QueryTokenKind::Not)
		{
			return ParsePrimary(depth, out);
		}
		const QueryToken notToken = Peek();
		if (depth + 1 > kMaxDepth)
		{
			return Fail("query is nested too deeply (maximum depth 16)", notToken);
		}
		++mPos;
		uint32_t operand = QueryNode::kNone;
		if (!ParseNot(depth + 1, operand)) { return false; }
		uint32_t node;
		if (!NewNode(QueryNodeKind::Not, node)) { return false; }
		mNodes[node].mLeft = operand;
		out = node;
		return true;
	}

	bool QueryParser::ParsePrimary(uint32_t depth, uint32_t& out)
	{
		const QueryToken& t = Peek();
		switch (t.mKind)
		{
		case QueryTokenKind::LParen:
		{
			const QueryToken open = t;
			if (depth + 1 > kMaxDepth)
			{
				return Fail("query is nested too deeply (maximum depth 16)", open);
			}
			++mPos;
			if (!ParseOr(depth + 1, out)) { return false; }
			if (Peek().mKind == QueryTokenKind::RParen)
			{
				++mPos;
				return true;
			}
			if (Peek().mKind == QueryTokenKind::End)
			{
				return Fail("unclosed '('", open);
			}
			return Fail("unexpected token", Peek());
		}
		case QueryTokenKind::Identifier:
			return ParseComparison(out);
		case QueryTokenKind::And:
		case QueryTokenKind::Or:
		case QueryTokenKind::RParen:
		case QueryTokenKind::End:
			return Fail("missing operand", t);
		default:
			return Fail("unexpected token", t);
		}
	}

	bool QueryParser::ParseComparison(uint32_t& out)
	{
		const QueryToken name = Peek();
		++mPos;

		const QueryToken opToken = Peek();
		QueryCompareOp op = QueryCompareOp::Lt;
		switch (opToken.mKind)
		{
		case QueryTokenKind::Less: op = QueryCompareOp::Lt; break;
		case QueryTokenKind::LessEqual: op = QueryCompareOp::Le; break;
		case QueryTokenKind::Greater: op = QueryCompareOp::Gt; break;
		case QueryTokenKind::GreaterEqual: op = QueryCompareOp::Ge; break;
		case QueryTokenKind::Equal: op = QueryCompareOp::Eq; break;
		case QueryTokenKind::NotEqual: op = QueryCompareOp::Ne; break;
		default:
			return Fail("missing operator (expected one of < <= > >= = == !=)", opToken);
		}
		++mPos;

		const QueryToken rhs = Peek();
		const bool equalityOp = (op == QueryCompareOp::Eq || op == QueryCompareOp::Ne);
		double value = 0.0;
		QueryRhsKind rhsKind = QueryRhsKind::Number;
		if (rhs.mKind == QueryTokenKind::Number)
		{
			char buffer[QueryLexer::kMaxInput + 1];
			const uint32_t len = rhs.mLength < QueryLexer::kMaxInput ? rhs.mLength : QueryLexer::kMaxInput;
			memcpy(buffer, mText + rhs.mColumn, len);
			buffer[len] = '\0';
			value = strtod(buffer, nullptr);
		}
		else if (rhs.mKind == QueryTokenKind::Identifier)
		{
			if (!equalityOp)
			{
				return Fail("expected a number", rhs);
			}
			rhsKind = QueryRhsKind::EnumId;
		}
		else if (rhs.mKind == QueryTokenKind::End || rhs.mKind == QueryTokenKind::And
			|| rhs.mKind == QueryTokenKind::Or || rhs.mKind == QueryTokenKind::RParen)
		{
			return Fail("missing operand (expected a value after the operator)", rhs);
		}
		else
		{
			return Fail(equalityOp ? "expected a number or identifier" : "expected a number", rhs);
		}
		++mPos;

		uint32_t node;
		if (!NewNode(QueryNodeKind::Compare, node)) { return false; }
		QueryNode& n = mNodes[node];
		n.mOp = op;
		n.mRhsKind = rhsKind;
		n.mValue = value;
		n.mName = { name.mColumn, name.mLength };
		n.mOpSpan = { opToken.mColumn, opToken.mLength };
		n.mRhs = { rhs.mColumn, rhs.mLength };
		out = node;
		return true;
	}
}
