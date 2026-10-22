#include <DiaValueSchema/RampLoader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <string>
#include <cstring>
#include <set>

namespace Dia::ValueSchema
{
	using namespace Dia::Core::Containers;

	namespace
	{
		bool FailFile(RampLoadResult& r, const char* reason)
		{
			r.mOk = false;
			SetText(r.mFileReason, reason);
			r.mRamps.RemoveAll();
			DIA_LOG_ERROR("VSC", "Ramp file '%s' rejected: %s", r.mFile.AsCStr(), reason);
			return false;
		}

		bool IsIdChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

		// [a-z0-9_]+(\.[a-z0-9_]+)*, max 63 chars.
		bool IsValidId(const std::string& id)
		{
			if (id.empty() || id.size() > String64::Capacity() - 1) { return false; }
			bool segmentEmpty = true;
			for (char c : id)
			{
				if (c == '.')
				{
					if (segmentEmpty) { return false; }
					segmentEmpty = true;
				}
				else if (IsIdChar(c)) { segmentEmpty = false; }
				else { return false; }
			}
			return !segmentEmpty;
		}

		bool OnlyKeys(const Json::Value& obj, const char* a, const char* b, const char* c, std::string& offender)
		{
			const Json::Value::Members names = obj.getMemberNames();
			for (const std::string& n : names)
			{
				if (n != a && n != b && n != c) { offender = n; return false; }
			}
			return true;
		}

		bool ReadStops(const Json::Value& ramp, RampData& out, std::string& why)
		{
			const Json::Value& arr = ramp["stops"];
			if (!ramp.isMember("stops") || !arr.isArray()) { why = "'stops' must be an array"; return false; }
			if (arr.size() < 2 || arr.size() > kMaxRampStops)
			{
				why = "continuous ramp needs 2 to " + std::to_string(kMaxRampStops) + " stops, got " + std::to_string(arr.size());
				return false;
			}
			double prev = -1.0;
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const Json::Value& s = arr[i];
				const std::string at = "stop " + std::to_string(i) + ": ";
				if (!s.isObject()) { why = at + "must be an object"; return false; }
				std::string offender;
				if (!OnlyKeys(s, "t", "colour", "colour", offender)) { why = at + "unknown key '" + offender + "'"; return false; }
				if (!s.isMember("t") || !s["t"].isNumeric()) { why = at + "'t' must be a number"; return false; }
				if (!s.isMember("colour") || !s["colour"].isString()) { why = at + "'colour' must be a string"; return false; }
				const double t = s["t"].asDouble();
				if (!(t > prev)) { why = at + "'t' must be strictly increasing"; return false; }
				if (t < 0.0 || t > 1.0) { why = at + "'t' must be within 0..1"; return false; }
				uint32_t rgba = 0;
				if (!ParseHexColour(s["colour"].asCString(), rgba)) { why = at + "colour must be #rrggbb"; return false; }
				out.mStops[i].mT = static_cast<float>(t);
				out.mStops[i].mRgba8 = rgba;
				prev = t;
			}
			if (arr[0]["t"].asDouble() != 0.0) { why = "first stop 't' must be exactly 0"; return false; }
			if (arr[arr.size() - 1]["t"].asDouble() != 1.0) { why = "last stop 't' must be exactly 1"; return false; }
			out.mStopCount = arr.size();
			return true;
		}

		bool ReadColours(const Json::Value& ramp, RampData& out, std::string& why)
		{
			const Json::Value& arr = ramp["colours"];
			if (!ramp.isMember("colours") || !arr.isArray()) { why = "'colours' must be an array"; return false; }
			if (arr.size() < 2 || arr.size() > kMaxCategoricalColours)
			{
				why = "categorical ramp needs 2 to " + std::to_string(kMaxCategoricalColours) + " colours, got " + std::to_string(arr.size());
				return false;
			}
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isString()) { why = "colour " + std::to_string(i) + " must be a string"; return false; }
				if (!ParseHexColour(arr[i].asCString(), out.mColours[i])) { why = "colour " + std::to_string(i) + " must be #rrggbb"; return false; }
			}
			out.mColourCount = arr.size();
			return true;
		}

		// Returns true and fills out on success; otherwise sets why. idText holds the best label for diagnostics.
		bool ReadRamp(const Json::Value& ramp, Json::ArrayIndex index, RampData& out, std::string& idText, std::string& why)
		{
			idText = "#" + std::to_string(index);
			if (!ramp.isObject()) { why = "ramp must be an object"; return false; }
			if (!ramp.isMember("id") || !ramp["id"].isString()) { why = "missing string 'id'"; return false; }
			const std::string id = ramp["id"].asString();
			if (!id.empty() && id.size() <= 100) { idText = id; }	// name the offending id in the error
			if (!IsValidId(id)) { why = "id must be lower-case dotted [a-z0-9_] names, max 63 characters"; return false; }
			idText = id;
			if (!ramp.isMember("kind") || !ramp["kind"].isString()) { why = "missing string 'kind'"; return false; }
			const std::string kind = ramp["kind"].asString();

			std::string offender;
			if (kind == "continuous")
			{
				if (!OnlyKeys(ramp, "id", "kind", "stops", offender)) { why = "unknown key '" + offender + "'"; return false; }
				out.mKind = RampKind::Continuous;
				if (!ReadStops(ramp, out, why)) { return false; }
			}
			else if (kind == "categorical")
			{
				if (!OnlyKeys(ramp, "id", "kind", "colours", offender)) { why = "unknown key '" + offender + "'"; return false; }
				out.mKind = RampKind::Categorical;
				if (!ReadColours(ramp, out, why)) { return false; }
			}
			else { why = "unknown 'kind' (expected continuous or categorical)"; return false; }

			SetText(out.mName, id.c_str());
			out.mId = Dia::Core::StringCRC(id.c_str());
			return true;
		}
	}

	RampLoadResult RampLoader::LoadFromText(const char* text, const char* fileName)
	{
		RampLoadResult r;
		if (fileName == nullptr) { fileName = ""; }
		if (strlen(fileName) <= String256::Capacity() - 1) { SetText(r.mFile, fileName); }
		else { SetText(r.mFile, "<file name too long>"); }
		
		// A null text is an empty document. A leading UTF-8 byte order mark is skipped (Python reads with utf-8-sig).
		if (text == nullptr) { text = ""; }
		if (static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) { text += 3; }
		
		Json::Value root;
		Json::Reader reader;
		bool parsed = false;
		std::string parseError;
		try
		{
			parsed = reader.parse(std::string(text), root);
			if (!parsed) { parseError = reader.getFormattedErrorMessages(); }
		}
		catch (const std::exception& e)
		{
			// jsoncpp throws when nesting exceeds its stack limit (1000 values) or memory runs out.
			parseError = e.what();
		}
		if (!parsed)
		{
			FailFile(r, (std::string("unparsable JSON: ") + parseError).c_str());
			return r;
		}
		if (!root.isObject()) { FailFile(r, "root must be an object"); return r; }
		if (!root.isMember("format") || !root["format"].isString() || root["format"].asString() != "dia.ramps")
		{
			FailFile(r, "'format' must be \"dia.ramps\"");
			return r;
		}
		if (!root.isMember("version") || !root["version"].isInt() || root["version"].asInt() != 1)
		{
			FailFile(r, "'version' must be the integer 1");
			return r;
		}
		if (!root.isMember("ramps") || !root["ramps"].isArray()) { FailFile(r, "missing 'ramps' array"); return r; }
		const Json::Value::Members keys = root.getMemberNames();
		for (const std::string& n : keys)
		{
			if (n != "format" && n != "version" && n != "ramps")
			{
				FailFile(r, (std::string("unknown file-level key '") + n + "'").c_str());
				return r;
			}
		}

		const Json::Value& ramps = root["ramps"];
		if (ramps.size() > 0) { r.mRamps.Reserve(ramps.size()); r.mRampErrors.Reserve(ramps.size()); }
		std::set<std::string> seen;
		for (Json::ArrayIndex i = 0; i < ramps.size(); ++i)
		{
			RampData data;
			std::string idText, why;
			bool ok = ReadRamp(ramps[i], i, data, idText, why);
			if (ok && !seen.insert(idText).second) { ok = false; why = "duplicate id in file"; }
			if (ok)
			{
				r.mRamps.Add(data);
				continue;
			}
			++r.mRampsRejected;
			String512 msg;
			msg.Format("Ramp '%s' in '%s' rejected: %s", idText.c_str(), r.mFile.AsCStr(), why.c_str());
			r.mRampErrors.Add(msg);
			DIA_LOG_ERROR("VSC", "%s", msg.AsCStr());
		}
		r.mOk = true;
		return r;
	}
}
