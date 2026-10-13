#include <DiaVisualDebugger/Console/ConsoleJson.h>

#include <cstdio>

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
		}
	}
}
