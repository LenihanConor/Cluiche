#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaObservation/Log/DiaLog.h>
#include <string>
#include <cstring>
#include <set>
#include <cmath>

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
			// SetText copies verbatim (a '%' in user text is never a format specifier) and bounds every copy:
			// a long jsoncpp message or unknown key is truncated to the String512 reason, never overflowed.
			SetText(r.mFailure.mFile, ctx.mFile);
			SetText(r.mFailure.mEntry, ctx.mEntry);
			SetText(r.mFailure.mReason, reason);
			r.mEntries.RemoveAll();
				r.mConstraints.RemoveAll();
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
			SetText(dst, src);
			return true;
		}

		template<class T>
		bool AssignOptional(LoadResult& r, const EntryContext& ctx, T& dst, const Json::Value& obj, const char* field)
		{
			if (!obj.isMember(field)) { return true; }
			if (!obj[field].isString()) { return Fail(r, ctx, (std::string("field '") + field + "' must be a string").c_str()); }
			return AssignString(r, ctx, dst, obj[field].asCString(), field);
		}

		bool RequireString(LoadResult& r, const EntryContext& ctx, const Json::Value& obj, const char* field, const char*& out)
		{
			if (!obj.isMember(field)) { return Fail(r, ctx, (std::string("missing required field '") + field + "'").c_str()); }
			if (!obj[field].isString()) { return Fail(r, ctx, (std::string("field '") + field + "' must be a string").c_str()); }
			out = obj[field].asCString();
			return true;
		}

		template<class T>
		bool AssignRequired(LoadResult& r, const EntryContext& ctx, T& dst, const Json::Value& obj, const char* field)
		{
			const char* v = nullptr;
			return RequireString(r, ctx, obj, field, v) && AssignString(r, ctx, dst, v, field);
		}

		bool ReadNumber(LoadResult& r, const EntryContext& ctx, const Json::Value& e, const char* field, bool& has, double& out)
		{
			if (!e.isMember(field)) { return true; }
			if (!e[field].isNumeric()) { return Fail(r, ctx, (std::string("field '") + field + "' must be a number").c_str()); }
			has = true;
			out = e[field].asDouble();
			return true;
		}

		bool ReadBool(LoadResult& r, const EntryContext& ctx, const Json::Value& e, const char* field, bool& out)
		{
			if (!e.isMember(field)) { return true; }
			if (!e[field].isBool()) { return Fail(r, ctx, (std::string("field '") + field + "' must be a boolean").c_str()); }
			out = e[field].asBool();
			return true;
		}

		bool IsKnownKey(const char* const* known, const char* key)
		{
			for (; *known; ++known) { if (strcmp(*known, key) == 0) { return true; } }
			return false;
		}

		const char* const kEntryKeys[] = { "key", "kind", "label", "description", "hint", "unit", "type", "min", "max", "default",
			"values", "affects", "related", "source", "unique", "required", "ref", "ui", "history", "payload", "vocabulary",
			"keyRef", "valueType", "bands", nullptr };
		const char* const kUiKeys[] = { "group", "step", "decimals", "ramp", "layer", "hidden", "format", "colour", "glyph", "hotkey", "delta_ramp",
			"default_base", "inset", "backdrop", "elevation_layer", "select_group", nullptr };
		const char* const kHistoryKeys[] = { "track", "group", "aggregate", nullptr };
		const char* const kFileKeys[] = { "format", "version", "for", "entries", "constraints", nullptr };

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
					SetText(ev.mColour, GetCategoricalColour(i));
					ev.mHasColour = false;
				}
				info.mEnumValues.Add(ev);
			}
			return true;
		}

		// Open block: any keys are kept; unknown ones warn once per (file, block, key).
		bool ReadOpenBlock(LoadResult& r, const EntryContext& ctx, std::set<std::string>& warned, const Json::Value& e,
			const char* block, const char* const* known, Json::Value& dst)
		{
			if (!e.isMember(block)) { return true; }
			if (!e[block].isObject()) { return Fail(r, ctx, (std::string("field '") + block + "' must be an object").c_str()); }
			dst = e[block];
			const Json::Value::Members names = dst.getMemberNames();
			for (const std::string& n : names)
			{
				if (IsKnownKey(known, n.c_str())) { continue; }
				if (!warned.insert(std::string(block) + "." + n).second) { continue; }
				String512 msg;
				msg.Format("Schema '%s' entry '%s': unknown %s key '%s' (kept)", ctx.mFile, ctx.mEntry, block, n.c_str());
				if (r.mWarnings.Size() == r.mWarnings.Capacity()) { r.mWarnings.Reserve(r.mWarnings.Size() == 0 ? 16 : r.mWarnings.Size() * 2); }
				r.mWarnings.Add(msg);
				DIA_LOG_WARNING("VSC", "%s", msg.AsCStr());
			}
			return true;
		}

		bool ReadPayload(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& e)
		{
			if (!e.isMember("payload")) { return true; }
			if (info.mKind != ValueKind::Event) { return Fail(r, ctx, "field 'payload' is only valid on event entries"); }
			const Json::Value& arr = e["payload"];
			if (!arr.isArray()) { return Fail(r, ctx, "field 'payload' must be an array"); }
			if (arr.size() == 0) { return true; }
			info.mPayload.Reserve(arr.size());
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const Json::Value& v = arr[i];
				if (!v.isObject()) { return Fail(r, ctx, "payload item must be an object"); }
				PayloadField f;
				if (!AssignRequired(r, ctx, f.mName, v, "name")) { return false; }
				const char* t = nullptr;
				if (!RequireString(r, ctx, v, "type", t)) { return false; }
				if (!ParseType(t, f.mType)) { return Fail(r, ctx, "payload item has unknown 'type'"); }
				if (!AssignRequired(r, ctx, f.mDescription, v, "description")) { return false; }
				if (!AssignOptional(r, ctx, f.mUnit, v, "unit")) { return false; }
				info.mPayload.Add(f);
			}
			return true;
		}

		bool ReadVocabulary(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& e)
		{
			if (!e.isMember("vocabulary")) { return true; }
			if (info.mKind != ValueKind::Event) { return Fail(r, ctx, "field 'vocabulary' is only valid on event entries"); }
			const Json::Value& arr = e["vocabulary"];
			if (!arr.isArray()) { return Fail(r, ctx, "field 'vocabulary' must be an array"); }
			if (arr.size() == 0) { return true; }
			info.mVocabulary.Reserve(arr.size());
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const Json::Value& v = arr[i];
				if (!v.isObject()) { return Fail(r, ctx, "vocabulary item must be an object"); }
				VocabularyEntry f;
				if (!AssignRequired(r, ctx, f.mId, v, "id")) { return false; }
				if (!AssignOptional(r, ctx, f.mLabel, v, "label")) { return false; }
				if (!AssignRequired(r, ctx, f.mDescription, v, "description")) { return false; }
				if (!AssignOptional(r, ctx, f.mUnit, v, "unit")) { return false; }
				info.mVocabulary.Add(f);
			}
			return true;
		}

		bool ReadTypeSpecific(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& e)
		{
			const bool isRef = info.mType == ValueType::Ref || info.mType == ValueType::RefList;
			if (isRef)
			{
				if (!AssignRequired(r, ctx, info.mRef, e, "ref")) { return false; }
			}
			else if (e.isMember("ref")) { return Fail(r, ctx, "field 'ref' is only valid on ref and ref[] types"); }

			if (info.mType == ValueType::Map)
			{
				if (!AssignRequired(r, ctx, info.mMapKeyRef, e, "keyRef")) { return false; }
				const char* vt = nullptr;
				if (!RequireString(r, ctx, e, "valueType", vt)) { return false; }
				ValueType t;
				if (!ParseType(vt, t) || t == ValueType::Ref || t == ValueType::RefList || t == ValueType::Map)
				{
					return Fail(r, ctx, "field 'valueType' must be one of float, int, bool, string, enum");
				}
				info.mMapValueType = t;
			}
			else
			{
				if (e.isMember("keyRef")) { return Fail(r, ctx, "field 'keyRef' is only valid on map types"); }
				if (e.isMember("valueType")) { return Fail(r, ctx, "field 'valueType' is only valid on map types"); }
			}

			if (info.mType == ValueType::Enum && !e.isMember("values")) { return Fail(r, ctx, "enum type requires a 'values' array"); }
			return true;
		}

		bool IsNumericType(ValueType t) { return t == ValueType::Float || t == ValueType::Int; }

		bool RangeCheck(LoadResult& r, const EntryContext& ctx, const ValueInfo& info, double v, const std::string& what)
		{
			if (info.mHasMin && v < info.mMin) { return Fail(r, ctx, (what + " is below 'min'").c_str()); }
			if (info.mHasMax && v > info.mMax) { return Fail(r, ctx, (what + " is above 'max'").c_str()); }
			return true;
		}

		// Object `default` on a map entry: every value checked against valueType, min and max.
		bool ReadMapDefault(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& d)
		{
			info.mHasMapDefault = true;
			const Json::Value::Members names = d.getMemberNames();
			if (!names.empty()) { info.mMapDefaults.Reserve(static_cast<unsigned int>(names.size())); }
			for (const std::string& n : names)
			{
				const Json::Value& v = d[n];
				const std::string what = "map default '" + n + "'";
				MapDefault md;
				if (!AssignString(r, ctx, md.mMapKey, n.c_str(), "default")) { return false; }
				switch (info.mMapValueType)
				{
				case ValueType::Float:
				case ValueType::Int:
					if (!v.isNumeric()) { return Fail(r, ctx, (what + " must be a number (field 'default')").c_str()); }
					md.mNumber = v.asDouble();
					if (info.mMapValueType == ValueType::Int && md.mNumber != std::floor(md.mNumber)) { return Fail(r, ctx, (what + " must be a whole number (field 'default')").c_str()); }
					if (!RangeCheck(r, ctx, info, md.mNumber, what + " (field 'default')")) { return false; }
					break;
				case ValueType::Bool:
					if (!v.isBool()) { return Fail(r, ctx, (what + " must be a boolean (field 'default')").c_str()); }
					md.mNumber = v.asBool() ? 1.0 : 0.0;
					break;
				default:	// string, enum
					if (!v.isString()) { return Fail(r, ctx, (what + " must be a string (field 'default')").c_str()); }
					if (!AssignString(r, ctx, md.mString, v.asCString(), "default")) { return false; }
					break;
				}
				info.mMapDefaults.Add(md);
			}
			return true;
		}

		bool ReadBands(LoadResult& r, const EntryContext& ctx, ValueInfo& info, const Json::Value& e)
		{
			if (!e.isMember("bands")) { return true; }
			if (!IsNumericType(info.mType)) { return Fail(r, ctx, "field 'bands' is only valid on float and int types"); }
			const Json::Value& b = e["bands"];
			if (!b.isObject()) { return Fail(r, ctx, "field 'bands' must be an object"); }
			const Json::Value::Members names = b.getMemberNames();
			if (!names.empty()) { info.mBands.Reserve(static_cast<unsigned int>(names.size())); }
			for (const std::string& n : names)
			{
				const Json::Value& v = b[n];
				const std::string what = "band '" + n + "'";
				if (!v.isArray() || v.size() != 2 || !v[0u].isNumeric() || !v[1u].isNumeric())
				{
					return Fail(r, ctx, (what + " must be [lo, hi] with two numbers").c_str());
				}
				BandInfo bi;
				if (!AssignString(r, ctx, bi.mName, n.c_str(), "bands")) { return false; }
				bi.mLo = v[0u].asDouble();
				bi.mHi = v[1u].asDouble();
				if (!std::isfinite(bi.mLo) || !std::isfinite(bi.mHi)) { return Fail(r, ctx, (what + " must be finite").c_str()); }
				if (bi.mLo > bi.mHi) { return Fail(r, ctx, (what + " has lo > hi").c_str()); }
				if ((info.mHasMin && bi.mLo < info.mMin) || (info.mHasMax && bi.mHi > info.mMax))
				{
					return Fail(r, ctx, (what + " lies outside [min, max]").c_str());
				}
				info.mBands.Add(bi);
			}
			return true;
		}

		bool ParseOp(const std::string& s, ConstraintOp& out)
		{
			if (s == "<")	{ out = ConstraintOp::Lt; return true; }
			if (s == "<=")	{ out = ConstraintOp::Le; return true; }
			if (s == ">")	{ out = ConstraintOp::Gt; return true; }
			if (s == ">=")	{ out = ConstraintOp::Ge; return true; }
			if (s == "==")	{ out = ConstraintOp::Eq; return true; }
			if (s == "!=")	{ out = ConstraintOp::Ne; return true; }
			return false;
		}

		// The head of a constraint operand ("<key>" or "<key>.<mapKey>") must name a numeric entry or a numeric map entry.
		// Only the head is checked here; the map-key part is checked by the consumer.
		bool OperandIsNumeric(const LoadResult& r, const std::string& operand, bool& found)
		{
			found = false;
			for (unsigned int i = 0; i < r.mEntries.Size(); ++i)
			{
				const ValueInfo& v = r.mEntries[i];
				const std::string key = v.mKey.AsCStr();
				if (v.mType == ValueType::Map)
				{
					if (operand.size() > key.size() + 1 && operand.compare(0, key.size(), key) == 0 && operand[key.size()] == '.')
					{
						found = true;
						return v.mMapValueType == ValueType::Float || v.mMapValueType == ValueType::Int;
					}
				}
				else if (key == operand)
				{
					found = true;
					return v.mType == ValueType::Float || v.mType == ValueType::Int;
				}
			}
			return false;
		}

		bool ReadConstraints(LoadResult& r, const char* fileName, const Json::Value& arr)
		{
			if (!arr.isArray())
			{
				EntryContext ctx{ fileName, "" };
				return Fail(r, ctx, "field 'constraints' must be an array");
			}
			if (arr.size() > 0) { r.mConstraints.Reserve(arr.size()); }
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const std::string name = "constraints[" + std::to_string(i) + "]";
				EntryContext ctx{ fileName, name.c_str() };
				const Json::Value& c = arr[i];
				if (!c.isObject()) { return Fail(r, ctx, "constraint must be an object"); }
				for (const std::string& n : c.getMemberNames())
				{
					if (n != "a" && n != "op" && n != "b") { return Fail(r, ctx, (std::string("unknown key '") + n + "'").c_str()); }
				}
				Constraint con;
				if (!AssignRequired(r, ctx, con.mA, c, "a")) { return false; }
				const char* op = nullptr;
				if (!RequireString(r, ctx, c, "op", op)) { return false; }
				if (!ParseOp(op, con.mOp)) { return Fail(r, ctx, (std::string("unknown op '") + op + "' (field 'op')").c_str()); }
				if (!AssignRequired(r, ctx, con.mB, c, "b")) { return false; }
				const std::string operands[2] = { con.mA.AsCStr(), con.mB.AsCStr() };
				for (const std::string& operand : operands)
				{
					bool found = false;
					const bool numeric = OperandIsNumeric(r, operand, found);
					if (!found) { return Fail(r, ctx, (std::string("constraint names unknown key '") + operand + "'").c_str()); }
					if (!numeric) { return Fail(r, ctx, (std::string("constraint key '") + operand + "' is not numeric").c_str()); }
				}
				r.mConstraints.Add(con);
			}
			return true;
		}

		bool ReadEntry(LoadResult& r, const char* fileName, const Json::Value& e, ValueInfo& info, std::set<std::string>& warned)
		{
			EntryContext ctx{ fileName, "" };
			if (!e.isObject()) { return Fail(r, ctx, "entry must be an object"); }
			if (!e.isMember("key") || !e["key"].isString()) { return Fail(r, ctx, "entry is missing string 'key'"); }
			const char* key = e["key"].asCString();
			String128 entryName;
			SetText(entryName, (strlen(key) > 127) ? "<key too long>" : key);
			ctx.mEntry = entryName.AsCStr();

			const Json::Value::Members names = e.getMemberNames();
			for (const std::string& n : names)
			{
				if (!IsKnownKey(kEntryKeys, n.c_str())) { return Fail(r, ctx, (std::string("unknown key '") + n + "'").c_str()); }
			}

			if (!AssignString(r, ctx, info.mKey, key, "key")) { return false; }
			info.mFor = r.mFor;
			if (!AssignString(r, ctx, info.mSourceFile, fileName, "sourceFile")) { return false; }

			const char* s = nullptr;
			if (!RequireString(r, ctx, e, "kind", s)) { return false; }
			if (!ParseKind(s, info.mKind)) { return Fail(r, ctx, "unknown 'kind'"); }
			if (!AssignRequired(r, ctx, info.mLabel, e, "label")) { return false; }
			if (!AssignRequired(r, ctx, info.mDescription, e, "description")) { return false; }
			if (!RequireString(r, ctx, e, "type", s)) { return false; }
			if (!ParseType(s, info.mType)) { return Fail(r, ctx, "unknown 'type'"); }

			if (!AssignOptional(r, ctx, info.mUnit, e, "unit")) { return false; }
			if (!AssignOptional(r, ctx, info.mHint, e, "hint")) { return false; }
			if (!AssignOptional(r, ctx, info.mSource, e, "source")) { return false; }

			if (!ReadNumber(r, ctx, e, "min", info.mHasMin, info.mMin)) { return false; }
			if (!ReadNumber(r, ctx, e, "max", info.mHasMax, info.mMax)) { return false; }
			if (e.isMember("default"))
			{
				const Json::Value& d = e["default"];
				if (d.isObject() && info.mType == ValueType::Map)
				{
					if (!ReadMapDefault(r, ctx, info, d)) { return false; }
				}
				else if (d.isBool())		{ info.mHasDefault = true; info.mDefaultNumber = d.asBool() ? 1.0 : 0.0; }
				else if (d.isNumeric())	{ info.mHasDefault = true; info.mDefaultNumber = d.asDouble(); }
				else if (d.isString())
				{
					info.mHasDefault = true;
					if (!AssignString(r, ctx, info.mDefaultString, d.asCString(), "default")) { return false; }
				}
			}

			if (!ReadBool(r, ctx, e, "unique", info.mUnique)) { return false; }
			if (!ReadBool(r, ctx, e, "required", info.mRequired)) { return false; }
			if (!ReadOpenBlock(r, ctx, warned, e, "ui", kUiKeys, info.mUi)) { return false; }
			if (!ReadOpenBlock(r, ctx, warned, e, "history", kHistoryKeys, info.mHistory)) { return false; }

			if (!ReadBands(r, ctx, info, e)) { return false; }
			if (!ReadStringList(r, ctx, info.mAffects, e, "affects")) { return false; }
			if (!ReadStringList(r, ctx, info.mRelated, e, "related")) { return false; }
			if (!ReadTypeSpecific(r, ctx, info, e)) { return false; }
			if (!ReadEnumValues(r, ctx, info, e)) { return false; }
			if (!ReadPayload(r, ctx, info, e)) { return false; }
			if (!ReadVocabulary(r, ctx, info, e)) { return false; }

			info.mPathSegments.Reserve(16);
			if (!SplitRecordPath(key, info.mPathSegments)) { return Fail(r, ctx, "key has a path segment that is too long or has too many segments"); }
			return true;
		}
	}

	bool SplitRecordPath(const char* key, ValueList<String64>& outSegments)
	{
		if (key == nullptr) { return true; }	// a null key is an empty key
		std::string seg;
		auto flush = [&]() -> bool
		{
			if (seg.empty()) { return true; }
			if (seg.size() > String64::Capacity() - 1 || outSegments.Size() >= outSegments.Capacity()) { return false; }
			String64 s;
			SetText(s, seg.c_str());
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
		// A file name that does not fit mFile / mSourceFile (String256) must neither fail a valid file nor overflow.
		// Like RampLoader, such a name is replaced by a fixed placeholder; a null name is treated as empty.
		if (fileName == nullptr) { fileName = ""; }
		if (strlen(fileName) > String256::Capacity() - 1) { fileName = "<file name too long>"; }
		EntryContext fileCtx{ fileName, "" };
		
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
			Fail(r, fileCtx, (std::string("unparsable JSON: ") + parseError).c_str());
			return r;
		}
		if (!root.isObject()) { Fail(r, fileCtx, "root must be an object"); return r; }

		if (!root.isMember("format") || !root["format"].isString() || root["format"].asString() != "dia.value_schema")
		{
			Fail(r, fileCtx, "'format' must be \"dia.value_schema\"");
			return r;
		}
		if (!root.isMember("version") || !root["version"].isInt() || root["version"].asInt() != 1)
		{
			Fail(r, fileCtx, "'version' must be the integer 1");
			return r;
		}
		if (!root.isMember("for") || !root["for"].isString())
		{
			Fail(r, fileCtx, "missing string 'for'");
			return r;
		}
		if (!root.isMember("entries") || !root["entries"].isArray()) { Fail(r, fileCtx, "missing 'entries' array"); return r; }

		const Json::Value::Members fileKeys = root.getMemberNames();
		for (const std::string& n : fileKeys)
		{
			if (!IsKnownKey(kFileKeys, n.c_str()))
			{
				Fail(r, fileCtx, (std::string("unknown file-level key '") + n + "'").c_str());
				return r;
			}
		}

		if (!AssignString(r, fileCtx, r.mFor, root["for"].asCString(), "for")) { return r; }

		std::set<std::string> warned;
		const Json::Value& entries = root["entries"];
		if (entries.size() > 0) { r.mEntries.Reserve(entries.size()); }
		for (Json::ArrayIndex i = 0; i < entries.size(); ++i)
		{
			ValueInfo info;
			if (!ReadEntry(r, fileName, entries[i], info, warned)) { return r; }
			r.mEntries.Add(info);
		}
		if (root.isMember("constraints"))
		{
			if (!ReadConstraints(r, fileName, root["constraints"])) { return r; }
		}
		r.mOk = true;
		return r;
	}
}
