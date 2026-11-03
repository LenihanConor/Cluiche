#ifdef DIA_DEBUG

#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaObservation/Log/DiaLog.h>

#include <json/json.h>

#include <cstdio>
#include <string>

namespace Dia::CellHistory
{
	using Dia::API::ValueParam;
	using Dia::Core::StringCRC;

	namespace
	{
		const char* const kTrackName = "cells.history.track";
		const char* const kUntrackName = "cells.history.untrack";
		const char* const kStatusName = "cells.history.status";

		const ValueParam kParamsLayers[] = {
			{ "layers", "string", true, "JSON array of 1 to 16 layer ids on the recorded grid" } };

		Dia::DebugServer::SimHelpInfo Help(const char* description, const ValueParam* params, unsigned int count)
		{
			Dia::DebugServer::SimHelpInfo h;
			h.description = description; h.category = "cells.history"; h.owner = "DiaCellHistory";
			if (params != nullptr) { h.params = params; h.paramCount = count; } else { h.noArgs = true; }
			return h;
		}

		Json::Value Error(const std::string& msg)
		{
			Json::Value out(Json::objectValue);
			out["error"] = msg;
			return out;
		}

		// Parses {layers:[...]}. Returns an empty string on success, else the error message.
		std::string ParseLayers(const Json::Value& args, StringCRC* ids, uint32_t& count)
		{
			count = 0;
			if (!args.isObject() || !args.isMember("layers") || !args["layers"].isArray()) { return "missing or non-array param 'layers'"; }
			const Json::Value& arr = args["layers"];
			if (arr.size() == 0) { return "param 'layers' is empty"; }
			if (arr.size() > CellHistoryCommands::kMaxLayersPerCall) { return "param 'layers' has more than 16 entries"; }
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isString() || arr[i].asString().empty()) { return "param 'layers' must contain only non-empty strings"; }
				ids[count++] = StringCRC(arr[i].asString().c_str());
			}
			return std::string();
		}

		Json::Value GapsJson(const CellHistoryRecorder::StatusGap* gaps, uint32_t count)
		{
			Json::Value arr(Json::arrayValue);
			for (uint32_t i = 0; i < count; ++i)
			{
				Json::Value g(Json::objectValue);
				g["first"] = gaps[i].mFirst;
				g["last"] = gaps[i].mLast;
				arr.append(g);
			}
			return arr;
		}

		Json::Value RangeJson(uint32_t first, uint32_t latest, uint32_t count)
		{
			Json::Value r(Json::objectValue);
			r["first"] = first;
			r["latest"] = latest;
			r["count"] = count;
			return r;
		}

		Json::Value Triple(Json::UInt64 cells, Json::UInt64 aggregates, Json::UInt64 events)
		{
			Json::Value t(Json::objectValue);
			t["cells"] = cells;
			t["aggregates"] = aggregates;
			t["events"] = events;
			return t;
		}
	}

	CellHistoryCommands::CellHistoryCommands(CellHistoryRecorder& recorder) : mRecorder(recorder) {}

	Json::Value CellHistoryCommands::Track(const Json::Value& args)
	{
		StringCRC ids[kMaxLayersPerCall];
		uint32_t n = 0;
		const std::string bad = ParseLayers(args, ids, n);
		if (!bad.empty())
		{
			DIA_LOG_ERROR("HIS", "%s rejected: %s", kTrackName, bad.c_str());
			return Error(bad);
		}
		CellHistoryRecorder::TrackResult res;
		if (!mRecorder.Track(ids, n, res)) { return Error(res.mError); }	// the recorder already logged the Error
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["windowLength"] = res.mWindowLength;
		return out;
	}

	Json::Value CellHistoryCommands::Untrack(const Json::Value& args)
	{
		StringCRC ids[kMaxLayersPerCall];
		uint32_t n = 0;
		const std::string bad = ParseLayers(args, ids, n);
		if (!bad.empty())
		{
			DIA_LOG_ERROR("HIS", "%s rejected: %s", kUntrackName, bad.c_str());
			return Error(bad);
		}
		CellHistoryRecorder::TrackResult res;
		if (!mRecorder.Untrack(ids, n, res)) { return Error(res.mError); }
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["windowLength"] = res.mWindowLength;
		return out;
	}

	Json::Value CellHistoryCommands::Status() const
	{
		CellHistoryRecorder::HistoryStatus s;
		mRecorder.BuildStatus(s);
		Json::Value out(Json::objectValue);
		out["sampleCount"] = s.mSampleCount;
		char hex[24];
		snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(s.mRunId));
		out["runId"] = hex;
		out["recordedLayers"] = s.mRecordedLayers;
		out["skippedLayers"] = s.mSkippedLayers;
		out["versionGatedSkips"] = static_cast<Json::UInt64>(s.mVersionGatedSkips);
		out["windowLength"] = s.mWindowLength;
		out["aggregateWindow"] = s.mAggregateWindow;
		Json::Value tracked(Json::arrayValue);
		for (uint32_t i = 0; i < s.mTrackedCount; ++i) { tracked.append(s.mTracked[i].AsChar()); }
		out["tracked"] = tracked;

		Json::Value retained(Json::objectValue);
		retained["cells"] = RangeJson(s.mCellFirst, s.mCellLatest, s.mCellRetained);
		retained["aggregates"] = RangeJson(s.mAggregateFirst, s.mAggregateLatest, s.mAggregateRetained);
		retained["events"] = RangeJson(s.mEventFirst, s.mEventLatest, s.mEventCount);
		out["retained"] = retained;
		out["memory"] = Triple(s.mCellBytes, s.mAggregateBytes, s.mEventBytes);
		out["budgets"] = Triple(s.mCellBudget, s.mAggregateBudget, s.mEventBudget);

		Json::Value gaps(Json::objectValue);
		gaps["cells"] = GapsJson(s.mCellGaps, s.mCellGapCount);
		gaps["aggregates"] = GapsJson(s.mAggregateGaps, s.mAggregateGapCount);
		gaps["events"] = GapsJson(s.mEventGaps, s.mEventGapCount);
		out["gaps"] = gaps;
		out["gapCap"] = CellHistoryRecorder::HistoryStatus::kMaxGaps;
		return out;
	}

	bool CellHistoryCommands::Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (mRegistered) { return true; }

		struct Def { const char* name; Dia::DebugServer::SimHelpInfo help; Dia::CellLayers::MirrorQueryHandler handler; };
		const Def defs[] = {
			{ kTrackName, Help("Start per-cell recording for layers on the recorded grid. All-or-nothing. Returns {ok, windowLength}.", kParamsLayers, 1u),
				[this](const Json::Value& args) -> Json::Value { return Track(args); } },
			{ kUntrackName, Help("Stop per-cell recording for layers and free their rings. All-or-nothing. Returns {ok, windowLength}.", kParamsLayers, 1u),
				[this](const Json::Value& args) -> Json::Value { return Untrack(args); } },
			{ kStatusName, Help("History status: sample count, retained ranges, memory and budgets by tier, tracked layers and gaps.", nullptr, 0u),
				[this](const Json::Value&) -> Json::Value { return Status(); } } };

		for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); ++i)
		{
			if (registrar.RegisterCommand(defs[i].name, defs[i].help, defs[i].handler)) { continue; }
			DIA_LOG_ERROR("HIS", "history commands: registering %s was rejected, rolling back", defs[i].name);
			for (size_t k = 0; k < i; ++k) { registrar.Unregister(defs[k].name); }
			return false;
		}
		mRegistered = true;
		return true;
	}

	void CellHistoryCommands::Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (!mRegistered) { return; }
		registrar.Unregister(kTrackName);
		registrar.Unregister(kUntrackName);
		registrar.Unregister(kStatusName);
		mRegistered = false;
	}
}

#endif // DIA_DEBUG
