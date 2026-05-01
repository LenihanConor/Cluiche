// ===================================================================
// PathStore.cpp
// Global path alias registry implementation
// ===================================================================

#include "DiaCore/FilePath/PathStore.h"

#include <tuple>
#include "DiaCore/FilePath/PathStoreConfig.h"
#include <DiaCore/FilePath/SerializedFileLoad.h>

namespace Dia
{
	namespace Core
	{
		PathStore::PathStoreMap PathStore::sPathRootStore;
		RWLock                  PathStore::sLock;
		bool                    PathStore::sLocked = false;

		void PathStore::Lock()
		{
			ScopedWriteLock lock(sLock);
			sLocked = true;
		}

		// RegisterToStore(config) acquires the write lock once per single-alias call below.
		// Internal helpers (IsRegistered_Unlocked, ResolveToString_Unlocked) are called
		// without locking to avoid recursive acquisition of std::shared_mutex.
		void PathStore::RegisterToStore(const PathStoreConfig& filePathConfig)
		{
			// Direct alias->path mappings
			{
				const PathStoreConfig::AliasPathTupleArray& aliasPathConfigArray = filePathConfig.GetAliasPathTupleArray();

				for (unsigned int i = 0; i < aliasPathConfigArray.Size(); i++)
				{
					const AliasPathConfigTuple& tuple = aliasPathConfigArray[i];
					RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), tuple.GetPath());
				}
			}

			// Compound aliases (built from existing aliases)
			{
				const PathStoreConfig::AliasAppendPathArray& aliasAppendPathConfigArray = filePathConfig.GetAliasAppendPathTupleArray();

				for (unsigned int i = 0; i < aliasAppendPathConfigArray.Size(); i++)
				{
					const AliasAppendPathConfig& tuple = aliasAppendPathConfigArray[i];
					Path::Alias baseAlias(tuple.GetBaseAlias().AsCStr());

					// Read-lock just for the lookup, then release before the write below
					Path::String appendString;
					{
						ScopedReadLock readLock(sLock);
						bool isAliasRegistered = sPathRootStore.find(baseAlias) != sPathRootStore.end();
						DIA_ASSERT(isAliasRegistered, "Could not find alias to build from, %s", tuple.GetBaseAlias());
						if (!isAliasRegistered)
						{
							continue;
						}
						Path::AppendStrings(sPathRootStore.at(baseAlias), tuple.GetPathAppend(), appendString);
					}

					RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), appendString);
				}
			}

			// Config fragments (load additional PathStoreConfig files)
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
			ScopedWriteLock lock(sLock);

			DIA_ASSERT(!sLocked, "PathStore is locked; all RegisterToStore calls must happen before PathStore::Lock()");
			DIA_ASSERT(sPathRootStore.find(pathalias) == sPathRootStore.end(), "%s rootname is already in the path store");

			Path::String temp = path;
			Path::CleanPathString(temp);

			sPathRootStore.insert(std::pair<Path::Alias, Path::String>(pathalias, temp));
		}

		bool PathStore::IsPathAliasRegistered(const Path::Alias& pathalias)
		{
			ScopedReadLock lock(sLock);
			return sPathRootStore.find(pathalias) != sPathRootStore.end();
		}

		const char* PathStore::ResolvePathToCString(const Path::Alias& pathalias)
		{
			ScopedReadLock lock(sLock);

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
			ScopedReadLock lock(sLock);

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