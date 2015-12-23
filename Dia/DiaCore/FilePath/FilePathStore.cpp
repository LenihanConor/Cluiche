#include "DiaCore/FilePath/FilePathStore.h"

#include <tuple>

namespace Dia
{	
	namespace Core
	{
		FilePathStore::PathStore FilePathStore::sPathRootStore;

		void FilePathStore::RegisterPathRootToStore(const FilePath::PathAlias& pathalias, const FilePath::Path& path)
		{
			DIA_ASSERT(!IsPathAliasRegistered(pathalias), "%s rootname is already in the path store");

			sPathRootStore.insert(std::pair<StripStringCRC, FilePath::Path>(StripStringCRC(pathalias.AsCStr()), path));
		}

		bool FilePathStore::IsPathAliasRegistered(const FilePath::PathAlias& pathalias)
		{
			return sPathRootStore.find(pathalias.AsCStr()) != sPathRootStore.end();
		}

		const char* FilePathStore::ResolvePathToCString(const FilePath::PathAlias& pathalias)
		{
			auto pathRootIter = sPathRootStore.find(pathalias.AsCStr());

			DIA_ASSERT(pathRootIter != sPathRootStore.end(), "%s rootname has not been added to store");

			if (pathRootIter != sPathRootStore.end())
			{
				return pathRootIter->second.AsCStr();
			}

			return nullptr;
		}

		const FilePath::Path& FilePathStore::ResolvePathToString(const FilePath::PathAlias& pathalias)
		{
			static FilePath::Path nullString;

			auto pathRootIter = sPathRootStore.find(pathalias.AsCStr());

			DIA_ASSERT(pathRootIter != sPathRootStore.end(), "%s rootname has not been added to store");

			if (pathRootIter != sPathRootStore.end())
			{
				return pathRootIter->second;
			}

			return nullString;
		}
	}
}