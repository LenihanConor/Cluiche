#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaSerializer/MetadataValue.h>

namespace Dia
{
	namespace Rig2D
	{
		using MetadataValue = Dia::Serializer::MetadataValue;
		using MetadataEntry = Dia::Serializer::MetadataEntry;

		static const unsigned int kMaxMetadataPerBone = 8;

		struct Bone
		{
			Dia::Core::StringCRC	name;
			int						parentIndex = -1;
			Dia::Maths::Vector2D	localPosition = Dia::Maths::Vector2D(0.0f, 0.0f);
			float					localRotation = 0.0f;
			Dia::Maths::Vector2D	localScale = Dia::Maths::Vector2D(1.0f, 1.0f);
			float					length = 0.0f;

			Dia::Core::Containers::DynamicArrayC<Dia::Serializer::MetadataEntry, kMaxMetadataPerBone> metadata;

			void SetMetadata(const Dia::Core::StringCRC& key, const Dia::Serializer::MetadataValue& value);
			const Dia::Serializer::MetadataValue* FindMetadata(const Dia::Core::StringCRC& key) const;
		};
	}
}
