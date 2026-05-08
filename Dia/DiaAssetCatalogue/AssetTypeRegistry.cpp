#include "DiaAssetCatalogue/AssetTypeRegistry.h"

#include "DiaCore/CRC/CRC.h"

#include <math.h>
#include <string.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// AssetTypeHashFunctor
		//------------------------------------------------------------------------------------
		unsigned int AssetTypeRegistry::AssetTypeHashFunctor::GetHashIndex(Key key, const TableData* tableData) const
		{
			static const unsigned int sTranslationToTableSpace = Dia::Core::CRC::MaxCRC() / AssetTypeRegistry::kMaxTableSize;

			DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");

			unsigned int index = static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) / static_cast<float>(sTranslationToTableSpace)));
			return index;
		}

		//------------------------------------------------------------------------------------
		// AssetTypeRegistry
		//------------------------------------------------------------------------------------
		AssetTypeRegistry::AssetTypeRegistry()
			: mTypeMap()
			, mCount(0)
		{}

		bool AssetTypeRegistry::Register(const AssetTypeDescriptor& descriptor)
		{
			if (mTypeMap.ContainsKey(descriptor.mTypeId))
			{
				return false;
			}

			mTypeMap.Add(descriptor.mTypeId, descriptor);
			++mCount;
			return true;
		}

		const AssetTypeDescriptor* AssetTypeRegistry::FindByTypeId(const Dia::Core::StringCRC& typeId) const
		{
			return mTypeMap.TryGetItemConst(typeId);
		}

		const AssetTypeDescriptor* AssetTypeRegistry::FindByFilePath(const Dia::Core::FilePath& path) const
		{
			// Extract the filename from the FilePath.
			// FilePath::GetFileName() returns the FileName (String32).
			const char* filename = path.GetFileName().AsCStr();

			// Iterate over all registered descriptors and check suffix match.
			// Pattern format: "*.suffix" or "*.suffix.ext"
			// Matching rule: filename ends with the pattern's suffix part (everything after the first '*').
			for (unsigned int i = 0; i < mCount; ++i)
			{
				const AssetTypeDescriptor& desc = mTypeMap.GetItemByIndexConst(i);

				const char* pattern = desc.mFilePattern.AsCStr();

				// The pattern must start with '*' followed by a suffix.
				if (pattern == nullptr || pattern[0] == '\0')
				{
					continue;
				}

				// Skip the leading '*'; suffix is everything after it.
				const char* suffix = (pattern[0] == '*') ? pattern + 1 : pattern;

				if (suffix[0] == '\0')
				{
					continue;
				}

				// Check if filename ends with the suffix.
				size_t filenameLen = strlen(filename);
				size_t suffixLen   = strlen(suffix);

				if (filenameLen >= suffixLen)
				{
					const char* filenameSuffix = filename + (filenameLen - suffixLen);
					if (strcmp(filenameSuffix, suffix) == 0)
					{
						return &desc;
					}
				}
			}

			return nullptr;
		}

		const AssetTypeDescriptor* AssetTypeRegistry::FindByFileName(const char* filename) const
		{
			if (!filename || filename[0] == '\0')
				return nullptr;

			size_t filenameLen = strlen(filename);

			for (unsigned int i = 0; i < mCount; ++i)
			{
				const AssetTypeDescriptor& desc = mTypeMap.GetItemByIndexConst(i);
				const char* pattern = desc.mFilePattern.AsCStr();

				if (!pattern || pattern[0] == '\0')
					continue;

				const char* suffix = (pattern[0] == '*') ? pattern + 1 : pattern;
				if (suffix[0] == '\0')
					continue;

				size_t suffixLen = strlen(suffix);
				if (filenameLen >= suffixLen)
				{
					const char* filenameSuffix = filename + (filenameLen - suffixLen);
					if (strcmp(filenameSuffix, suffix) == 0)
						return &desc;
				}
			}
			return nullptr;
		}

		unsigned int AssetTypeRegistry::GetCount() const
		{
			return mCount;
		}

	} // namespace AssetCatalogue
} // namespace Dia
