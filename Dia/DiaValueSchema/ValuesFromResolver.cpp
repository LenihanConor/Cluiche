#include <DiaValueSchema/ValuesFromResolver.h>
#include <DiaValueSchema/CategoricalColours.h>

#include <filesystem>
#include <fstream>
#include <iterator>

namespace Dia::ValueSchema
{
	namespace
	{
		bool ReadJson(const std::string& path, Json::Value& out)
		{
			std::ifstream in(std::filesystem::path(path), std::ios::binary);
			if (!in) { return false; }
			const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			Json::Reader reader;
			return reader.parse(text, out) && out.isObject();
		}

		// Reads record[field] as a string into dst. Empty field name = not configured (skipped).
		template<class T>
		bool ReadField(const Json::Value& record, const char* field, bool required, T& dst, unsigned int index, std::string& reason)
		{
			if (field[0] == '\0') { return true; }
			if (!record.isMember(field) || !record[field].isString())
			{
				if (!required && !record.isMember(field)) { return true; }
				reason = std::string("record ") + std::to_string(index) + " has no string '" + field + "'";
				return false;
			}
			const std::string text = record[field].asString();
			if (text.size() > T::Capacity() - 1)
			{
				reason = std::string("record ") + std::to_string(index) + " '" + field + "' is too long";
				return false;
			}
			SetText(dst, text.c_str());
			return true;
		}
	}

	bool ResolveValuesFrom(ValueInfo& entry, const std::vector<std::string>& candidateDataFiles, std::string& reason)
	{
		const ValuesFrom& vf = entry.mValuesFrom;
		Json::Value doc;
		bool found = false;
		for (const std::string& path : candidateDataFiles)
		{
			Json::Value candidate;
			if (!ReadJson(path, candidate)) { continue; }
			if (candidate["format"].isString() && candidate["format"].asString() == vf.mFormat.AsCStr())
			{
				doc = candidate;
				found = true;
				break;
			}
		}
		if (!found)
		{
			reason = std::string("no data file with format '") + vf.mFormat.AsCStr() + "' found next to a schema sidecar";
			return false;
		}

		const Json::Value& records = doc[vf.mRecords.AsCStr()];
		if (!records.isArray())
		{
			reason = std::string("'") + vf.mRecords.AsCStr() + "' in format '" + vf.mFormat.AsCStr() + "' is not an array";
			return false;
		}

		ValueList<EnumValue> values;
		values.Reserve(records.size() > 0 ? records.size() : 1);
		for (Json::ArrayIndex i = 0; i < records.size(); ++i)
		{
			const Json::Value& rec = records[i];
			if (!rec.isObject()) { reason = std::string("record ") + std::to_string(i) + " is not an object"; return false; }
			EnumValue ev;
			if (!ReadField(rec, vf.mId.AsCStr(), true, ev.mId, i, reason)) { return false; }
			if (ev.mId.Length() == 0) { reason = std::string("record ") + std::to_string(i) + " has an empty id"; return false; }
			if (!ReadField(rec, vf.mLabel.AsCStr(), true, ev.mLabel, i, reason)) { return false; }
			if (!ReadField(rec, vf.mDescription.AsCStr(), true, ev.mDescription, i, reason)) { return false; }
			if (vf.mColour.Length() > 0)
			{
				if (!ReadField(rec, vf.mColour.AsCStr(), true, ev.mColour, i, reason)) { return false; }
				ev.mHasColour = true;
			}
			else
			{
				SetText(ev.mColour, GetCategoricalColour(i));
				ev.mHasColour = false;
			}
			values.Add(ev);
		}
		entry.mEnumValues = values;
		return true;
	}
}
