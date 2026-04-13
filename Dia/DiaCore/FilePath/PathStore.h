#pragma once

#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/FilePath/Path.h"
#include <map>

namespace Dia
{
	namespace Core
	{
		class PathStoreConfig;

		//---------------------------------------------------------------------------------------------------------------------------------
		// PathStore
		//
		// Global registry for path aliases used throughout the engine.
		//
		// Provides a centralized location for managing base paths, allowing the engine to use
		// abstract path aliases (e.g., "data", "config", "exe") that resolve to actual directories
		// at runtime. This enables:
		//   - Platform independence (different base paths per platform)
		//   - Easy reconfiguration (change paths without changing code)
		//   - Portable file paths (FilePath objects resolve via PathStore)
		//
		// USAGE:
		//   // Register paths at startup
		//   PathStore::RegisterToStore("data", "C:/Game/Data");
		//   PathStore::RegisterToStore("config", "C:/Game/Config");
		//
		//   // Later, resolve aliases
		//   const char* dataPath = PathStore::ResolvePathToCString("data");
		//
		// NOTE: This is a static class with a single global registry
		//---------------------------------------------------------------------------------------------------------------------------------
		class PathStore
		{
		public:
			// Register multiple paths from a configuration object
			// Supports: direct aliases, compound aliases (built from other aliases), and config fragments
			static void RegisterToStore(const PathStoreConfig& filePathConfig);

			// Register a single path alias
			// @param pathalias - The alias identifier (CRC hash)
			// @param path - The actual file system path
			static void RegisterToStore(const Path::Alias& pathalias, const Path::String& path);

			// Check if an alias has been registered
			static bool IsPathAliasRegistered(const Path::Alias& pathalias);

			// Resolve alias to C-string path (returns nullptr if not found)
			static const char* ResolvePathToCString(const Path::Alias& pathalias);

			// Resolve alias to Path::String reference (returns empty string if not found)
			static const Path::String& ResolvePathToString(const Path::Alias& pathalias);

		private:
			// Internal storage: maps path aliases to their resolved strings
			typedef std::map<Path::Alias, Path::String> PathStoreMap;

			static PathStoreMap sPathRootStore;
		};
	}
}