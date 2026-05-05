#include "DiaAssetCatalogue/ContentHasher.h"

#include "DiaCore/CRC/CRC.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		//------------------------------------------------------------------------------------
		// ComputeHash
		//------------------------------------------------------------------------------------
		unsigned int ContentHasher::ComputeHash(const char* filePath) const
		{
			if (filePath == nullptr || filePath[0] == '\0')
			{
				return 0;
			}

			FILE* f = nullptr;
#if defined(_MSC_VER)
			fopen_s(&f, filePath, "rb");
#else
			f = fopen(filePath, "rb");
#endif
			if (!f)
			{
				return 0;
			}

			fseek(f, 0, SEEK_END);
			long fileSize = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (fileSize <= 0)
			{
				fclose(f);
				return 0;
			}

			// Read file into buffer
			static const unsigned int kMaxFileSize = 4 * 1024 * 1024; // 4MB max
			if (static_cast<unsigned long>(fileSize) > kMaxFileSize)
			{
				fclose(f);
				return 0;
			}

			std::vector<char> buffer(static_cast<size_t>(fileSize));
			size_t bytesRead = fread(buffer.data(), 1, static_cast<size_t>(fileSize), f);
			fclose(f);

			if (bytesRead != static_cast<size_t>(fileSize))
			{
				return 0;
			}

			// Compute CRC32 using DiaCore CRC
			Dia::Core::CRC crc(static_cast<const void*>(buffer.data()), bytesRead);
			return crc.Value();
		}

		//------------------------------------------------------------------------------------
		// Helper: Recursively enumerate files in a directory
		//------------------------------------------------------------------------------------
		namespace
		{
			void EnumerateFilesRecursive(const std::wstring& basePath,
				const std::wstring& relativePath,
				std::vector<std::string>& outRelativePaths)
			{
				std::wstring searchPath = basePath;
				if (!relativePath.empty())
				{
					searchPath += L"\\" + relativePath;
				}
				searchPath += L"\\*";

				WIN32_FIND_DATAW findData;
				HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
				if (hFind == INVALID_HANDLE_VALUE)
				{
					return;
				}

				do
				{
					// Skip . and ..
					if (wcscmp(findData.cFileName, L".") == 0 ||
						wcscmp(findData.cFileName, L"..") == 0)
					{
						continue;
					}

					std::wstring childRelative = relativePath;
					if (!childRelative.empty())
					{
						childRelative += L"\\";
					}
					childRelative += findData.cFileName;

					if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						// Recurse into subdirectory
						EnumerateFilesRecursive(basePath, childRelative, outRelativePaths);
					}
					else
					{
						// Convert wide string relative path to narrow string
						std::string narrowPath;
						narrowPath.resize(childRelative.size());
						for (size_t i = 0; i < childRelative.size(); ++i)
						{
							narrowPath[i] = static_cast<char>(childRelative[i]);
						}
						outRelativePaths.push_back(narrowPath);
					}
				} while (FindNextFileW(hFind, &findData));

				FindClose(hFind);
			}
		}

		//------------------------------------------------------------------------------------
		// ComputeDirectoryHash
		//------------------------------------------------------------------------------------
		unsigned int ContentHasher::ComputeDirectoryHash(const char* folderPath) const
		{
			if (folderPath == nullptr || folderPath[0] == '\0')
			{
				return 0;
			}

			// Convert to wide string for Win32 API
			std::wstring wideFolderPath;
			size_t pathLen = strlen(folderPath);
			wideFolderPath.resize(pathLen);
			for (size_t i = 0; i < pathLen; ++i)
			{
				wideFolderPath[i] = static_cast<wchar_t>(folderPath[i]);
			}

			// Enumerate all files recursively
			std::vector<std::string> relativePaths;
			EnumerateFilesRecursive(wideFolderPath, L"", relativePaths);

			if (relativePaths.empty())
			{
				return 0;
			}

			// Sort by relative path for determinism
			std::sort(relativePaths.begin(), relativePaths.end());

			// Compute CRC32 of each file, collect as bytes for combined hash
			std::vector<unsigned char> combinedData;
			combinedData.reserve(relativePaths.size() * 4);

			for (const std::string& relPath : relativePaths)
			{
				// Build full path
				std::string fullPath(folderPath);
				fullPath += "\\";
				fullPath += relPath;

				unsigned int fileCrc = ComputeHash(fullPath.c_str());

				// Append CRC as 4 bytes little-endian
				combinedData.push_back(static_cast<unsigned char>((fileCrc >> 0) & 0xFF));
				combinedData.push_back(static_cast<unsigned char>((fileCrc >> 8) & 0xFF));
				combinedData.push_back(static_cast<unsigned char>((fileCrc >> 16) & 0xFF));
				combinedData.push_back(static_cast<unsigned char>((fileCrc >> 24) & 0xFF));
			}

			// Compute CRC32 over the combined per-file hashes
			Dia::Core::CRC combinedCrc(static_cast<const void*>(combinedData.data()), combinedData.size());
			return combinedCrc.Value();
		}

	} // namespace AssetCatalogue
} // namespace Dia
