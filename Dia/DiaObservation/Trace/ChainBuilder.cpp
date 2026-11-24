#include "DiaObservation/Trace/ChainBuilder.h"

#include <DiaObservation/JsonEscape.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		namespace
		{
			std::string Escaped(const char* text)
			{
				char buf[256];
				EscapeJsonString(text ? text : "", buf, sizeof(buf));
				return std::string(buf);
			}

			std::string Unavailable(const char* reason)
			{
				return std::string("{\"available\":false,\"reason\":\"") + Escaped(reason) + "\"}";
			}

			const ChainSpan* FindBySpanId(const ChainSpan* spans, uint32_t count, uint64_t traceId, uint64_t spanId)
			{
				for (uint32_t i = count; i > 0; --i)
				{
					const ChainSpan& s = spans[i - 1];
					if (s.spanId == spanId && s.traceId == traceId)
						return &s;
				}
				return nullptr;
			}

			bool NameIs(const ChainSpan& s, const char* name)
			{
				return name != nullptr && std::strncmp(s.name, name, ChainSpan::kNameLength - 1) == 0;
			}

			struct Edge { const ChainSpan* from; const ChainSpan* to; };

			std::string StageJson(const ChainSpan& s, const ThreadNameFn& threadName)
			{
				char unit[64] = {};
				if (!threadName || !threadName(s.threadId, unit, sizeof(unit)) || unit[0] == '\0')
					std::snprintf(unit, sizeof(unit), "thread %u", s.threadId);
				char buf[512];
				std::snprintf(buf, sizeof(buf),
					"{\"id\":\"%llu\",\"unit\":\"%s\",\"name\":\"%s\",\"startMs\":%.3f,\"endMs\":%.3f}",
					static_cast<unsigned long long>(s.spanId), Escaped(unit).c_str(), Escaped(s.name).c_str(),
					static_cast<double>(s.startSteadyNs) / 1.0e6, static_cast<double>(s.endSteadyNs) / 1.0e6);
				return std::string(buf);
			}
		}

		bool BuildChainJson(const ChainSpan* spans, uint32_t count, const ChainConfig& cfg,
		                    const ThreadNameFn& threadName, const EdgeStatsFn& edgeStats, std::string& outJson)
		{
			if (spans == nullptr || count == 0)
			{
				outJson = Unavailable("no spans recorded (is tracing on?)");
				return false;
			}

			static const uint32_t kMaxEndCandidates = 8;
			static const uint32_t kMaxHops = 8;

			// Newest end spans first; take the first whose writer is still in the history.
			bool sawEnd = false;
			std::vector<const ChainSpan*> chain;   // end span first, then its writer, then that writer's writer
			uint32_t tried = 0;
			for (uint32_t i = count; i > 0 && tried < kMaxEndCandidates; --i)
			{
				const ChainSpan& e = spans[i - 1];
				if (!NameIs(e, cfg.endZone) || e.linkSpanId == 0)
					continue;
				sawEnd = true;
				++tried;

				std::vector<const ChainSpan*> candidate;
				candidate.push_back(&e);
				const ChainSpan* cur = &e;
				for (uint32_t hop = 0; hop < kMaxHops && cur->linkSpanId != 0; ++hop)
				{
					const ChainSpan* writer = FindBySpanId(spans, count, cur->linkTraceId, cur->linkSpanId);
					if (writer == nullptr)
						break;
					candidate.push_back(writer);
					cur = writer;
				}
				if (candidate.size() > 1)
				{
					chain.swap(candidate);
					break;
				}
			}

			if (chain.empty())
			{
				outJson = Unavailable(sawEnd ? "linked writer span not in recent history"
				                             : "no linked chain recorded yet");
				return false;
			}

			std::vector<const ChainSpan*> stages;
			std::vector<Edge> edges;
			for (size_t c = 0; c < chain.size(); ++c)
			{
				const ChainSpan* span = chain[c];
				stages.push_back(span);
				if (c + 1 < chain.size())
					edges.push_back({ chain[c + 1], span });

				// Writer side: its direct children, longest per name.
				if (c > 0)
				{
					std::vector<const ChainSpan*> kids;
					for (uint32_t i = 0; i < count; ++i)
					{
						const ChainSpan& k = spans[i];
						if (k.traceId != span->traceId || k.parentSpanId != span->spanId)
							continue;
						bool merged = false;
						for (auto& existing : kids)
						{
							if (existing->nameCrc == k.nameCrc)
							{
								if (k.endSteadyNs - k.startSteadyNs > existing->endSteadyNs - existing->startSteadyNs)
									existing = &k;
								merged = true;
								break;
							}
						}
						if (!merged)
							kids.push_back(&k);
					}
					stages.insert(stages.end(), kids.begin(), kids.end());
				}
			}

			// Reading side tail: the first tail zone after the end span in the same trace.
			if (cfg.tailZone != nullptr && cfg.tailZone[0] != '\0')
			{
				const ChainSpan* end = chain[0];
				const ChainSpan* tail = nullptr;
				for (uint32_t i = 0; i < count; ++i)
				{
					const ChainSpan& t = spans[i];
					if (t.traceId != end->traceId || !NameIs(t, cfg.tailZone) || t.startSteadyNs < end->endSteadyNs)
						continue;
					if (tail == nullptr || t.startSteadyNs < tail->startSteadyNs)
						tail = &t;
				}
				if (tail != nullptr)
					stages.push_back(tail);
			}

			std::stable_sort(stages.begin(), stages.end(),
				[](const ChainSpan* a, const ChainSpan* b) { return a->startSteadyNs < b->startSteadyNs; });

			std::string json = "{\"available\":true,\"name\":\"" + Escaped(cfg.chainName) + "\",\"stages\":[";
			for (size_t i = 0; i < stages.size(); ++i)
			{
				if (i > 0) json += ",";
				json += StageJson(*stages[i], threadName);
			}
			json += "],\"edges\":[";

			ChainEdgeStats stats;
			const bool haveStats = edgeStats && cfg.edgeStream != nullptr && edgeStats(cfg.edgeStream, stats);
			for (size_t i = 0; i < edges.size(); ++i)
			{
				if (i > 0) json += ",";
				char head[256];
				std::snprintf(head, sizeof(head), "{\"stream\":\"%s\",\"fromId\":\"%llu\",\"toId\":\"%llu\"",
					Escaped(cfg.edgeStream).c_str(),
					static_cast<unsigned long long>(edges[i].from->spanId),
					static_cast<unsigned long long>(edges[i].to->spanId));
				json += head;
				char field[96];
				if (haveStats && stats.hasBytes)     { std::snprintf(field, sizeof(field), ",\"bytes\":%.1f", stats.bytesPerWrite);    json += field; }
				if (haveStats && stats.hasRate)      { std::snprintf(field, sizeof(field), ",\"ratePerSec\":%.2f", stats.ratePerSec);  json += field; }
				if (haveStats && stats.hasDataAge)   { std::snprintf(field, sizeof(field), ",\"dataAgeMs\":%.3f", stats.dataAgeMs);    json += field; }
				if (haveStats && stats.hasWriteCost) { std::snprintf(field, sizeof(field), ",\"writeCostMs\":%.4f", stats.writeCostMs); json += field; }
				json += "}";
			}
			json += "]}";

			outJson = json;
			return true;
		}
	}
} // namespace Observation
} // namespace Dia
