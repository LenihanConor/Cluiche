#pragma once

#include "DiaSerializer/MetadataValue.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <string>

namespace Dia
{
	namespace Serializer
	{
		template<unsigned int N>
		inline void WriteMetadataToJson(const Dia::Core::Containers::DynamicArrayC<MetadataEntry, N>& arr, Json::Value& outObject)
		{
			if (arr.Size() == 0)
				return;

			Json::Value meta;
			for (unsigned int i = 0; i < arr.Size(); ++i)
			{
				const MetadataEntry& entry = arr[i];
				const char* key = entry.key.AsChar();
				switch (entry.value.type)
				{
				case MetadataValue::kBool:   meta[key] = entry.value.boolVal;             break;
				case MetadataValue::kInt:    meta[key] = entry.value.intVal;              break;
				case MetadataValue::kFloat:  meta[key] = entry.value.floatVal;            break;
				case MetadataValue::kString: meta[key] = entry.value.stringVal.AsChar();  break;
				}
			}
			outObject["metadata"] = meta;
		}

		template<unsigned int N>
		inline bool ReadMetadataFromJson(const Json::Value& root, Dia::Core::Containers::DynamicArrayC<MetadataEntry, N>& outArr)
		{
			if (!root.isMember("metadata") || !root["metadata"].isObject())
				return true;

			const Json::Value& meta = root["metadata"];
			auto members = meta.getMemberNames();

			for (unsigned int m = 0; m < static_cast<unsigned int>(members.size()); ++m)
			{
				const std::string& key = members[m];
				const Json::Value& val = meta[key];

				MetadataEntry entry;
				entry.key = Dia::Core::StringCRC(key.c_str());

				if (val.isBool())
					entry.value = MetadataValue::FromBool(val.asBool());
				else if (val.isInt())
					entry.value = MetadataValue::FromInt(val.asInt());
				else if (val.isDouble())
					entry.value = MetadataValue::FromFloat(val.asFloat());
				else if (val.isString())
					entry.value = MetadataValue::FromString(val.asCString());
				else
				{
					DIA_LOG_WARNING("Serializer", "ReadMetadataFromJson: key '%s' has unsupported type, skipping", key.c_str());
					continue;
				}

				outArr.Add(entry);
			}
			return true;
		}
	}
}
