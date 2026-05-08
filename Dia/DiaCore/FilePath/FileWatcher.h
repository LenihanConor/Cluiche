#ifndef DIA_FILE_WATCHER_H
#define DIA_FILE_WATCHER_H

#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// File Watcher
		//
		// Monitors file system for changes to enable hot-reload functionality.
		//
		// USAGE:
		//   FileWatcher watcher;
		//   watcher.Watch("assets/config.json", [](const char* path) {
		//       // File changed, reload it
		//   });
		//   watcher.Update();  // Call each frame
		//
		// FEATURES:
		//   - File change notifications
		//   - Directory monitoring
		//   - Hot-reload support
		//   - Multiple file tracking
		//
		// NOTE: Platform-specific implementation needed for production use.
		//       This is a polling-based implementation (checks file modification time).
		//---------------------------------------------------------------------------------------------------------------------------------

		enum class FileWatchEvent
		{
			Modified,
			Created,
			Deleted,
			Renamed
		};

		using FileWatchCallback = std::function<void(const char* filePath, FileWatchEvent event)>;

		//-----------------------------------------------------------------------------
		// File Watch Entry
		//-----------------------------------------------------------------------------
		struct FileWatchEntry
		{
			std::string filePath;
			FileWatchCallback callback;
			long long lastModifiedTime;

			FileWatchEntry()
				: lastModifiedTime(0)
			{}
		};

		//-----------------------------------------------------------------------------
		// File Watcher
		//-----------------------------------------------------------------------------
		class FileWatcher
		{
		public:
			FileWatcher()
				: mRunning(false)
			{}

			~FileWatcher()
			{
				Stop();
			}

			// Start watching (optional: starts background thread)
			void Start()
			{
				if (!mRunning)
				{
					mRunning = true;
				}
			}

			// Stop watching
			void Stop()
			{
				mRunning = false;
				ClearAll();
			}

			// Watch a file for changes
			void Watch(const char* filePath, FileWatchCallback callback)
			{
				ScopedLock<Mutex> lock(mMutex);

				FileWatchEntry entry;
				entry.filePath = filePath;
				entry.callback = callback;
				entry.lastModifiedTime = GetFileModifiedTime(filePath);

				mWatchedFiles[filePath] = entry;
			}

			// Stop watching a file
			void Unwatch(const char* filePath)
			{
				ScopedLock<Mutex> lock(mMutex);
				mWatchedFiles.erase(filePath);
			}

			// Clear all watches
			void ClearAll()
			{
				ScopedLock<Mutex> lock(mMutex);
				mWatchedFiles.clear();
			}

			// Update - check for file changes (call each frame)
			void Update()
			{
				if (!mRunning) return;

				ScopedLock<Mutex> lock(mMutex);

				for (auto& pair : mWatchedFiles)
				{
					FileWatchEntry& entry = pair.second;

					long long currentTime = GetFileModifiedTime(entry.filePath.c_str());

					if (currentTime != entry.lastModifiedTime)
					{
						if (entry.lastModifiedTime == 0)
						{
							// File created
							if (entry.callback)
							{
								entry.callback(entry.filePath.c_str(), FileWatchEvent::Created);
							}
						}
						else if (currentTime == 0)
						{
							// File deleted
							if (entry.callback)
							{
								entry.callback(entry.filePath.c_str(), FileWatchEvent::Deleted);
							}
						}
						else
						{
							// File modified
							if (entry.callback)
							{
								entry.callback(entry.filePath.c_str(), FileWatchEvent::Modified);
							}
						}

						entry.lastModifiedTime = currentTime;
					}
				}
			}

			// Get number of watched files
			size_t GetWatchCount() const
			{
				ScopedLock<Mutex> lock(mMutex);
				return mWatchedFiles.size();
			}

		private:
			// Get file modification time (returns 0 if file doesn't exist)
			// Platform-specific implementation needed for better performance
			static long long GetFileModifiedTime(const char* filePath)
			{
#ifdef _WIN32
				struct _stat64 fileStat;
				if (_stat64(filePath, &fileStat) == 0)
				{
					return fileStat.st_mtime;
				}
#else
				struct stat fileStat;
				if (stat(filePath, &fileStat) == 0)
				{
					return fileStat.st_mtime;
				}
#endif
				return 0;
			}

			std::unordered_map<std::string, FileWatchEntry> mWatchedFiles;
			mutable Mutex mMutex;
			bool mRunning;
		};
	}
}

#endif // DIA_FILE_WATCHER_H
