#pragma once

#include "DiaCore/FilePath/FilePath.h"

#include "DiaCore/Strings/String256.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/CRC/StripStringCRC.h"

#include <map>

namespace Dia
{
	namespace Core
	{
		// This helper class is used to abstact file path locations.
		// Specific paths are set at applcation run time that are used as roots
		// to every other path from then on.
		class FilePathStore
		{
		public:
			static void RegisterPathRootToStore(const FilePath::PathAlias& pathalias, const FilePath::Path& path);
			static bool IsPathAliasRegistered(const FilePath::PathAlias& pathalias);

			static const char* ResolvePathToCString(const FilePath::PathAlias& pathalias);
			static const FilePath::Path& ResolvePathToString(const FilePath::PathAlias& pathalias);

		private:
			// key/value: PathAlias/Path
			typedef std::map<StripStringCRC, FilePath::Path> PathStore;

			static PathStore sPathRootStore;
		}; 
	}
}