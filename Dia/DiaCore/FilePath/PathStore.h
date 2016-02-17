#pragma once

#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/FilePath/Path.h"
#include <map>

namespace Dia
{
	namespace Core
	{
		class FilePathStoreConfig;

		// This helper class is used to abstact file path locations.
		// Specific paths are set at applcation run time that are used as roots
		// to every other path from then on.
		class PathStore
		{
		public:
			//static void RegisterPathRootToStore(const Path& filePath);
			//static void RegisterPathRootToStore(const FilePathStoreConfig& filePathConfig);
			static void RegisterPathRootToStore(const Path::Alias& pathalias, const Path::String& path);

			static bool IsPathAliasRegistered(const Path::Alias& pathalias);

			static const char* ResolvePathToCString(const Path::Alias& pathalias);
			static const Path::String& ResolvePathToString(const Path::Alias& pathalias);

		private:
			// key/value: PathAlias/Path
			typedef std::map<Path::Alias, Path::String> PathStoreMap;

			static PathStoreMap sPathRootStore;
		}; 
	}
}