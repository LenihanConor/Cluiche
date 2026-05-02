#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Rig2D
	{
		static const unsigned int kMaxMetadataPerBone = 8;

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

			static MetadataValue FromBool(bool v) { MetadataValue m; m.type = kBool; m.boolVal = v; return m; }
			static MetadataValue FromInt(int v) { MetadataValue m; m.type = kInt; m.intVal = v; return m; }
			static MetadataValue FromFloat(float v) { MetadataValue m; m.type = kFloat; m.floatVal = v; return m; }
			static MetadataValue FromString(const char* v) { MetadataValue m; m.type = kString; m.stringVal = Dia::Core::StringCRC(v); return m; }
		};

		struct MetadataEntry
		{
			Dia::Core::StringCRC	key;
			MetadataValue			value;
		};

		struct Bone
		{
			Dia::Core::StringCRC	name;
			int						parentIndex = -1;
			Dia::Maths::Vector2D	localPosition = Dia::Maths::Vector2D(0.0f, 0.0f);
			float					localRotation = 0.0f;
			Dia::Maths::Vector2D	localScale = Dia::Maths::Vector2D(1.0f, 1.0f);
			float					length = 0.0f;

			Dia::Core::Containers::DynamicArrayC<MetadataEntry, kMaxMetadataPerBone> metadata;

			void SetMetadata(const Dia::Core::StringCRC& key, const MetadataValue& value);
			const MetadataValue* FindMetadata(const Dia::Core::StringCRC& key) const;
		};
	}
}
