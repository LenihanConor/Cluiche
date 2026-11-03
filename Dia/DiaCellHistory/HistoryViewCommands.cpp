#ifdef DIA_DEBUG

#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryConfig.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaObservation/Log/DiaLog.h>

#include <json/json.h>

#include <cstdint>
#include <string>

namespace Dia::CellHistory
{
	using Dia::API::ValueParam;
	using Dia::Core::StringCRC;

	namespace
	{
		const char* const kViewName = "cells.history.view";
		const char* const kLiveName = "cells.history.live";
		const char* const kStepName = "cells.history.step";
		const char* const kEventName = "cells.history.event";

		const ValueParam kParamsView[] = { { "sample", "int", true, "Recorded sample index, 0 to the latest sample" } };
		const ValueParam kParamsStep[] = {
			{ "n", "int", true, "Samples to move; negative is earlier. Clamped to 0 and the latest sample" },
			{ "band", "string", false, "Band name or index: move to the nearest sample (direction of n) whose band is at least this" } };
		const ValueParam kParamsEvent[] = {
			{ "direction", "string", true, "prev or next" },
			{ "type", "string", false, "Only events of this event type" } };

		Dia::DebugServer::SimHelpInfo Help(const char* description, const ValueParam* params, unsigned int count)
		{
			Dia::DebugServer::SimHelpInfo h;
			h.description = description; h.category = "cells.history"; h.owner = "DiaCellHistory";
			if (params != nullptr) { h.params = params; h.paramCount = count; } else { h.noArgs = true; }
			return h;
		}
	}

	HistoryViewCommands::HistoryViewCommands(const CellHistoryRecorder& recorder, HistoryViewState& view, const HistoryConfig* config)
		: mRecorder(recorder), mView(view), mConfig(config) {}

	namespace
	{
		// Integer from a JSON integer or a decimal string ("-12"). Out-of-int64 values clamp. false on anything else.
		bool ParseInt(const Json::Value& v, int64_t& out)
		{
			if (v.isInt64()) { out = v.asInt64(); return true; }
			if (v.isUInt64()) { out = INT64_MAX; return true; }
			if (!v.isString()) { return false; }
			const std::string s = v.asString();
			size_t i = 0;
			bool neg = false;
			if (i < s.size() && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); ++i; }
			if (i >= s.size()) { return false; }
			int64_t acc = 0;
			for (; i < s.size(); ++i)
			{
				if (s[i] < '0' || s[i] > '9') { return false; }
				const int digit = s[i] - '0';
				acc = (acc > (INT64_MAX - digit) / 10) ? INT64_MAX / 2 : acc * 10 + digit;	// saturate far above any sample count
			}
			out = neg ? -acc : acc;
			return true;
		}

		// Band index from a configured name or an integer (JSON or decimal string). Fills an error message on failure.
		bool ParseBand(const Json::Value& v, const HistoryConfig* config, uint32_t& out, const char*& error)
		{
			if (v.isString() && config != nullptr)
			{
				const std::string name = v.asString();
				for (uint32_t i = 0; i < config->mBandCount && i < HistoryConfig::kMaxBands; ++i)
				{
					if (!name.empty() && name == config->mBands[i].AsCStr()) { out = i; return true; }
				}
			}
			int64_t idx = 0;
			if (!ParseInt(v, idx)) { error = "param 'band' is not a configured band name or a band index"; return false; }
			if (idx < 0 || idx > 255) { error = "param 'band' index is out of range"; return false; }
			if (config != nullptr && config->mBandCount > 0 && idx >= static_cast<int64_t>(config->mBandCount)) { error = "param 'band' index is beyond the configured bands"; return false; }
			out = static_cast<uint32_t>(idx);
			return true;
		}

		struct EventScan
		{
			uint32_t	mType = 0;
			bool		mFilter = false;
			bool		mWantLatest = false;	// prev: the latest matching sample in range; next: the earliest
			bool		mFound = false;
			uint32_t	mBest = 0;
		};

		void VisitEvent(const EventView& e, void* context)
		{
			EventScan& s = *static_cast<EventScan*>(context);
			if (s.mFilter && e.mType.Value() != s.mType) { return; }
			if (!s.mFound) { s.mFound = true; s.mBest = e.mSample; return; }
			if (s.mWantLatest) { if (e.mSample > s.mBest) { s.mBest = e.mSample; } }
			else if (e.mSample < s.mBest) { s.mBest = e.mSample; }
		}
	}

	Json::Value HistoryViewCommands::Result() const
	{
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		const uint32_t count = mRecorder.SampleCount();
		if (mView.IsLive())
		{
			out["mode"] = "live";
			out["sample"] = count == 0 ? -1 : static_cast<Json::Int64>(count - 1);
			out["label"] = count == 0 ? "" : mRecorder.Header(count - 1).mLabel.AsCStr();
		}
		else
		{
			out["mode"] = "sample";
			out["sample"] = mView.Sample();
			out["label"] = mRecorder.Header(mView.Sample()).mLabel.AsCStr();
		}
		return out;
	}

	Json::Value HistoryViewCommands::Fail(const char* command, const char* message) const
	{
		DIA_LOG_ERROR("HIS", "%s rejected: %s", command, message);
		Json::Value out(Json::objectValue);
		out["error"] = message;
		return out;
	}

	Json::Value HistoryViewCommands::View(const Json::Value& args)
	{
		int64_t sample = 0;
		if (!args.isObject() || !args.isMember("sample") || !ParseInt(args["sample"], sample)) { return Fail(kViewName, "missing or non-integer param 'sample'"); }
		if (sample < 0 || sample >= static_cast<int64_t>(mRecorder.SampleCount())) { return Fail(kViewName, "param 'sample' is out of range"); }
		ViewResult res;
		if (!mView.ViewSample(static_cast<uint32_t>(sample), res)) { return Fail(kViewName, res.mError); }
		return Result();
	}

	Json::Value HistoryViewCommands::Live()
	{
		mView.GoLive();
		return Result();
	}

	Json::Value HistoryViewCommands::Step(const Json::Value& args)
	{
		int64_t n = 0;
		if (!args.isObject() || !args.isMember("n") || !ParseInt(args["n"], n)) { return Fail(kStepName, "missing or non-integer param 'n'"); }
		if (n > (int64_t(1) << 40)) { n = int64_t(1) << 40; }	// far beyond any sample count; keeps base + n from overflowing
		if (n < -(int64_t(1) << 40)) { n = -(int64_t(1) << 40); }
		bool hasBand = false;
		uint32_t band = 0;
		if (args.isMember("band"))
		{
			const char* error = nullptr;
			if (!ParseBand(args["band"], mConfig, band, error)) { return Fail(kStepName, error); }
			hasBand = true;
		}
		const uint32_t count = mRecorder.SampleCount();
		if (count == 0) { return Fail(kStepName, "no samples recorded"); }
		const int64_t latest = static_cast<int64_t>(count) - 1;
		const int64_t base = mView.IsLive() ? latest : static_cast<int64_t>(mView.Sample());

		int64_t target = base;
		if (!hasBand)
		{
			target = base + n;		// n is clamped far below int64 overflow by ParseInt
			if (target < 0) { target = 0; }
			if (target > latest) { target = latest; }
		}
		else if (n != 0)
		{
			const int64_t dir = n < 0 ? -1 : 1;
			const int64_t end = dir < 0 ? 0 : latest;	// clamp target when no sample qualifies
			target = end;
			for (int64_t s = base + dir; s >= 0 && s <= latest; s += dir)
			{
				if (mRecorder.HasHeader(static_cast<uint32_t>(s)) && mRecorder.Header(static_cast<uint32_t>(s)).mBand >= band) { target = s; break; }
			}
		}
		ViewResult res;
		if (!mView.ViewSample(static_cast<uint32_t>(target), res)) { return Fail(kStepName, res.mError); }
		return Result();
	}

	Json::Value HistoryViewCommands::Event(const Json::Value& args)
	{
		if (!args.isObject() || !args.isMember("direction") || !args["direction"].isString()) { return Fail(kEventName, "missing or non-string param 'direction'"); }
		const std::string dir = args["direction"].asString();
		if (dir != "prev" && dir != "next") { return Fail(kEventName, "param 'direction' must be prev or next"); }
		const bool prev = (dir == "prev");

		EventScan scan;
		if (args.isMember("type"))
		{
			if (!args["type"].isString() || args["type"].asString().empty()) { return Fail(kEventName, "param 'type' must be a non-empty string"); }
			scan.mFilter = true;
			scan.mType = StringCRC(args["type"].asString().c_str()).Value();
		}
		const uint32_t count = mRecorder.SampleCount();
		if (count == 0) { return Fail(kEventName, "no samples recorded"); }
		const uint32_t current = mView.IsLive() ? count - 1 : mView.Sample();
		scan.mWantLatest = prev;
		if (prev)
		{
			if (current > 0) { mRecorder.Events(0, current - 1).ForEach(&VisitEvent, &scan); }
		}
		else if (current + 1 < count)
		{
			mRecorder.Events(current + 1, count - 1).ForEach(&VisitEvent, &scan);
		}
		if (!scan.mFound) { return Fail(kEventName, prev ? "no earlier event" : "no later event"); }
		ViewResult res;
		if (!mView.ViewSample(scan.mBest, res)) { return Fail(kEventName, res.mError); }
		return Result();
	}

	bool HistoryViewCommands::Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (mRegistered) { return true; }

		struct Def { const char* name; Dia::DebugServer::SimHelpInfo help; Dia::CellLayers::MirrorQueryHandler handler; };
		const Def defs[] = {
			{ kViewName, Help("Show recorded sample <sample> (0 to latest). Enters scrub mode and stays on it as samples arrive. Returns {ok, mode, sample, label}.", kParamsView, 1u),
				[this](const Json::Value& args) -> Json::Value { return View(args); } },
			{ kLiveName, Help("Return to live values. Returns {ok, mode, sample, label}.", nullptr, 0u),
				[this](const Json::Value&) -> Json::Value { return Live(); } },
			{ kStepName, Help("Move the scrubbed sample by n (negative = earlier), or with band to the nearest sample of at least that band. Clamped. Returns {ok, mode, sample, label}.", kParamsStep, 2u),
				[this](const Json::Value& args) -> Json::Value { return Step(args); } },
			{ kEventName, Help("Jump to the previous or next sample that has an event, optionally of one event type. Returns {ok, mode, sample, label}.", kParamsEvent, 2u),
				[this](const Json::Value& args) -> Json::Value { return Event(args); } } };

		for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); ++i)
		{
			if (registrar.RegisterCommand(defs[i].name, defs[i].help, defs[i].handler)) { continue; }
			DIA_LOG_ERROR("HIS", "history view commands: registering %s was rejected, rolling back", defs[i].name);
			for (size_t k = 0; k < i; ++k) { registrar.Unregister(defs[k].name); }
			return false;
		}
		mRegistered = true;
		return true;
	}

	void HistoryViewCommands::Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (!mRegistered) { return; }
		registrar.Unregister(kViewName);
		registrar.Unregister(kLiveName);
		registrar.Unregister(kStepName);
		registrar.Unregister(kEventName);
		mRegistered = false;
	}
}

#endif // DIA_DEBUG
