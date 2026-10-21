#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaObservation/Log/DiaLog.h>
#include <string>
#include <cstring>

namespace Dia::ValueSchema
{
	using namespace Dia::Core::Containers;

	namespace
	{
		struct EntryContext
		{
			const char*	mFile;
			const char*	mEntry;
		};

		bool Fail(LoadResult& r, const EntryContext& ctx, const char* reason)
		{
			r.mOk = false;
			r.mFailure.mFile = ctx.mFile;
			r.mFailure.mEntry = ctx.mEntry;
			// Format() with "%s" avoids treating a percent sign in user text as a format specifier.
			r.mFailure.mReason.Format("%s", reason);
			r.mEntries.RemoveAll();
			DIA_LOG_ERROR("VSC", "Schema '%s' entry '%s' rejected: %s", ctx.mFile, ctx.mEntry, reason);
			return false;
		}

		// Single path for every string assignment: fails (naming field and entry) instead of truncating.
		template<class T>
		bool AssignString(LoadResult& r, const EntryContext& ctx, T& dst, const char* src, const char* field)
		{
			if (strlen(src) > T::Capacity() - 1)
			{
				std::string reason = std::string("field '") + field + "' exceeds capacity of " + std::to_string(T::Capacity() - 1) + " characters";
				return Fail(r, ctx, reason.c_str());
			}
			dst = src;
			return true;
		}

		template<class T>
		bool AssignOptional(LoadResult& r, const EntryContext& ctx, T& dst, const Json::Value& obj, const char* field)
		{
			if (!obj.isMember(field)) { return true; }
			if (!obj[field].isString()) { return Fail(r, ctx, (std::string("field '") + field + "' must be a string").c_str()); }
			return AssignString(r, ctx, dst, obj[field].asCString(), field);
		}

		bool ParseKind(const std::string& s, ValueKind& out)
		{
			if (s == "parameter")			{ out = ValueKind::Parameter; return true; }
			if (s == "field")				{ out = ValueKind::Field; return true; }
			if (s == "metric")				{ out = ValueKind::Metric; return true; }
			if (s == "arg")					{ out = ValueKind::Arg; return true; }
			if (s == "command")				{ out = ValueKind::Command; return true; }
			if (s == "event")				{ out = ValueKind::Event; return true; }
			if (s == "component_field")		{ out = ValueKind::ComponentField; return true; }
			return false;
		}

		bool ParseType(const std::string& s, ValueType& out)
		{
			if (s == "float")	{ out = ValueType::Float; return true; }
			if (s == "int")		{ out = ValueType::Int; return true; }
			if (s == "bool")	{ out = ValueType::Bool; return true; }
			if (s == "enum")	{ out = ValueType::Enum; return true; }
			if (s == "string")	{ out = ValueType::String; return true; }
			if (s == "ref")		{ out = ValueType::Ref; return true; }
			if (s == "ref[]")	{ out = ValueType::RefList; return true; }
			if (s == "map")		{ out = ValueType::Map; return true; }
			return false;
		}

		bool ReadStringList(LoadResult& r, const EntryContext& ctx, ValueList<String128>& dst, const Json::Value& obj, const char* field)
		{
			if (!obj.isMember(field)) { return true; }
			const Json::Value& arr = obj[field];
			if (!arr.isArray()) { return Fail(r, ctx, (std::string("field '") + field + "' must be an array").c_str()); }
			if (arr.size() == 0) { return true; }
			dst.Reserve(arr.size());
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				if (!arr[i].isString()) { return Fail(r, ctx, (std::string("field '") + field + "' must contain strings").c_str()); }
				String128 s;
				if (!AssignString(r, ctx, s, arr[i].asCString(), field)) { return false; }
				dst.Add(s);
			}
			return true;
		}

		bool ReadEnumValues(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& entry)
		{
			if (!entry.isMember("values")) { return true; }
			const Json::Value& arr = entry["values"];
			if (!arr.isArray()) { return Fail(r, ctx, "field 'values' must be an array"); }
			if (arr.size() == 0) { return true; }
			info.mEnumValues.Reserve(arr.size());
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const Json::Value& v = arr[i];
				if (!v.isObject()) { return Fail(r, ctx, "enum value must be an object"); }
				EnumValue ev;
				if (!AssignOptional(r, ctx, ev.mId, v, "id")) { return false; }
				if (!AssignOptional(r, ctx, ev.mLabel, v, "label")) { return false; }
				if (!AssignOptional(r, ctx, ev.mDescription, v, "description")) { return false; }
				if (v.isMember("colour"))
				{
					if (!AssignOptional(r, ctx, ev.mColour, v, "colour")) { return false; }
					ev.mHasColour = true;
				}
				else
				{
					ev.mColour = GetCategoricalColour(i);
					ev.mHasColour = false;
				}
				info.mEnumValues.Add(ev);
			}
			return true;
		}

		bool ReadEntry(LoadResult& r, const char* fileName, const Json::Value& e, ValueInfo& info)
		{
			EntryContext ctx{ fileName, "" };
			if (!e.isObject()) { return Fail(r, ctx, "entry must be an object"); }
			if (!e.isMember("key") || !e["key"].isString()) { return Fail(r, ctx, "entry is missing string 'key'"); }
			const char* key = e["key"].asCString();
			String128 entryName;
			entryName = (strlen(key) > 127) ? "<key too long>" : key;
			ctx.mEntry = entryName.AsCStr();

			if (!AssignString(r, ctx, info.mKey, key, "key")) { return false; }
			info.mFor = r.mFor;
			if (!AssignString(r, ctx, info.mSourceFile, fileName, "sourceFile")) { return false; }

			if (e.isMember("kind"))
			{
				if (!e["kind"].isString() || !ParseKind(e["kind"].asString(), info.mKind)) { return Fail(r, ctx, "unknown 'kind'"); }
			}
			if (e.isMember("type"))
			{
				if (!e["type"].isString() || !ParseType(e["type"].asString(), info.mType)) { return Fail(r, ctx, "unknown 'type'"); }
			}

			if (!AssignOptional(r, ctx, info.mLabel, e, "label")) { return false; }
			if (!AssignOptional(r, ctx, info.mDescription, e, "description")) { return false; }
			if (!AssignOptional(r, ctx, info.mUnit, e, "unit")) { return false; }
			if (!AssignOptional(r, ctx, info.mHint, e, "hint")) { return false; }
			if (!AssignOptional(r, ctx, info.mSource, e, "source")) { return false; }
			if (!AssignOptional(r, ctx, info.mRef, e, "ref")) { return false; }

			if (e.isMember("min") && e["min"].isNumeric()) { info.mHasMin = true; info.mMin = e["min"].asDouble(); }
			if (e.isMember("max") && e["max"].isNumeric()) { info.mHasMax = true; info.mMax = e["max"].asDouble(); }
			if (e.isMember("default"))
			{
				const Json::Value& d = e["default"];
				if (d.isBool())			{ info.mHasDefault = true; info.mDefaultNumber = d.asBool() ? 1.0 : 0.0; }
				else if (d.isNumeric())	{ info.mHasDefault = true; info.mDefaultNumber = d.asDouble(); }
				else if (d.isString())
				{
					info.mHasDefault = true;
					if (!AssignString(r, ctx, info.mDefaultString, d.asCString(), "default")) { return false; }
				}
			}

			if (e.isMember("unique") && e["unique"].isBool())		{ info.mUnique = e["unique"].asBool(); }
			if (e.isMember("required") && e["required"].isBool())	{ info.mRequired = e["required"].asBool(); }
			if (e.isMember("ui"))		{ info.mUi = e["ui"]; }
			if (e.isMember("history"))	{ info.mHistory = e["history"]; }

			if (!ReadStringList(r, ctx, info.mAffects, e, "affects")) { return false; }
			if (!ReadStringList(r, ctx, info.mRelated, e, "related")) { return false; }
			if (!ReadEnumValues(r, ctx, info, e)) { return false; }

			info.mPathSegments.Reserve(16);
			if (!SplitRecordPath(key, info.mPathSegments)) { return Fail(r, ctx, "key has a path segment that is too long or has too many segments"); }
			return true;
		}
	}

	bool SplitRecordPath(const char* key, ValueList<String64>& outSegments)
	{
		std::string seg;
		auto flush = [&]() -> bool
		{
			if (seg.empty()) { return true; }
			if (seg.size() > String64::Capacity() - 1 || outSegments.Size() >= outSegments.Capacity()) { return false; }
			String64 s;
			s = seg.c_str();
			outSegments.Add(s);
			seg.clear();
			return true;
		};
		const char* p = key;
		while (*p)
		{
			if (*p == '.')
			{
				if (!flush()) { return false; }
			}
			else if (p[0] == '[' && p[1] == ']')
			{
				if (!flush()) { return false; }
				seg = "[]";
				if (!flush()) { return false; }
				++p;
			}
			else
			{
				seg += *p;
			}
			++p;
		}
		return flush();
	}

	LoadResult ValueSchemaLoader::LoadFromText(const char* text, const char* fileName)
	{
		LoadResult r;
		EntryContext fileCtx{ fileName, "" };

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(std::string(text), root))
		{
			Fail(r, fileCtx, (std::string("unparsable JSON: ") + reader.getFormattedErrorMessages()).c_str());
			return r;
		}
		if (!root.isObject()) { Fail(r, fileCtx, "root must be an object"); return r; }
		if (!root.isMember("entries") || !root["entries"].isArray()) { Fail(r, fileCtx, "missing 'entries' array"); return r; }

		if (root.isMember("for") && root["for"].isString())
		{
			if (!AssignString(r, fileCtx, r.mFor, root["for"].asCString(), "for")) { return r; }
		}

		const Json::Value& entries = root["entries"];
		if (entries.size() > 0) { r.mEntries.Reserve(entries.size()); }
		for (Json::ArrayIndex i = 0; i < entries.size(); ++i)
		{
			ValueInfo info;
			if (!ReadEntry(r, fileName, entries[i], info)) { return r; }
			r.mEntries.Add(info);
		}
		r.mOk = true;
		return r;
	}
}
