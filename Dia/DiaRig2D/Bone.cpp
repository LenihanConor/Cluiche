#include "Bone.h"

namespace Dia
{
	namespace Rig2D
	{
		void Bone::SetMetadata(const Dia::Core::StringCRC& key, const MetadataValue& value)
		{
			for (unsigned int i = 0; i < metadata.Size(); ++i)
			{
				if (metadata[i].key == key)
				{
					metadata[i].value = value;
					return;
				}
			}

			MetadataEntry entry;
			entry.key = key;
			entry.value = value;
			metadata.Add(entry);
		}

		const MetadataValue* Bone::FindMetadata(const Dia::Core::StringCRC& key) const
		{
			for (unsigned int i = 0; i < metadata.Size(); ++i)
			{
				if (metadata[i].key == key)
				{
					return &metadata[i].value;
				}
			}
			return nullptr;
		}
	}
}
