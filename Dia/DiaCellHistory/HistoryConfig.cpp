#include <DiaCellHistory/HistoryConfig.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/ValueInfo.h>

#include <string>

namespace Dia::CellHistory
{
	namespace
	{
		using Dia::ValueSchema::SetText;

		HistoryConfigResult Fail(const char* key, const char* what)
		{
			HistoryConfigResult r;
			r.mOk = false;
			r.mError.Format("history.%s: %s", key, what);
			return r;
		}

		bool IsKnownKey(const std::string& k)
		{
			static const char* const kKnown[] = { "bands", "track", "grid", "cellBudgetMB", "aggregateBudgetMB", "eventBudgetMB", "maxAggregateLayers" };
			for (const char* n : kKnown) { if (k == n) { return true; } }
			return false;
		}

		bool ReadPositive(const Json::Value& v, uint32_t maxValue, uint32_t& out)
		{
			if (!v.isIntegral() || v.isBool()) { return false; }
			if (!v.isUInt64() || v.asUInt64() < 1 || v.asUInt64() > maxValue) { return false; }
			out = static_cast<uint32_t>(v.asUInt64());
			return true;
		}

		// Reads an array of non-empty strings no longer than maxLen into dst (capacity maxCount).
		template<class StringT>
		HistoryConfigResult ReadNames(const char* key, const Json::Value& v, StringT* dst, uint32_t maxCount, uint32_t maxLen, uint32_t& outCount)
		{
			if (!v.isArray()) { return Fail(key, "must be an array of strings"); }
			if (v.size() > maxCount) { return Fail(key, "has more entries than the fixed capacity allows"); }
			for (Json::ArrayIndex i = 0; i < v.size(); ++i)
			{
				if (!v[i].isString()) { return Fail(key, "entries must be strings"); }
				const std::string s = v[i].asString();
				if (s.empty()) { return Fail(key, "entries must not be empty"); }
				if (s.size() > maxLen) { return Fail(key, "entry is too long"); }
			}
			for (Json::ArrayIndex i = 0; i < v.size(); ++i) { SetText(dst[i], v[i].asCString()); }
			for (Json::ArrayIndex i = v.size(); i < maxCount; ++i) { SetText(dst[i], ""); }
			outCount = v.size();
			return HistoryConfigResult();
		}
	}

	HistoryConfigResult ParseHistoryConfig(const Json::Value& json, HistoryConfig& out)
	{
		if (!json.isObject()) { return Fail("(root)", "must be an object"); }

		HistoryConfig cfg;	// defaults; committed to out only if everything validates
		HistoryConfigResult r;

		if (json.isMember("bands"))
		{
			r = ReadNames("bands", json["bands"], cfg.mBands, HistoryConfig::kMaxBands, HistoryConfig::kBandNameLength, cfg.mBandCount);
			if (!r.mOk) { return r; }
		}
		if (json.isMember("track"))
		{
			r = ReadNames("track", json["track"], cfg.mTrack, HistoryConfig::kMaxTrack, HistoryConfig::kLayerNameLength, cfg.mTrackCount);
			if (!r.mOk) { return r; }
		}
		if (json.isMember("grid"))
		{
			const Json::Value& g = json["grid"];
			if (!g.isString()) { return Fail("grid", "must be a string"); }
			const std::string s = g.asString();
			if (s.size() > HistoryConfig::kGridNameLength) { return Fail("grid", "name is too long"); }
			SetText(cfg.mGrid, s.c_str());
		}

		struct IntKey { const char* mName; uint32_t* mDst; uint32_t mMax; };
		const IntKey ints[] = {
			{ "cellBudgetMB", &cfg.mCellBudgetMB, 0xFFFFFFFFu },
			{ "aggregateBudgetMB", &cfg.mAggregateBudgetMB, 0xFFFFFFFFu },
			{ "eventBudgetMB", &cfg.mEventBudgetMB, 0xFFFFFFFFu },
			{ "maxAggregateLayers", &cfg.mMaxAggregateLayers, HistoryConfig::kHardMaxAggregateLayers },
		};
		for (const IntKey& k : ints)
		{
			if (!json.isMember(k.mName)) { continue; }
			if (!ReadPositive(json[k.mName], k.mMax, *k.mDst))
			{
				return Fail(k.mName, k.mMax == 0xFFFFFFFFu ? "must be a positive integer" : "must be an integer from 1 to the hard cap of 64");
			}
		}

		const Json::Value::Members names = json.getMemberNames();
		for (const std::string& n : names)
		{
			if (!IsKnownKey(n)) { DIA_LOG_WARNING("HIS", "history config: unknown key '%s' ignored", n.c_str()); }
		}

		out = cfg;
		return HistoryConfigResult();
	}

	namespace
	{
		const Json::Value* HistoryKey(const Dia::ValueSchema::ValueInfo& info, const char* key)
		{
			const Json::Value& h = info.mHistory;
			if (!h.isObject() || !h.isMember(key)) { return nullptr; }
			return &h[key];
		}

		bool BoolKey(const Dia::ValueSchema::ValueInfo& info, const char* key, bool dflt)
		{
			const Json::Value* v = HistoryKey(info, key);
			return (v != nullptr && v->isBool()) ? v->asBool() : dflt;
		}
	}

	bool HistoryTrack(const Dia::ValueSchema::ValueInfo& info) { return BoolKey(info, "track", false); }

	bool HistoryGroup(const Dia::ValueSchema::ValueInfo& info)
	{
		const Json::Value* v = HistoryKey(info, "group");
		if (v == nullptr) { return false; }
		if (v->isBool()) { return v->asBool(); }
		return v->isString() && v->asString().size() > 0;
	}

	bool HistoryAggregate(const Dia::ValueSchema::ValueInfo& info) { return BoolKey(info, "aggregate", true); }
}
