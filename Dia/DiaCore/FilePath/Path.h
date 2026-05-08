#pragma once

#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String512.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Path
		//
		// Path utility class for managing file system paths.
		//
		// FEATURES:
		//   - Path aliasing via StringCRC for fast lookup
		//   - Path normalization (forward slashes, no trailing slashes)
		//   - Path concatenation with automatic separator handling
		//   - Executable path resolution
		//
		// USAGE:
		//   Path dataPath("data", "C:/Game/Assets/Data");
		//   Path::String combined;
		//   Path::AppendStrings(dataPath.GetPath(), "textures", combined);
		//---------------------------------------------------------------------------------------------------------------------------------
		class Path
		{
		public:
			typedef StringCRC Alias;                      // CRC-based path alias for fast lookup
			typedef Containers::String256 String;         // Path string type

			// Get the directory containing the executable
			static void ExePath(std::string& outString);

			// Concatenate two path strings with proper separator handling
			// Normalizes slashes and ensures single separator between parts
			static void AppendStrings(const String& str1, const String& str2, String& outString);

			// Resolve a relative path against a base directory into an absolute path.
			// Handles "." (base dir itself), "./" prefix (strip and append), and bare relative paths.
			static void ResolveRelative(const char* baseDir, const char* relativePath, Containers::String512& outPath);

			// Normalize path string: converts backslashes to forward slashes, removes trailing slash
			static void CleanPathString(String& outString);

			// Default constructor
			Path();

			// Construct with alias and path string (path will be normalized)
			Path(const Alias& alias, const String& path);

			// Accessors
			const Alias GetAlias()const;
			const String GetPath()const;

		private:
			// Internal path normalization helpers
			static void ReplaceAllBackSlashWithForwardSlash(Path::String& outString);
			static void RemoveEndingForwardSlash(Path::String& outString);

			Alias mAlias;      // Path identifier (CRC hash)
			String mPath;      // Actual path string (normalized)
		};
	}
}