#include "DiaCore/FilePath/PathStore.h"

#include <tuple>

namespace Dia
{	
	namespace Core
	{
		PathStore::PathStoreMap PathStore::sPathRootStore;

	/*	void PathStore::RegisterPathRootToStore(const FilePath& filePath)
		{
			DIA_ASSERT(!IsPathAliasRegistered(filePath.GetPathAlias()), "%s rootname is already in the path store");

			sPathRootStore.insert(std::pair<StripStringCRC, FilePath::Path>(StripStringCRC(filePath.GetPathAlias().AsCStr()), filePath.));
		}
		*/
		void PathStore::RegisterPathRootToStore(const Path::Alias& pathalias, const Path::String& path)
		{
			DIA_ASSERT(!IsPathAliasRegistered(pathalias), "%s rootname is already in the path store");

			sPathRootStore.insert(std::pair<Path::Alias, Path::String>(pathalias, path));
		}

		bool PathStore::IsPathAliasRegistered(const Path::Alias& pathalias)
		{
			return sPathRootStore.find(pathalias) != sPathRootStore.end();
		}

		const char* PathStore::ResolvePathToCString(const Path::Alias& pathalias)
		{
			auto pathRootIter = sPathRootStore.find(pathalias);

			DIA_ASSERT(pathRootIter != sPathRootStore.end(), "%s rootname has not been added to store");

			if (pathRootIter != sPathRootStore.end())
			{
				return pathRootIter->second.AsCStr();
			}

			return nullptr;
		}

		const Path::String& PathStore::ResolvePathToString(const Path::Alias& pathalias)
		{
			static Path::String nullString;

			auto pathRootIter = sPathRootStore.find(pathalias);

			DIA_ASSERT(pathRootIter != sPathRootStore.end(), "%s rootname has not been added to store");

			if (pathRootIter != sPathRootStore.end())
			{
				return pathRootIter->second;
			}

			return nullString;
		}
	}
}