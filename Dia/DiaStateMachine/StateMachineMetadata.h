#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace StateMachine
	{
		static const unsigned int kMaxMetadataEntries = 16;

		struct MetadataValue
		{
			enum Type : unsigned char
			{
				kBool,
				kInt,
				kFloat,
				kString
			};

			Type type = kBool;

			union
			{
				bool	boolVal;
				int		intVal;
				float	floatVal;
			};

			Dia::Core::StringCRC stringVal;

			static MetadataValue FromBool(bool v)			{ MetadataValue m; m.type = kBool;   m.boolVal  = v;                          return m; }
			static MetadataValue FromInt(int v)				{ MetadataValue m; m.type = kInt;    m.intVal   = v;                          return m; }
			static MetadataValue FromFloat(float v)			{ MetadataValue m; m.type = kFloat;  m.floatVal = v;                          return m; }
			static MetadataValue FromString(const char* v)	{ MetadataValue m; m.type = kString; m.stringVal = Dia::Core::StringCRC(v);   return m; }
		};

		struct MetadataEntry
		{
			Dia::Core::StringCRC	key;
			MetadataValue			value;
		};

		using MetadataArray = Dia::Core::Containers::DynamicArrayC<MetadataEntry, kMaxMetadataEntries>;

		inline void SetMetadata(MetadataArray& arr, const Dia::Core::StringCRC& key, const MetadataValue& value)
		{
			for (unsigned int i = 0; i < arr.Size(); ++i)
			{
				if (arr[i].key == key)
				{
					arr[i].value = value;
					return;
				}
			}
			MetadataEntry entry;
			entry.key = key;
			entry.value = value;
			arr.Add(entry);
		}

		inline const MetadataValue* FindMetadata(const MetadataArray& arr, const Dia::Core::StringCRC& key)
		{
			for (unsigned int i = 0; i < arr.Size(); ++i)
			{
				if (arr[i].key == key)
					return &arr[i].value;
			}
			return nullptr;
		}
	}
}
