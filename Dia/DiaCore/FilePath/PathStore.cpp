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
		// Global path registry storage
		PathStore::PathStoreMap PathStore::sPathRootStore;

		// Register paths from a configuration object
		// Processes three types of entries:
		//   1. Direct alias->path mappings
		//   2. Compound aliases (built from existing aliases + sub-paths)
		//   3. Config fragments (load additional PathStoreConfig files)
		void PathStore::RegisterToStore(const PathStoreConfig& filePathConfig)
		{
			// Register direct alias->path mappings
			{
				const PathStoreConfig::AliasPathTupleArray& aliasPathConfigArray = filePathConfig.GetAliasPathTupleArray();

				for (unsigned int i = 0; i < aliasPathConfigArray.Size(); i++)
				{
					const AliasPathConfigTuple& tuple = aliasPathConfigArray[i];

					RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), tuple.GetPath());
				}
			}

			// Register compound aliases (built from existing aliases)
			// Example: "data_textures" = resolve("data") + "/textures"
			{
				const PathStoreConfig::AliasAppendPathArray& aliasAppendPathConfigArray = filePathConfig.GetAliasAppendPathTupleArray();

				for (unsigned int i = 0; i < aliasAppendPathConfigArray.Size(); i++)
				{
					const AliasAppendPathConfig& tuple = aliasAppendPathConfigArray[i];

					bool isAliasRegistered = IsPathAliasRegistered(Path::Alias(tuple.GetBaseAlias().AsCStr()));
					DIA_ASSERT(isAliasRegistered, "Could not find alias to build from, %s", tuple.GetBaseAlias());
					if (isAliasRegistered)
					{
						// Resolve base alias and append the sub-path
						const Path::String& baseString = ResolvePathToString(Path::Alias(tuple.GetBaseAlias().AsCStr()));
						Path::String appendString;
						Path::AppendStrings(baseString, tuple.GetPathAppend(), appendString);
						RegisterToStore(Path::Alias(tuple.GetAlias().AsCStr()), appendString);
					}
				}
			}

			// Load and register paths from external config fragments
			// Allows splitting path configuration across multiple JSON files
			{
				const PathStoreConfig::PathStoreConfigFragmentArray& fragmentArray = filePathConfig.GetPathStoreConfigFragmentArray();

				for (unsigned int i = 0; i < fragmentArray.Size(); i++)
				{
					const PathStoreConfigFragment& fragment = fragmentArray[i];
					Path::Alias pathAlias = Path::Alias(fragment.GetBaseAlias().AsCStr());
					FilePath filePath(pathAlias, fragment.GetPathAppend().AsCStr(), fragment.GetFileName());

					// Resolve the fragment file path
					FilePath::ResoledFilePath bufferToResovleInto;
					filePath.Resolve(bufferToResovleInto);

					// Load and deserialize the config fragment
					Dia::Core::PathStoreConfig pathStoreConfig;
					Dia::Core::SerializedFileLoad fileLoad;
					if (fileLoad.LoadNow(bufferToResovleInto, pathStoreConfig, 5096) == Dia::Core::IFileLoad::ReturnCode::kSuccess)
					{
						// Recursively register paths from the fragment
						Dia::Core::PathStore::RegisterToStore(pathStoreConfig);
					}
				}
			}
		}

		// Register a single path alias
		// The path is normalized before storage
		void PathStore::RegisterToStore(const Path::Alias& pathalias, const Path::String& path)
		{
			DIA_ASSERT(!IsPathAliasRegistered(pathalias), "%s rootname is already in the path store");

			// Normalize the path (forward slashes, no trailing slash)
			Path::String temp = path;
			Path::CleanPathString(temp);

			sPathRootStore.insert(std::pair<Path::Alias, Path::String>(pathalias, temp));
		}

		// Check if an alias exists in the registry
		bool PathStore::IsPathAliasRegistered(const Path::Alias& pathalias)
		{
			return sPathRootStore.find(pathalias) != sPathRootStore.end();
		}

		// Resolve path alias to C-string
		// Asserts if alias is not registered
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

		// Resolve path alias to Path::String reference
		// Returns empty string if alias not found
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