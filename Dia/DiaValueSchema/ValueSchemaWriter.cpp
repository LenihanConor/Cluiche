#include <DiaValueSchema/ValueSchemaWriter.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Token-level scan of a dia.value_schema text. Only `entries[*].default` (and the members of an object default) are
// located; every other value is skipped without interpretation. Replacements are spliced into a copy of the input.

namespace Dia::ValueSchema
{
	namespace
	{
		struct Span { size_t mStart = 0; size_t mEnd = 0; };

		struct MemberSpan
		{
			std::string	mName;
			Span		mValue;
		};

		struct EntryScan
		{
			std::string					mKey;
			bool						mHasKey = false;
			bool						mHasDefault = false;
			bool						mDefaultIsObject = false;
			Span						mDefault;
			std::vector<MemberSpan>		mMembers;	// when mDefaultIsObject
		};

		class Scanner
		{
		public:
			Scanner(const char* s, size_t n) : mS(s), mN(n) {}

			std::vector<EntryScan>	mEntries;
			std::string				mError;

			bool ParseDocument()
			{
				size_t p = 0;
				if (!SkipWs(p)) { return false; }
				if (p >= mN || mS[p] != '{') { return Fail("document is not an object", p); }
				++p;
				bool first = true;
				for (;;)
				{
					if (!SkipWs(p)) { return false; }
					if (p < mN && mS[p] == '}') { return true; }
					if (!first)
					{
						if (p >= mN || mS[p] != ',') { return Fail("expected ',' or '}'", p); }
						++p;
						if (!SkipWs(p)) { return false; }
					}
					first = false;
					std::string name;
					if (!ParseString(p, &name)) { return false; }
					if (!SkipWs(p)) { return false; }
					if (p >= mN || mS[p] != ':') { return Fail("expected ':'", p); }
					++p;
					if (!SkipWs(p)) { return false; }
					if (name == "entries" && p < mN && mS[p] == '[')
					{
						if (!ParseEntries(p)) { return false; }
					}
					else if (!SkipValue(p)) { return false; }
				}
			}

			static std::string DecodeString(const char* s, Span sp)
			{
				Scanner sc(s, sp.mEnd);
				size_t p = sp.mStart;
				std::string out;
				sc.ParseString(p, &out);
				return out;
			}

		private:
			const char*	mS;
			size_t		mN;

			bool Fail(const char* what, size_t at)
			{
				if (mError.empty()) { mError = std::string(what) + " at offset " + std::to_string(at); }
				return false;
			}

			// Skips whitespace and // and /* */ comments (jsoncpp accepts them).
			bool SkipWs(size_t& p)
			{
				for (;;)
				{
					while (p < mN && (mS[p] == ' ' || mS[p] == '\t' || mS[p] == '\r' || mS[p] == '\n')) { ++p; }
					if (p + 1 < mN && mS[p] == '/' && mS[p + 1] == '/')
					{
						while (p < mN && mS[p] != '\n') { ++p; }
						continue;
					}
					if (p + 1 < mN && mS[p] == '/' && mS[p + 1] == '*')
					{
						p += 2;
						while (p + 1 < mN && !(mS[p] == '*' && mS[p + 1] == '/')) { ++p; }
						if (p + 1 >= mN) { return Fail("unterminated comment", p); }
						p += 2;
						continue;
					}
					return true;
				}
			}

			static void AppendUtf8(std::string& out, unsigned cp)
			{
				if (cp < 0x80) { out += static_cast<char>(cp); }
				else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
				else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
				else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
			}

			bool Hex4(size_t p, unsigned& v)
			{
				if (p + 4 > mN) { return Fail("bad \\u escape", p); }
				v = 0;
				for (size_t i = 0; i < 4; ++i)
				{
					const char c = mS[p + i];
					v <<= 4;
					if (c >= '0' && c <= '9') { v |= static_cast<unsigned>(c - '0'); }
					else if (c >= 'a' && c <= 'f') { v |= static_cast<unsigned>(c - 'a' + 10); }
					else if (c >= 'A' && c <= 'F') { v |= static_cast<unsigned>(c - 'A' + 10); }
					else { return Fail("bad \\u escape", p); }
				}
				return true;
			}

			bool ParseString(size_t& p, std::string* decoded)
			{
				if (p >= mN || mS[p] != '"') { return Fail("expected string", p); }
				++p;
				while (p < mN && mS[p] != '"')
				{
					if (mS[p] != '\\')
					{
						if (decoded != nullptr) { *decoded += mS[p]; }
						++p;
						continue;
					}
					if (p + 1 >= mN) { return Fail("unterminated string", p); }
					const char e = mS[p + 1];
					p += 2;
					char c = 0;
					switch (e)
					{
					case '"': c = '"'; break;
					case '\\': c = '\\'; break;
					case '/': c = '/'; break;
					case 'b': c = '\b'; break;
					case 'f': c = '\f'; break;
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 't': c = '\t'; break;
					case 'u':
					{
						unsigned cp = 0;
						if (!Hex4(p, cp)) { return false; }
						p += 4;
						if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= mN && mS[p] == '\\' && mS[p + 1] == 'u')
						{
							unsigned lo = 0;
							if (Hex4(p + 2, lo) && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
						}
						if (decoded != nullptr) { AppendUtf8(*decoded, cp); }
						continue;
					}
					default: return Fail("bad escape", p);
					}
					if (decoded != nullptr) { *decoded += c; }
				}
				if (p >= mN) { return Fail("unterminated string", p); }
				++p;	// closing quote
				return true;
			}

			bool SkipValue(size_t& p)
			{
				if (!SkipWs(p)) { return false; }
				if (p >= mN) { return Fail("unexpected end of text", p); }
				const char c = mS[p];
				if (c == '"') { return ParseString(p, nullptr); }
				if (c == '{' || c == '[')
				{
					const char close = (c == '{') ? '}' : ']';
					++p;
					bool first = true;
					for (;;)
					{
						if (!SkipWs(p)) { return false; }
						if (p >= mN) { return Fail("unterminated container", p); }
						if (mS[p] == close) { ++p; return true; }
						if (!first)
						{
							if (mS[p] != ',') { return Fail("expected ','", p); }
							++p;
							if (!SkipWs(p)) { return false; }
						}
						first = false;
						if (c == '{')
						{
							if (!ParseString(p, nullptr)) { return false; }
							if (!SkipWs(p)) { return false; }
							if (p >= mN || mS[p] != ':') { return Fail("expected ':'", p); }
							++p;
						}
						if (!SkipValue(p)) { return false; }
					}
				}
				// number or literal
				const size_t start = p;
				while (p < mN && (isalnum(static_cast<unsigned char>(mS[p])) || mS[p] == '-' || mS[p] == '+' || mS[p] == '.')) { ++p; }
				if (p == start) { return Fail("unexpected character", p); }
				return true;
			}

			bool ParseEntries(size_t& p)
			{
				++p;	// '['
				bool first = true;
				for (;;)
				{
					if (!SkipWs(p)) { return false; }
					if (p >= mN) { return Fail("unterminated entries array", p); }
					if (mS[p] == ']') { ++p; return true; }
					if (!first)
					{
						if (mS[p] != ',') { return Fail("expected ','", p); }
						++p;
						if (!SkipWs(p)) { return false; }
					}
					first = false;
					if (p < mN && mS[p] == '{')
					{
						if (!ParseEntry(p)) { return false; }
					}
					else if (!SkipValue(p)) { return false; }
				}
			}

			bool ParseEntry(size_t& p)
			{
				EntryScan e;
				++p;	// '{'
				bool first = true;
				for (;;)
				{
					if (!SkipWs(p)) { return false; }
					if (p >= mN) { return Fail("unterminated entry", p); }
					if (mS[p] == '}') { ++p; break; }
					if (!first)
					{
						if (mS[p] != ',') { return Fail("expected ','", p); }
						++p;
						if (!SkipWs(p)) { return false; }
					}
					first = false;
					std::string name;
					if (!ParseString(p, &name)) { return false; }
					if (!SkipWs(p)) { return false; }
					if (p >= mN || mS[p] != ':') { return Fail("expected ':'", p); }
					++p;
					if (!SkipWs(p)) { return false; }
					if (name == "key" && p < mN && mS[p] == '"' && !e.mHasKey)
					{
						if (!ParseString(p, &e.mKey)) { return false; }
						e.mHasKey = true;
					}
					else if (name == "default" && !e.mHasDefault)
					{
						e.mHasDefault = true;
						e.mDefault.mStart = p;
						if (p < mN && mS[p] == '{')
						{
							e.mDefaultIsObject = true;
							if (!ParseObjectMembers(p, e.mMembers)) { return false; }
						}
						else if (!SkipValue(p)) { return false; }
						e.mDefault.mEnd = p;
					}
					else if (!SkipValue(p)) { return false; }
				}
				mEntries.push_back(std::move(e));
				return true;
			}

			bool ParseObjectMembers(size_t& p, std::vector<MemberSpan>& members)
			{
				++p;	// '{'
				bool first = true;
				for (;;)
				{
					if (!SkipWs(p)) { return false; }
					if (p >= mN) { return Fail("unterminated object", p); }
					if (mS[p] == '}') { ++p; return true; }
					if (!first)
					{
						if (mS[p] != ',') { return Fail("expected ','", p); }
						++p;
						if (!SkipWs(p)) { return false; }
					}
					first = false;
					MemberSpan m;
					if (!ParseString(p, &m.mName)) { return false; }
					if (!SkipWs(p)) { return false; }
					if (p >= mN || mS[p] != ':') { return Fail("expected ':'", p); }
					++p;
					if (!SkipWs(p)) { return false; }
					m.mValue.mStart = p;
					if (!SkipValue(p)) { return false; }
					m.mValue.mEnd = p;
					members.push_back(std::move(m));
				}
			}
		};

		std::string ShortestNumber(double v, bool single)
		{
			char buf[40];
			const int maxPrec = single ? 9 : 17;
			for (int prec = 1; prec <= maxPrec; ++prec)
			{
				snprintf(buf, sizeof(buf), "%.*g", prec, v);
				const bool same = single ? (strtof(buf, nullptr) == static_cast<float>(v)) : (strtod(buf, nullptr) == v);
				if (same) { break; }
			}
			std::string s = buf;
			if (s.find_first_of(".eE") == std::string::npos) { s += ".0"; }
			return s;
		}

		std::string EscapeString(const char* s)
		{
			std::string out = "\"";
			for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p != 0; ++p)
			{
				switch (*p)
				{
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				case '\b': out += "\\b"; break;
				case '\f': out += "\\f"; break;
				default:
					if (*p < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", *p); out += b; }
					else { out += static_cast<char>(*p); }
				}
			}
			out += '"';
			return out;
		}

		// True when the existing token already holds the requested value (so its original spelling is kept).
		bool SameValue(const char* text, Span sp, const DefaultValue& v)
		{
			const std::string tok(text + sp.mStart, sp.mEnd - sp.mStart);
			switch (v.mKind)
			{
			case DefaultKind::Bool:
				return tok == (v.mNumber != 0.0 ? "true" : "false");
			case DefaultKind::String:
				return !tok.empty() && tok[0] == '"' && Scanner::DecodeString(text, sp) == std::string(v.mString != nullptr ? v.mString : "");
			default:
				break;
			}
			if (tok.empty() || tok[0] == '"' || tok[0] == '{' || tok[0] == '[' || isalpha(static_cast<unsigned char>(tok[0]))) { return false; }
			char* end = nullptr;
			const double old = strtod(tok.c_str(), &end);
			if (end == tok.c_str() || *end != '\0') { return false; }
			switch (v.mKind)
			{
			case DefaultKind::Float: return memcmp(&old, &v.mNumber, sizeof(double)) == 0;
			case DefaultKind::Float32:
			{
				const float a = static_cast<float>(old);
				const float b = static_cast<float>(v.mNumber);
				return memcmp(&a, &b, sizeof(float)) == 0;
			}
			case DefaultKind::Int: return old == v.mNumber;
			default: return false;
			}
		}

		bool FormatValue(const DefaultValue& v, std::string& out)
		{
			switch (v.mKind)
			{
			case DefaultKind::Float:
			case DefaultKind::Float32:
				if (!std::isfinite(v.mNumber)) { return false; }
				out = ShortestNumber(v.mKind == DefaultKind::Float32 ? static_cast<double>(static_cast<float>(v.mNumber)) : v.mNumber,
					v.mKind == DefaultKind::Float32);
				return true;
			case DefaultKind::Int:
			{
				if (!std::isfinite(v.mNumber)) { return false; }
				char b[32];
				snprintf(b, sizeof(b), "%lld", static_cast<long long>(v.mNumber));
				out = b;
				return true;
			}
			case DefaultKind::Bool: out = (v.mNumber != 0.0) ? "true" : "false"; return true;
			case DefaultKind::String: out = EscapeString(v.mString != nullptr ? v.mString : ""); return true;
			}
			return false;
		}

		struct Replacement { Span mSpan; std::string mText; };

		bool Failure(WriteError& err, ValueList<char>& out, const std::string& key, const std::string& reason)
		{
			out.RemoveAll();
			SetText(err.mKey, key.c_str());
			SetText(err.mReason, reason.c_str());
			return false;
		}
	}

	bool WriteDefaultsInPlace(const char* sidecarText, const DefaultValue* values, uint32_t count,
		ValueList<char>& out, WriteError& err)
	{
		out.RemoveAll();
		SetText(err.mKey, "");
		SetText(err.mReason, "");
		if (sidecarText == nullptr) { return Failure(err, out, "", "sidecar text is null"); }
		if (count > 0 && values == nullptr) { return Failure(err, out, "", "values is null"); }

		const size_t n = strlen(sidecarText);
		Scanner scan(sidecarText, n);
		if (!scan.ParseDocument()) { return Failure(err, out, "", "sidecar text is not parseable: " + scan.mError); }

		std::vector<Replacement> reps;
		std::vector<std::string> seen;
		for (uint32_t i = 0; i < count; ++i)
		{
			const DefaultValue& v = values[i];
			const std::string key = v.mKey != nullptr ? v.mKey : "";
			const std::string name = v.mMapKey != nullptr ? key + "." + v.mMapKey : key;
			if (std::find(seen.begin(), seen.end(), name) != seen.end()) { return Failure(err, out, name, "'" + name + "' is given more than once"); }
			seen.push_back(name);

			const EntryScan* entry = nullptr;
			for (const EntryScan& e : scan.mEntries) { if (e.mHasKey && e.mKey == key) { entry = &e; break; } }
			if (entry == nullptr) { return Failure(err, out, name, "no entry '" + key + "' in the sidecar"); }
			if (!entry->mHasDefault) { return Failure(err, out, name, "entry '" + key + "' has no default to rewrite"); }

			Span target = entry->mDefault;
			if (v.mMapKey != nullptr)
			{
				if (!entry->mDefaultIsObject) { return Failure(err, out, name, "entry '" + key + "' default is not an object, cannot set map key '" + v.mMapKey + "'"); }
				const MemberSpan* member = nullptr;
				for (const MemberSpan& m : entry->mMembers) { if (m.mName == v.mMapKey) { member = &m; break; } }
				if (member == nullptr) { return Failure(err, out, name, "entry '" + key + "' default has no map key '" + v.mMapKey + "'"); }
				target = member->mValue;
			}
			else if (entry->mDefaultIsObject)
			{
				return Failure(err, out, name, "entry '" + key + "' has an object default; give one value per map key");
			}

			if (SameValue(sidecarText, target, v)) { continue; }
			Replacement r;
			r.mSpan = target;
			if (!FormatValue(v, r.mText)) { return Failure(err, out, name, "value for '" + name + "' is not a finite number"); }
			reps.push_back(std::move(r));
		}

		std::sort(reps.begin(), reps.end(), [](const Replacement& a, const Replacement& b) { return a.mSpan.mStart < b.mSpan.mStart; });

		std::string result;
		result.reserve(n + 64);
		size_t at = 0;
		for (const Replacement& r : reps)
		{
			result.append(sidecarText + at, r.mSpan.mStart - at);
			result += r.mText;
			at = r.mSpan.mEnd;
		}
		result.append(sidecarText + at, n - at);

		out.Reserve(static_cast<unsigned int>(result.size() + 1));
		for (char c : result) { out.Add(c); }
		out.Add('\0');
		return true;
	}
}
