#pragma once

#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/FilePath/Path.h"
#include "DiaCore/Threading/Mutex.h"
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
		//   // Register paths at startup (before Lock())
		//   PathStore::RegisterToStore("data", "C:/Game/Data");
		//   PathStore::RegisterToStore("config", "C:/Game/Config");
		//   PathStore::Lock(); // Seal writes; only reads permitted after this point
		//
		//   // Later, resolve aliases (thread-safe, concurrent reads allowed)
		//   const char* dataPath = PathStore::ResolvePathToCString("data");
		//
		// THREAD SAFETY:
		//   Reads (IsPathAliasRegistered, ResolvePathToCString, ResolvePathToString) are
		//   concurrent-safe via shared read lock. Writes (RegisterToStore) acquire an
		//   exclusive write lock. Call Lock() once startup registration is complete to
		//   assert against any further writes in debug builds.
		//---------------------------------------------------------------------------------------------------------------------------------
		class PathStore
		{
		public:
			// Register multiple paths from a configuration object.
			// Must be called before Lock().
			static void RegisterToStore(const PathStoreConfig& filePathConfig);

			// Register a single path alias.
			// Must be called before Lock().
			static void RegisterToStore(const Path::Alias& pathalias, const Path::String& path);

			// Seal the store against further writes.
			// Call once all startup registration is complete.
			// In DEBUG builds, subsequent RegisterToStore calls will assert.
			static void Lock();

			// Check if an alias has been registered (thread-safe, concurrent reads allowed)
			static bool IsPathAliasRegistered(const Path::Alias& pathalias);

			// Resolve alias to C-string path (thread-safe, concurrent reads allowed)
			static const char* ResolvePathToCString(const Path::Alias& pathalias);

			// Resolve alias to Path::String reference (thread-safe, concurrent reads allowed)
			static const Path::String& ResolvePathToString(const Path::Alias& pathalias);

		private:
			typedef std::map<Path::Alias, Path::String> PathStoreMap;

			static PathStoreMap sPathRootStore;
			static RWLock       sLock;
			static bool         sLocked;
		};
	}
}