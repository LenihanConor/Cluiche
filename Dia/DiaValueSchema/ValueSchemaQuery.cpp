#include <DiaValueSchema/ValueSchemaQuery.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>

#include <string>
#include <string.h>
#include <stdio.h>

namespace Dia::ValueSchema
{
	namespace
	{
		const char* KindName(ValueKind kind)
		{
			switch (kind)
			{
			case ValueKind::Parameter:		return "parameter";
			case ValueKind::Field:			return "field";
			case ValueKind::Metric:			return "metric";
			case ValueKind::Arg:			return "arg";
			case ValueKind::Command:		return "command";
			case ValueKind::Event:			return "event";
			case ValueKind::ComponentField:	return "component_field";
			}
			return "parameter";
		}

		const char* TypeName(ValueType type)
		{
			switch (type)
			{
			case ValueType::Float:		return "float";
			case ValueType::Int:		return "int";
			case ValueType::Bool:		return "bool";
			case ValueType::Enum:		return "enum";
			case ValueType::String:		return "string";
			case ValueType::Ref:		return "ref";
			case ValueType::RefList:	return "ref[]";
			case ValueType::Map:		return "map";
			}
			return "float";
		}

		std::string HexColour(uint32_t rgba8)
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "#%02x%02x%02x", (rgba8 >> 24) & 0xFF, (rgba8 >> 16) & 0xFF, (rgba8 >> 8) & 0xFF);
			return buf;
		}

		void SetIfNotEmpty(Json::Value& obj, const char* name, const char* text)
		{
			if (text != nullptr && text[0] != '\0') { obj[name] = text; }
		}

		template<class List>
		void SetStringList(Json::Value& obj, const char* name, const List& list)
		{
			if (list.Size() == 0) { return; }
			Json::Value arr(Json::arrayValue);
			for (unsigned int i = 0; i < list.Size(); ++i) { arr.append(list[i].AsCStr()); }
			obj[name] = arr;
		}
	}

	Json::Value ValueInfoToJson(const ValueInfo& info)
	{
		Json::Value e(Json::objectValue);
		e["for"] = info.mFor.AsCStr();
		e["key"] = info.mKey.AsCStr();
		e["kind"] = KindName(info.mKind);
		e["label"] = info.mLabel.AsCStr();
		e["description"] = info.mDescription.AsCStr();
		SetIfNotEmpty(e, "hint", info.mHint.AsCStr());
		SetIfNotEmpty(e, "unit", info.mUnit.AsCStr());
		e["type"] = TypeName(info.mType);
		if (info.mHasMin) { e["min"] = info.mMin; }
		if (info.mHasMax) { e["max"] = info.mMax; }
		if (info.mHasDefault)
		{
			switch (info.mType)
			{
			case ValueType::Bool:	e["default"] = info.mDefaultNumber != 0.0; break;
			case ValueType::Float:
			case ValueType::Int:	e["default"] = info.mDefaultNumber; break;
			default:				e["default"] = info.mDefaultString.AsCStr(); break;
			}
		}

		if (info.mEnumValues.Size() > 0)
		{
			Json::Value values(Json::arrayValue);
			for (unsigned int i = 0; i < info.mEnumValues.Size(); ++i)
			{
				const EnumValue& v = info.mEnumValues[i];
				Json::Value item(Json::objectValue);
				item["id"] = v.mId.AsCStr();
				item["label"] = v.mLabel.AsCStr();
				item["description"] = v.mDescription.AsCStr();
				if (v.mHasColour) { item["colour"] = v.mColour.AsCStr(); }
				values.append(item);
			}
			e["values"] = values;
		}

		SetStringList(e, "affects", info.mAffects);
		SetStringList(e, "related", info.mRelated);
		SetIfNotEmpty(e, "source", info.mSource.AsCStr());
		if (info.mUnique) { e["unique"] = true; }
		if (info.mRequired) { e["required"] = true; }
		SetIfNotEmpty(e, "ref", info.mRef.AsCStr());
		if (info.mType == ValueType::Map)
		{
			SetIfNotEmpty(e, "keyRef", info.mMapKeyRef.AsCStr());
			e["valueType"] = TypeName(info.mMapValueType);
		}
		if (info.mUi.isObject() && !info.mUi.empty()) { e["ui"] = info.mUi; }
		if (info.mHistory.isObject() && !info.mHistory.empty()) { e["history"] = info.mHistory; }

		if (info.mPayload.Size() > 0)
		{
			Json::Value payload(Json::arrayValue);
			for (unsigned int i = 0; i < info.mPayload.Size(); ++i)
			{
				const PayloadField& f = info.mPayload[i];
				Json::Value item(Json::objectValue);
				item["name"] = f.mName.AsCStr();
				item["type"] = TypeName(f.mType);
				item["description"] = f.mDescription.AsCStr();
				SetIfNotEmpty(item, "unit", f.mUnit.AsCStr());
				payload.append(item);
			}
			e["payload"] = payload;
		}
		if (info.mVocabulary.Size() > 0)
		{
			Json::Value vocab(Json::arrayValue);
			for (unsigned int i = 0; i < info.mVocabulary.Size(); ++i)
			{
				const VocabularyEntry& v = info.mVocabulary[i];
				Json::Value item(Json::objectValue);
				item["id"] = v.mId.AsCStr();
				SetIfNotEmpty(item, "label", v.mLabel.AsCStr());
				item["description"] = v.mDescription.AsCStr();
				SetIfNotEmpty(item, "unit", v.mUnit.AsCStr());
				vocab.append(item);
			}
			e["vocabulary"] = vocab;
		}
		return e;
	}

	Json::Value RampToJson(const RampData& ramp)
	{
		Json::Value r(Json::objectValue);
		r["id"] = ramp.mName.AsCStr();
		if (ramp.mKind == RampKind::Continuous)
		{
			r["kind"] = "continuous";
			Json::Value stops(Json::arrayValue);
			for (unsigned i = 0; i < ramp.mStopCount; ++i)
			{
				Json::Value s(Json::objectValue);
				s["t"] = ramp.mStops[i].mT;
				s["colour"] = HexColour(ramp.mStops[i].mRgba8);
				stops.append(s);
			}
			r["stops"] = stops;
		}
		else
		{
			r["kind"] = "categorical";
			Json::Value colours(Json::arrayValue);
			for (unsigned i = 0; i < ramp.mColourCount; ++i) { colours.append(HexColour(ramp.mColours[i])); }
			r["colours"] = colours;
		}
		return r;
	}

	ValueSchemaQueryHandler::ValueSchemaQueryHandler(const ValueSchemaService& service)
		: mService(service)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mQueriesServed = metrics.RegisterCounter(Dia::Core::StringCRC("valueschema.queries_served"));
		mQueryUnresolved = metrics.RegisterCounter(Dia::Core::StringCRC("valueschema.query_unresolved"));
	}

	Json::Value ValueSchemaQueryHandler::Handle(const Json::Value& args) const
	{
		DIA_TRACE_ZONE("ValueSchema.Query", Dia::Observation::Trace::Category::kDiaAssetRuntime);
		DIA_PROFILE_SCOPE("ValueSchema.Query", Dia::Observation::Profile::Category::kDiaAssetRuntime);

		std::string prefix;
		if (args.isObject() && args["prefix"].isString()) { prefix = args["prefix"].asString(); }

		Json::Value entries(Json::arrayValue);
		const unsigned int count = mService.Count();
		for (unsigned int i = 0; i < count; ++i)
		{
			const ValueInfo* info = mService.At(i);
			if (info == nullptr) { continue; }
			if (!prefix.empty() && strncmp(info->mFullKey.AsCStr(), prefix.c_str(), prefix.size()) != 0) { continue; }
			entries.append(ValueInfoToJson(*info));
		}

		if (mQueriesServed) { mQueriesServed->Inc(); }
		if (entries.empty() && mQueryUnresolved) { mQueryUnresolved->Inc(); }
		DIA_LOG_DEBUG("VSC", "valueschema.get prefix='%s' matched %u entries (version %u)", prefix.c_str(), static_cast<unsigned int>(entries.size()), mService.Version());

		// Ramps are global and small: always the full set, never filtered by prefix.
		Json::Value ramps(Json::arrayValue);
		const unsigned int rampCount = mService.RampCount();
		for (unsigned int i = 0; i < rampCount; ++i)
		{
			const RampData* ramp = mService.RampAt(i);
			if (ramp != nullptr) { ramps.append(RampToJson(*ramp)); }
		}

		Json::Value result(Json::objectValue);
		result["ramps"] = ramps;
		result["version"] = mService.Version();
		result["entries"] = entries;
		return result;
	}
}
