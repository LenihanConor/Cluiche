// ===================================================================
// Path.cpp
// Path utility implementation for file system operations
// ===================================================================

#include "DiaCore/FilePath/Path.h"

#include <windows.h>
#include <string>
#include "DiaCore/Strings/stringutils.h"

namespace Dia
{
	namespace Core
	{
		// Get the directory containing the currently running executable
		// Uses Windows-specific GetModuleFileName
		void Path::ExePath(std::string& outString)
		{
			char buffer[MAX_PATH];
			GetModuleFileNameA(NULL, buffer, MAX_PATH);
			std::string str(buffer);
			std::string::size_type pos = str.find_last_of("\\/");
			outString = str.substr(0, pos);
		}

		// Safely concatenate two path strings
		// - Normalizes both strings to use forward slashes
		// - Ensures exactly one separator between the paths
		// - Result is stored in outString
		void Path::AppendStrings(const Path::String& str1, const Path::String& str2, Path::String& outString)
		{
			Path::String temp1(str1);
			Path::String temp2(str2);

			// Normalize both paths to use forward slashes
			ReplaceAllBackSlashWithForwardSlash(temp1);
			ReplaceAllBackSlashWithForwardSlash(temp2);

			outString.Append(temp1);

			// Ensure separator between paths
			Path::String::ConstReverseIterator iter(&outString.Back(), &outString.Front(), &outString.Back());
			char lastChar = *iter.Begin();
			if (lastChar != '/')
			{
				outString.Append("/");
			}

			outString.Append(temp2);
		}

		// Normalize a path string to standard format
		// - Converts all backslashes to forward slashes
		// - Removes any trailing forward slash
		void Path::CleanPathString(String& outString)
		{
			ReplaceAllBackSlashWithForwardSlash(outString);
			RemoveEndingForwardSlash(outString);
		}

		// Default constructor
		Path::Path()
		{}

		// Construct with alias and path
		// Automatically normalizes the path string
		Path::Path(const Alias& alias, const String& path)
			: mAlias(alias)
			, mPath(path)
		{
			CleanPathString(mPath);
		}

		const Path::Alias Path::GetAlias()const { return mAlias; }
		const Path::String Path::GetPath()const { return mPath; }

		void Path::ResolveRelative(const char* baseDir, const char* relativePath, Containers::String512& outPath)
		{
			if (relativePath[0] == '.' && relativePath[1] == '/')
			{
				outPath = Containers::String512("%s%s", baseDir, relativePath + 2);
			}
			else if (relativePath[0] == '.' && relativePath[1] == '\0')
			{
				outPath = Containers::String512("%s", baseDir);
			}
			else
			{
				outPath = Containers::String512("%s%s", baseDir, relativePath);
			}
		}

		// Replace all backslashes with forward slashes
		// Loops until all backslashes are converted
		void Path::ReplaceAllBackSlashWithForwardSlash(Path::String& outString)
		{
			while (1)
			{
				int pos = outString.Find('\\');

				if (pos == -1)
				{
					break;
				}

				outString[pos] = '/';
			}
		}

		// Remove trailing forward slash from path string
		// Ensures paths don't end with a separator
		void Path::RemoveEndingForwardSlash(Path::String& outString)
		{
			Path::String::ConstReverseIterator iter(&outString.Back(), &outString.Front(), &outString.Back());
			char lastChar = *iter.Begin();
			if (lastChar == '/')
			{
				outString.Trim(outString.Size() - 1);
			}
		}
	}
}