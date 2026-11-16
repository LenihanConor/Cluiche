#include "DiaObservation/Profile/TripRecord.h"
#include "DiaObservation/Profile/Profiler.h"

#include "DiaCore/Json/external/json/json.h"

#include <cstring>
#include <string>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		namespace
		{
			void CopyText(char* dst, size_t cap, const char* src)
			{
				if (!src) src = "";
				strncpy_s(dst, cap, src, _TRUNCATE);
			}
		}

		void BuildTripRecord(TripRecord& out, const char* unit, const TripResult& trip, float budgetMs,
		                     const char* buildConfig, const UnitTimingTable& table, TripNameResolver resolver)
		{
			std::memset(&out, 0, sizeof(out));
			if (!resolver) resolver = &Profiler::ResolveSubjectName;

			CopyText(out.unit, sizeof(out.unit), unit);
			CopyText(out.buildConfig, sizeof(out.buildConfig), buildConfig);
			out.firstFrame = trip.firstEntryIndex;
			out.lastFrame = trip.lastEntryIndex;
			out.budgetMs = budgetMs;
			out.peakMs = trip.peakMs;
			out.suppressedSinceLastTrip = trip.suppressedSinceLastTrip;

			// Last <= 60 unit ticks.
			out.recentCount = table.CopyRecentUnit(out.recent, TripRecord::kRecent);
			const uint32_t u = out.recentCount;

			// Window length for the mean.
			uint32_t window = (trip.lastEntryIndex >= trip.firstEntryIndex)
				? (trip.lastEntryIndex - trip.firstEntryIndex + 1) : 1;
			if (window > UnitTimingTable::kRingSize) window = UnitTimingTable::kRingSize;

			// Worst frame within the recent window.
			if (u > 0)
			{
				uint32_t w = 0;
				for (uint32_t i = 1; i < u; ++i)
					if (out.recent[i] > out.recent[w]) w = i;
				out.worstIndex = w;
				out.worstUnitMs = out.recent[w];
			}

			struct Cand { uint32_t crc; float mean; };
			Cand cands[TripRecord::kMaxModules];
			uint32_t candCount = 0;

			float buf[UnitTimingTable::kRingSize];
			for (uint32_t slot = 0; slot < UnitTimingTable::kMaxModules; ++slot)
			{
				if (table.GetSampleCount(slot) == 0) continue;
				const uint32_t crc = table.GetModuleCrc(slot);

				// Mean over the most recent min(window, available) samples.
				const uint32_t n = table.CopyRecent(slot, buf, UnitTimingTable::kRingSize);
				if (n == 0) continue;
				const uint32_t take = n < window ? n : window;
				double sum = 0.0;
				for (uint32_t i = n - take; i < n; ++i) sum += buf[i];
				cands[candCount].crc = crc;
				cands[candCount].mean = static_cast<float>(sum / take);
				++candCount;

				// Worst-frame breakdown: align newest-last with the unit series.
				if (u > 0)
				{
					const uint32_t m = n < TripRecord::kRecent ? n : TripRecord::kRecent;
					if (m <= u)
					{
						const uint32_t base = u - m;   // unit index of this module's oldest recent sample
						if (out.worstIndex >= base)
						{
							TripRecord::NamedMs& e = out.worst[out.worstCount++];
							resolver(crc, e.name, sizeof(e.name));
							e.ms = buf[n - m + (out.worstIndex - base)];
						}
					}
				}
			}

			// Top 5 by mean, descending (selection; candCount <= 64).
			bool used[TripRecord::kMaxModules] = {};
			for (uint32_t k = 0; k < TripRecord::kTopCount && k < candCount; ++k)
			{
				int best = -1;
				for (uint32_t i = 0; i < candCount; ++i)
					if (!used[i] && (best < 0 || cands[i].mean > cands[best].mean)) best = static_cast<int>(i);
				if (best < 0) break;
				used[best] = true;
				TripRecord::NamedMs& e = out.top[out.topCount++];
				resolver(cands[best].crc, e.name, sizeof(e.name));
				e.ms = cands[best].mean;
			}
			// out.sectionCount stays 0: per-scope data lives on the profiler drain path, not cheaply here.
		}

		void SerializeTripRecord(const TripRecord& r, char* outBuf, size_t outSize, size_t& outLen)
		{
			outLen = 0;
			if (!outBuf || outSize == 0) return;

			Json::Value root(Json::objectValue);
			root["record_type"] = "trip";
			root["unit"] = r.unit;
			root["build_config"] = r.buildConfig;
			root["first_frame"] = Json::Value(static_cast<Json::UInt>(r.firstFrame));
			root["last_frame"] = Json::Value(static_cast<Json::UInt>(r.lastFrame));
			root["budget_ms"] = Json::Value(static_cast<double>(r.budgetMs));
			root["peak_ms"] = Json::Value(static_cast<double>(r.peakMs));
			root["suppressed_since_last_trip"] = Json::Value(static_cast<Json::UInt>(r.suppressedSinceLastTrip));

			auto toArray = [](const TripRecord::NamedMs* e, uint32_t n)
			{
				Json::Value a(Json::arrayValue);
				for (uint32_t i = 0; i < n; ++i)
				{
					Json::Value o(Json::objectValue);
					o["module"] = e[i].name;
					o["ms"] = Json::Value(static_cast<double>(e[i].ms));
					a.append(o);
				}
				return a;
			};
			root["top_modules"] = toArray(r.top, r.topCount);
			root["sections"] = toArray(r.sections, r.sectionCount);

			Json::Value worst(Json::objectValue);
			worst["index"] = Json::Value(static_cast<Json::UInt>(r.worstIndex));
			worst["unit_ms"] = Json::Value(static_cast<double>(r.worstUnitMs));
			worst["modules"] = toArray(r.worst, r.worstCount);
			root["worst_frame"] = worst;

			Json::Value recent(Json::arrayValue);
			for (uint32_t i = 0; i < r.recentCount; ++i)
				recent.append(Json::Value(static_cast<double>(r.recent[i])));
			root["recent_unit_ms"] = recent;

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			builder["commentStyle"] = "None";
			builder["precision"] = 6;
			std::string s = Json::writeString(builder, root);

			// Single line: strip any CR/LF the writer may emit.
			size_t n = 0;
			for (size_t i = 0; i < s.size() && n + 2 < outSize; ++i)
				if (s[i] != '\r' && s[i] != '\n') outBuf[n++] = s[i];
			outBuf[n++] = '\n';
			outBuf[n] = '\0';
			outLen = n;
		}
	}}
}
