#include <DiaCellLayers/QueryLexer.h>

#include <string.h>

namespace Dia::CellLayers
{
	namespace
	{
		bool IsDigit(char c) { return c >= '0' && c <= '9'; }
		bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
		bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
		bool IsNumberJunk(char c) { return IsAlpha(c) || IsDigit(c) || c == '.' || c == '%'; }

		void CopyText(char* dst, uint32_t size, const char* src)
		{
			strncpy_s(dst, size, src, _TRUNCATE);
		}
	}

	bool QueryLexer::Fail(const char* message, uint32_t column, uint32_t length, const char* suggestion)
	{
		mCount = 0;
		mError = QueryError();
		CopyText(mError.mMessage, QueryError::kMessageSize, message);
		mError.mColumn = column;
		mError.mLength = length;
		if (suggestion != nullptr)
		{
			CopyText(mError.mSuggestion, QueryError::kSuggestionSize, suggestion);
		}
		return false;
	}

	void QueryLexer::Push(QueryTokenKind kind, uint32_t column, uint32_t length)
	{
		QueryToken& t = mTokens[mCount++];
		t.mKind = kind;
		t.mColumn = column;
		t.mLength = length;
	}

	bool QueryLexer::Tokenize(const char* text)
	{
		mCount = 0;
		mError = QueryError();
		if (text == nullptr)
		{
			text = "";
		}

		const uint32_t len = static_cast<uint32_t>(strlen(text));
		if (len > kMaxInput)
		{
			return Fail("query is too long (maximum 512 characters)", kMaxInput, len - kMaxInput);
		}

		uint32_t i = 0;
		while (i < len)
		{
			const char c = text[i];
			if (IsSpace(c))
			{
				++i;
				continue;
			}

			if (IsAlpha(c))
			{
				const uint32_t start = i;
				while (i < len && (IsAlpha(text[i]) || IsDigit(text[i]) || text[i] == '.'))
				{
					++i;
				}
				const uint32_t n = i - start;
				QueryTokenKind kind = QueryTokenKind::Identifier;
				if (n == 3 && strncmp(text + start, "and", 3) == 0) { kind = QueryTokenKind::And; }
				else if (n == 2 && strncmp(text + start, "or", 2) == 0) { kind = QueryTokenKind::Or; }
				else if (n == 3 && strncmp(text + start, "not", 3) == 0) { kind = QueryTokenKind::Not; }
				Push(kind, start, n);
				continue;
			}

			if (IsDigit(c) || c == '+' || c == '-' || c == '.')
			{
				const uint32_t start = i;
				bool good = true;
				if (c == '+' || c == '-')
				{
					++i;
					if (i >= len || !IsDigit(text[i]))
					{
						good = false;
					}
				}
				else if (c == '.')
				{
					good = false;
				}
				if (good)
				{
					while (i < len && IsDigit(text[i])) { ++i; }
					if (i < len && text[i] == '.')
					{
						if (i + 1 < len && IsDigit(text[i + 1]))
						{
							i += 2;
							while (i < len && IsDigit(text[i])) { ++i; }
						}
						else
						{
							good = false;
						}
					}
					if (good && i < len && IsNumberJunk(text[i]))
					{
						good = false;
					}
				}
				if (!good)
				{
					while (i < len && IsNumberJunk(text[i])) { ++i; }
					if (i == start) { ++i; }
					return Fail("malformed number (expected digits with optional fraction, e.g. 12 or -0.5)",
						start, i - start);
				}
				Push(QueryTokenKind::Number, start, i - start);
				continue;
			}

			const char next = (i + 1 < len) ? text[i + 1] : '\0';
			switch (c)
			{
			case '(': Push(QueryTokenKind::LParen, i, 1); ++i; break;
			case ')': Push(QueryTokenKind::RParen, i, 1); ++i; break;
			case '<':
				if (next == '=') { Push(QueryTokenKind::LessEqual, i, 2); i += 2; }
				else { Push(QueryTokenKind::Less, i, 1); ++i; }
				break;
			case '>':
				if (next == '=') { Push(QueryTokenKind::GreaterEqual, i, 2); i += 2; }
				else { Push(QueryTokenKind::Greater, i, 1); ++i; }
				break;
			case '=':
				if (next == '=') { Push(QueryTokenKind::Equal, i, 2); i += 2; }
				else { Push(QueryTokenKind::Equal, i, 1); ++i; }
				break;
			case '!':
				if (next == '=') { Push(QueryTokenKind::NotEqual, i, 2); i += 2; }
				else { return Fail("unexpected '!' (did you mean '!=' or 'not'?)", i, 1, "!="); }
				break;
			default:
				return Fail("unexpected character", i, 1);
			}
		}

		Push(QueryTokenKind::End, len, 0);
		return true;
	}
}
