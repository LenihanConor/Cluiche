#include "DiaObservation/Profile/PerfSummary.h"
#include "DiaObservation/Profile/UnitTimingTable.h"

#include "DiaCore/Json/external/json/json.h"

#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		bool PerfSummaryAccumulator::Upsert(const PerfModuleRow& row)
		{
			for (uint32_t i = 0; i < mCount; ++i)
			{
				if (mRows[i].crc == row.crc)
				{
					mRows[i] = row;
					return true;
				}
			}
			if (mCount >= kCapacity)
			{
				++mDropped;
				return false;
			}
			mRows[mCount++] = row;
			return true;
		}

		void BuildPerfModuleRow(PerfModuleRow& out, const UnitTimingTable& table, uint32_t slot, const char* name, uint32_t crc)
		{
			std::memset(&out, 0, sizeof(out));
			strncpy_s(out.name, sizeof(out.name), name ? name : "", _TRUNCATE);
			out.crc = crc;
			if (slot >= UnitTimingTable::kMaxModules || table.GetSampleCount(slot) == 0)
				return;
			out.entryCount = table.GetSampleCount(slot);
			out.p50Ms      = table.GetP50(slot);
			out.p95Ms      = table.GetP95(slot);
			out.maxMs      = table.GetMax(slot);
			out.ewmaMs     = table.GetEwma(slot);
		}

		std::string SerializePerfSummary(const PerfSummary& s)
		{
			Json::Value root(Json::objectValue);
			root["record_type"]  = "perf_summary";
			root["build_config"] = s.buildConfig;

			Json::Value units(Json::arrayValue);
			for (const PerfUnitSummary& u : s.units)
			{
				Json::Value o(Json::objectValue);
				o["unit"]               = u.unit;
				o["budget_ms"]          = Json::Value(static_cast<double>(u.budgetMs));
				o["entry_count"]        = Json::Value(static_cast<Json::UInt>(u.entryCount));
				o["unit_p50_ms"]        = Json::Value(static_cast<double>(u.unitP50Ms));
				o["unit_p95_ms"]        = Json::Value(static_cast<double>(u.unitP95Ms));
				o["unit_max_ms"]        = Json::Value(static_cast<double>(u.unitMaxMs));
				o["unit_ewma_ms"]       = Json::Value(static_cast<double>(u.unitEwmaMs));
				o["trip_count"]         = Json::Value(static_cast<Json::UInt>(u.tripCount));
				o["suppressed_count"]   = Json::Value(static_cast<Json::UInt>(u.suppressedCount));
				o["over_budget_frames"] = Json::Value(static_cast<Json::UInt>(u.overBudgetFrames));
				o["modules_dropped"]    = Json::Value(static_cast<Json::UInt>(u.modules.GetDropped()));

				Json::Value mods(Json::arrayValue);
				for (uint32_t i = 0; i < u.modules.GetCount(); ++i)
				{
					const PerfModuleRow& r = u.modules.GetRow(i);
					Json::Value m(Json::objectValue);
					m["module"]      = r.name;
					m["entry_count"] = Json::Value(static_cast<Json::UInt>(r.entryCount));
					m["p50_ms"]      = Json::Value(static_cast<double>(r.p50Ms));
					m["p95_ms"]      = Json::Value(static_cast<double>(r.p95Ms));
					m["max_ms"]      = Json::Value(static_cast<double>(r.maxMs));
					m["ewma_ms"]     = Json::Value(static_cast<double>(r.ewmaMs));
					mods.append(m);
				}
				o["modules"] = mods;
				units.append(o);
			}
			root["units"] = units;

			Json::StreamWriterBuilder builder;
			builder["indentation"]  = "  ";
			builder["commentStyle"] = "None";
			builder["precision"]    = 6;
			return Json::writeString(builder, root);
		}

		bool WritePerfSummary(const PerfSummary& summary, const char* sessionDir)
		{
			if (sessionDir == nullptr || sessionDir[0] == '\0')
				return false;

			std::string path = sessionDir;
			const char last = path.back();
			if (last != '/' && last != '\\')
				path += '/';
			path += "perf-summary.json";

			const std::string text = SerializePerfSummary(summary);
			FILE* f = nullptr;
			if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr)
				return false;
			const size_t written = std::fwrite(text.data(), 1, text.size(), f);
			std::fclose(f);
			return written == text.size();
		}

		bool WritePerfSummaryIfEnabled(bool perfEnabled, const PerfSummary& summary, const char* sessionDir)
		{
			if (!perfEnabled)
				return false;
			return WritePerfSummary(summary, sessionDir);
		}
	}}
}
