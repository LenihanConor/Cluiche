#include "DiaCore/FilePath/PathStore.h"

#include <tuple>
#include "DiaCore/FilePath/PathStoreConfig.h"
#include <DiaCore/FilePath/SerializedFileLoad.h>

namespace Dia
{	
	namespace Core
	{
		PathStore::PathStoreMap PathStore::sPathRootStore;

		void PathStore::RegisterToStore(const PathStoreConfig& filePathConfig)
		{
			// Register the alias path combinations
			{
				const PathStoreConfig::AliasPathTupleArray& aliasPathConfigArray = filePathConfig.GetAliasPathTupleArray();

				for (unsigned int i = 0; i < aliasPathConfigArray.Size(); i++)
				{
					const AliasPathConfigTuple& tuple = aliasPathConfigArray[i];
					
					RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), tuple.GetPath());
				}
			}

			//Register and build the alias that are built on each other
			{
				const PathStoreConfig::AliasAppendPathArray& aliasAppendPathConfigArray = filePathConfig.GetAliasAppendPathTupleArray();

				for (unsigned int i = 0; i < aliasAppendPathConfigArray.Size(); i++)
				{
					const AliasAppendPathConfig& tuple = aliasAppendPathConfigArray[i];

					bool isAliasRegistered = IsPathAliasRegistered(Path::Alias(tuple.GetBaseAlias().AsCStr()));
					DIA_ASSERT(isAliasRegistered, "Could not find alias to build from, %s", tuple.GetBaseAlias());
					if (isAliasRegistered)
					{
						const Path::String& baseString = ResolvePathToString(Path::Alias(tuple.GetBaseAlias().AsCStr()));
						Path::String appendString;
						Path::AppendStrings(baseString, tuple.GetPathAppend(), appendString);
						RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), appendString);
					}
				}
			}

			//Open other fragments
			{
				const PathStoreConfig::PathStoreConfigFragmentArray& fragmentArray = filePathConfig.GetPathStoreConfigFragmentArray();

				for (unsigned int i = 0; i < fragmentArray.Size(); i++)
				{
					const PathStoreConfigFragment& fragment = fragmentArray[i];
					Path::Alias pathAlias = Path::Alias(fragment.GetBaseAlias().AsCStr());
					FilePath filePath(pathAlias, fragment.GetPathAppend().AsCStr(), fragment.GetFileName());

					FilePath::ResoledFilePath bufferToResovleInto;
					filePath.Resolve(bufferToResovleInto);

					Dia::Core::PathStoreConfig pathStoreConfig;
					Dia::Core::SerializedFileLoad fileLoad;
					if (fileLoad.LoadNow(bufferToResovleInto, pathStoreConfig, 5096) == Dia::Core::IFileLoad::ReturnCode::kSuccess)
					{
						Dia::Core::PathStore::RegisterToStore(pathStoreConfig);
					}
				}
			}
		}

		void PathStore::RegisterToStore(const Path::Alias& pathalias, const Path::String& path)
		{
			DIA_ASSERT(!IsPathAliasRegistered(pathalias), "%s rootname is already in the path store");

			Path::String temp = path;
			Path::CleanPathString(temp);

			sPathRootStore.insert(std::pair<Path::Alias, Path::String>(pathalias, temp));
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