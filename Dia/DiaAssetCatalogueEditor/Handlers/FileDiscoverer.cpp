#include "DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h"

#include <DiaLogger/DiaLog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <ctype.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			void FileDiscoverer::Discover(
				const char* rootPath,
				const Dia::AssetCatalogue::AssetTypeRegistry& typeRegistry,
				const Dia::AssetCatalogue::AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles>& outFiles)
			{
				if (!rootPath || rootPath[0] == '\0')
					return;
				ScanDirectory(rootPath, rootPath, typeRegistry, registry, outFiles);
			}

			void FileDiscoverer::ScanDirectory(
				const char* dir,
				const char* rootPath,
				const Dia::AssetCatalogue::AssetTypeRegistry& typeRegistry,
				const Dia::AssetCatalogue::AssetRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles>& outFiles)
			{
				if (outFiles.IsFull())
					return;

				char searchPath[512];
				_snprintf_s(searchPath, sizeof(searchPath), _TRUNCATE, "%s\\*", dir);

				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA(searchPath, &findData);
				if (hFind == INVALID_HANDLE_VALUE)
					return;

				do
				{
					const char* name = findData.cFileName;
					if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
						continue;

					char fullPath[512];
					_snprintf_s(fullPath, sizeof(fullPath), _TRUNCATE, "%s\\%s", dir, name);

					if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						// Try to match folder itself as a folder asset type (pattern e.g. "*.folder")
						const AssetTypeDescriptor* desc = typeRegistry.FindByFileName(name);
						if (desc && !outFiles.IsFull())
						{
							char suggestedId[256] = {};
							if (GenerateDefaultId(rootPath, fullPath, desc->mTypeId.AsChar(), suggestedId, sizeof(suggestedId)))
							{
								if (!registry.FindById(Dia::Core::StringCRC(suggestedId)))
								{
									DiscoveredFile df;
									strncpy_s(df.mFullPath, sizeof(df.mFullPath), fullPath, _TRUNCATE);
									strncpy_s(df.mSuggestedType, sizeof(df.mSuggestedType), desc->mTypeId.AsChar(), _TRUNCATE);
									strncpy_s(df.mSuggestedId, sizeof(df.mSuggestedId), suggestedId, _TRUNCATE);
									outFiles.Add(df);
								}
							}
						}

						ScanDirectory(fullPath, rootPath, typeRegistry, registry, outFiles);
					}
					else
					{
						const AssetTypeDescriptor* desc = typeRegistry.FindByFileName(name);
						if (desc && !outFiles.IsFull())
						{
							char suggestedId[256] = {};
							if (GenerateDefaultId(rootPath, fullPath, desc->mTypeId.AsChar(), suggestedId, sizeof(suggestedId)))
							{
								if (!registry.FindById(Dia::Core::StringCRC(suggestedId)))
								{
									DiscoveredFile df;
									strncpy_s(df.mFullPath, sizeof(df.mFullPath), fullPath, _TRUNCATE);
									strncpy_s(df.mSuggestedType, sizeof(df.mSuggestedType), desc->mTypeId.AsChar(), _TRUNCATE);
									strncpy_s(df.mSuggestedId, sizeof(df.mSuggestedId), suggestedId, _TRUNCATE);
									outFiles.Add(df);
								}
							}
						}
					}
				}
				while (FindNextFileA(hFind, &findData) && !outFiles.IsFull());

				FindClose(hFind);
			}

			bool FileDiscoverer::GenerateDefaultId(
				const char* rootPath,
				const char* fullPath,
				const char* typeId,
				char* outId,
				unsigned int outIdCapacity)
			{
				if (!rootPath || !fullPath || !typeId || !outId || outIdCapacity == 0)
					return false;

				size_t rootLen = strlen(rootPath);
				size_t fullLen = strlen(fullPath);

				if (fullLen <= rootLen)
					return false;

				const char* relative = fullPath + rootLen;
				if (relative[0] == '\\' || relative[0] == '/')
					++relative;

				if (relative[0] == '\0')
					return false;

				// Lowercase, separators and dots → '_'
				char name[256] = {};
				unsigned int nameLen = 0;

				for (const char* p = relative; *p != '\0' && nameLen < sizeof(name) - 1; ++p)
				{
					char c = *p;
					if (c == '\\' || c == '/' || c == '.')
						c = '_';
					else
						c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

					name[nameLen++] = c;
				}
				name[nameLen] = '\0';

				// Trim trailing underscores from extension artifacts
				while (nameLen > 0 && name[nameLen - 1] == '_')
					name[--nameLen] = '\0';

				if (nameLen == 0)
					return false;

				_snprintf_s(outId, outIdCapacity, _TRUNCATE, "%s.%s", typeId, name);
				return outId[0] != '\0';
			}

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
