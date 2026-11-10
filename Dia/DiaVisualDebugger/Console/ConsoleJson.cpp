#include <DiaVisualDebugger/Console/ConsoleJson.h>

#include <DiaVisualDebugger/Console/FuzzyMatcher.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				// Bounded append-only writer. Once an append fails the writer stays failed.
				class Writer
				{
				public:
					Writer(char* buf, unsigned size) : mBuf(buf), mSize(size), mLen(0), mOk(buf != nullptr && size > 0)
					{
						if (mOk) mBuf[0] = '\0';
					}

					bool Ok() const { return mOk; }
					unsigned Length() const { return mLen; }

					// Roll back to a previous length and clear the failed state.
					void Rewind(unsigned len)
					{
						mLen = len;
						mOk = (mBuf != nullptr && mSize > 0);
						if (mOk) mBuf[mLen] = '\0';
					}

					void Raw(const char* s)
					{
						while (mOk && *s) Put(*s++);
					}

					void Int(long long v)
					{
						char tmp[32];
						snprintf(tmp, sizeof(tmp), "%lld", v);
						Raw(tmp);
					}

					void Float(double v)
					{
						char tmp[48];
						snprintf(tmp, sizeof(tmp), "%g", v);
						// JSON needs '.' as decimal separator: a decimal-comma process locale must not leak in.
						for (char* c = tmp; *c; ++c)
						{
							if (*c == ',') *c = '.';
						}
						Raw(tmp);
					}

					void Seconds(double v)
					{
						char tmp[48];
						snprintf(tmp, sizeof(tmp), "%.3f", v);
						Raw(tmp);
					}

					void Str(const char* s)
					{
						Put('"');
						if (s != nullptr)
						{
							for (; mOk && *s; ++s)
							{
								const unsigned char c = static_cast<unsigned char>(*s);
								switch (c)
								{
								case '"':  Raw("\\\""); break;
								case '\\': Raw("\\\\"); break;
								case '\b': Raw("\\b"); break;
								case '\f': Raw("\\f"); break;
								case '\n': Raw("\\n"); break;
								case '\r': Raw("\\r"); break;
								case '\t': Raw("\\t"); break;
								default:
									if (c < 0x20)
									{
										char tmp[8];
										snprintf(tmp, sizeof(tmp), "\\u%04x", static_cast<unsigned>(c));
										Raw(tmp);
									}
									else
									{
										Put(static_cast<char>(c));
									}
									break;
								}
							}
						}
						Put('"');
					}

					bool Finish()
					{
						if (!mOk && mBuf != nullptr && mSize > 0) mBuf[0] = '\0';
						return mOk;
					}

				private:
					void Put(char c)
					{
						if (!mOk) return;
						if (mLen + 1 >= mSize) { mOk = false; return; }
						mBuf[mLen++] = c;
						mBuf[mLen] = '\0';
					}

					char* mBuf;
					unsigned mSize;
					unsigned mLen;
					bool mOk;
				};

				const char* LevelName(Dia::Observation::Log::LogLevel level)
				{
					using Dia::Observation::Log::LogLevel;
					switch (level)
					{
					case LogLevel::kWarning: return "WARN";
					case LogLevel::kError:   return "ERROR";
					case LogLevel::kInfo:    return "INFO";
					default:                 return "DEBUG";
					}
				}

				const char* RankName(MatchRank rank)
				{
					switch (rank)
					{
					case MatchRank::Exact:       return "exact";
					case MatchRank::Prefix:      return "prefix";
					case MatchRank::Contains:    return "contains";
					case MatchRank::Subsequence: return "subsequence";
					default:                     return "none";
					}
				}
			}

#ifdef DIA_DEBUG
			namespace
			{
				bool NonEmpty(const char* s) { return s != nullptr && s[0] != '\0'; }

				// ,"args":[{name,type,required,description?,unit?,min?,max?,values?}]
				void WriteArgs(Writer& w, const Dia::API::ValueParam* params, unsigned count)
				{
					w.Raw(",\"args\":[");
					for (unsigned i = 0; i < count; ++i)
					{
						const Dia::API::ValueParam& p = params[i];
						if (i > 0) w.Raw(",");
						w.Raw("{\"name\":");     w.Str(p.name);
						w.Raw(",\"type\":");     w.Str(p.type);
						w.Raw(",\"required\":"); w.Raw(p.required ? "true" : "false");
						if (NonEmpty(p.description)) { w.Raw(",\"description\":"); w.Str(p.description); }
						if (NonEmpty(p.unit))        { w.Raw(",\"unit\":");        w.Str(p.unit); }
						// NaN/inf are not JSON numbers: omit BOTH bounds if either is non-finite.
						if (p.hasRange && std::isfinite(p.min) && std::isfinite(p.max))
						{
							w.Raw(",\"min\":"); w.Float(p.min);
							w.Raw(",\"max\":"); w.Float(p.max);
						}
						if (NonEmpty(p.enumValues))
						{
							// Empty tokens ("a||b", "|a", "a|") are skipped; "values" is omitted if none remain.
							// Tokens longer than 127 bytes are cut (never inside a UTF-8 sequence).
							const char* s = p.enumValues;
							bool opened = false;
							for (;;)
							{
								const char* e = s;
								while (*e != '\0' && *e != '|') ++e;
								char tmp[128];
								size_t n = static_cast<size_t>(e - s);
								if (n >= sizeof(tmp))
								{
									n = sizeof(tmp) - 1;
									while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
								}
								if (n > 0)
								{
									std::memcpy(tmp, s, n);
									tmp[n] = '\0';
									w.Raw(opened ? "," : ",\"values\":[");
									opened = true;
									w.Str(tmp);
								}
								if (*e == '\0') break;
								s = e + 1;
							}
							if (opened) w.Raw("]");
						}
						w.Raw("}");
					}
					w.Raw("]");
				}
			}
#endif

			bool ConsoleJson::BuildConsoleResult(uint32_t seq, int exitCode, unsigned parseError, const char* text,
												 char* out, unsigned outSize)
			{
				Writer w(out, outSize);
				w.Raw("{\"seq\":");        w.Int(seq);
				w.Raw(",\"exitCode\":");   w.Int(exitCode);
				w.Raw(",\"parseError\":"); w.Int(parseError);
				w.Raw(",\"text\":");       w.Str(text);
				w.Raw("}");
				return w.Finish();
			}

			bool ConsoleJson::BuildAppendLogLines(const ConsoleLogLine* lines, unsigned count,
												  char* out, unsigned outSize, unsigned* outLinesWritten)
			{
				if (outLinesWritten != nullptr) *outLinesWritten = 0;

				Writer w(out, outSize);
				w.Raw("{\"lines\":[");

				// Closing "]}" plus the NUL must still fit after each accepted line.
				const unsigned kClose = 2;
				unsigned written = 0;
				for (unsigned i = 0; i < count && w.Ok(); ++i)
				{
					const unsigned before = w.Length();
					if (written > 0) w.Raw(",");
					w.Raw("{\"t\":");       w.Seconds(static_cast<double>(lines[i].timestampNs) / 1e9);
					w.Raw(",\"level\":");   w.Str(LevelName(lines[i].level));
					w.Raw(",\"channel\":"); w.Str(lines[i].channel.AsChar());
					w.Raw(",\"msg\":");     w.Str(lines[i].message);
					w.Raw("}");

					if (!w.Ok() || w.Length() + kClose + 1 > outSize)
					{
						w.Rewind(before);
						break;
					}
					++written;
				}

				w.Raw("]}");
				if (outLinesWritten != nullptr && w.Ok()) *outLinesWritten = written;
				return w.Finish();
			}

			bool ConsoleJson::BuildSetInput(const char* text, bool restore, char* out, unsigned outSize)
			{
				Writer w(out, outSize);
				w.Raw("{\"text\":");    w.Str(text);
				w.Raw(",\"restore\":"); w.Raw(restore ? "true" : "false");
				w.Raw("}");
				return w.Finish();
			}

			bool ConsoleJson::BuildCompletions(const Completion* items, unsigned count, char* out, unsigned outSize)
			{
				Writer w(out, outSize);
				w.Raw("{\"items\":[");
				for (unsigned i = 0; i < count; ++i)
				{
					if (i > 0) w.Raw(",");
					w.Raw("{\"name\":"); w.Str(items[i].name);
					w.Raw(",\"rank\":"); w.Str(RankName(items[i].rank));
					w.Raw("}");
				}
				w.Raw("]}");
				return w.Finish();
			}

#ifdef DIA_DEBUG
				bool ConsoleJson::BuildPaletteResults(const PaletteIndex& index, const char* query, int typeFilterMask,
													  char* out, unsigned outSize)
				{
					const char* q = (query != nullptr) ? query : "";
					Dia::Core::Containers::DynamicArrayC<int, PaletteIndex::kMaxResults> results;
					const int countAll = index.Search(q, PaletteIndex::kTypeCommands | PaletteIndex::kTypeDomains, results);
					const int countCmd = index.Search(q, PaletteIndex::kTypeCommands, results);
					const int countDom = index.Search(q, PaletteIndex::kTypeDomains, results);
					const int total = index.Search(q, typeFilterMask, results);   // leaves the filtered results in 'results'

					Writer w(out, outSize);
					w.Raw("{\"total\":");           w.Int(total);
					w.Raw(",\"counts\":{\"all\":");  w.Int(countAll);
					w.Raw(",\"commands\":");        w.Int(countCmd);
					w.Raw(",\"domains\":");         w.Int(countDom);
					w.Raw("},\"items\":[");
					if (!w.Ok()) return w.Finish();

					const unsigned kClose = 2;   // "]}"
					unsigned written = 0;
					for (unsigned i = 0; i < results.Size() && w.Ok(); ++i)
					{
						const PaletteItem& item = index.GetItem(results[i]);
						const PaletteAction action = ResolvePaletteAction(item);

						unsigned char mask[sizeof(item.name)];
						std::memset(mask, 0, sizeof(mask));
						if (q[0] != '\0')
							FuzzyMatcher::Match(q, item.name, mask, sizeof(mask));
						char hl[sizeof(item.name) + 1];
						size_t nameLen = std::strlen(item.name);
						if (nameLen > sizeof(mask)) nameLen = sizeof(mask);
						for (size_t k = 0; k < nameLen; ++k)
							hl[k] = mask[k] ? '1' : '0';
						hl[nameLen] = '\0';

						const unsigned before = w.Length();
						if (written > 0) w.Raw(",");
						const bool isCmd = item.type == PaletteItemType::Command;
						w.Raw("{\"type\":");        w.Str(isCmd ? "command" : "domain");
						w.Raw(",\"name\":");        w.Str(item.name);
						w.Raw(",\"description\":"); w.Str(item.description);
						w.Raw(",\"category\":");    w.Str(item.category);
						w.Raw(",\"example\":");     w.Str(item.example);
						w.Raw(",\"domainId\":");    w.Str(item.domainId);
						w.Raw(",\"action\":");      w.Str(action.kind == PaletteAction::FillConsole ? "fill" : "reveal");
						w.Raw(",\"actionText\":");  w.Str(action.text);
						w.Raw(",\"hl\":");          w.Str(hl);
						if (isCmd && item.params != nullptr && item.paramCount > 0)
							WriteArgs(w, item.params, item.paramCount);
						w.Raw("}");

						if (!w.Ok() || w.Length() + kClose + 1 > outSize)
						{
							w.Rewind(before);
							break;
						}
						++written;
					}

					w.Raw("]}");
					return w.Finish();
				}
#endif
		}
	}
}
