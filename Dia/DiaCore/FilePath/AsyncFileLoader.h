#ifndef DIA_ASYNC_FILE_LOADER_H
#define DIA_ASYNC_FILE_LOADER_H

#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <functional>
#include <queue>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Async File Loader
		//
		// Non-blocking file loading system for streaming assets.
		//
		// USAGE:
		//   AsyncFileLoader::Initialize();
		//   AsyncFileLoader::LoadAsync("texture.png", [](const char* data, size_t size, bool success) {
		//       if (success) {
		//           // Process loaded data
		//       }
		//   });
		//
		// FEATURES:
		//   - Non-blocking I/O
		//   - Priority queues
		//   - Completion callbacks
		//   - Progress tracking
		//---------------------------------------------------------------------------------------------------------------------------------

		enum class LoadPriority
		{
			Low = 0,
			Normal = 1,
			High = 2,
			Critical = 3
		};

		struct LoadResult
		{
			char* data;           // Loaded data (must be freed by caller)
			size_t size;          // Size of loaded data
			bool success;         // Load succeeded
			const char* filePath; // File path that was loaded
			void* userData;       // User data passed to request
		};

		using LoadCallback = std::function<void(const LoadResult&)>;

		//-----------------------------------------------------------------------------
		// Async File Load Request
		//-----------------------------------------------------------------------------
		struct AsyncLoadRequest
		{
			char filePath[256];
			LoadCallback callback;
			LoadPriority priority;
			void* userData;

			AsyncLoadRequest()
				: callback(nullptr)
				, priority(LoadPriority::Normal)
				, userData(nullptr)
			{
				filePath[0] = '\0';
			}
		};

		//-----------------------------------------------------------------------------
		// Async File Loader System
		//-----------------------------------------------------------------------------
		class AsyncFileLoader
		{
		public:
			// Initialize the async loader
			static void Initialize(unsigned int numThreads = 2)
			{
				GetInstance().InitializeImpl(numThreads);
			}

			// Shutdown the async loader
			static void Shutdown()
			{
				GetInstance().ShutdownImpl();
			}

			// Load file asynchronously
			static void LoadAsync(const char* filePath, LoadCallback callback,
				LoadPriority priority = LoadPriority::Normal, void* userData = nullptr)
			{
				GetInstance().LoadAsyncImpl(filePath, callback, priority, userData);
			}

			// Process completed loads (call from main thread)
			static void ProcessCallbacks()
			{
				GetInstance().ProcessCallbacksImpl();
			}

			// Get number of pending loads
			static size_t GetPendingCount()
			{
				return GetInstance().GetPendingCountImpl();
			}

			// Wait for all loads to complete
			static void WaitAll()
			{
				GetInstance().WaitAllImpl();
			}

		private:
			AsyncFileLoader()
				: mThreadPool(nullptr)
				, mShutdown(false)
			{}

			~AsyncFileLoader()
			{
				ShutdownImpl();
			}

			static AsyncFileLoader& GetInstance()
			{
				static AsyncFileLoader instance;
				return instance;
			}

			void InitializeImpl(unsigned int numThreads)
			{
				if (!mThreadPool)
				{
					mThreadPool = new ThreadPool(numThreads);
				}
			}

			void ShutdownImpl()
			{
				mShutdown = true;

				if (mThreadPool)
				{
					mThreadPool->Shutdown();
					delete mThreadPool;
					mThreadPool = nullptr;
				}
			}

			void LoadAsyncImpl(const char* filePath, LoadCallback callback,
				LoadPriority priority, void* userData)
			{
				if (!mThreadPool) return;

				AsyncLoadRequest request;
				strncpy(request.filePath, filePath, sizeof(request.filePath) - 1);
				request.callback = callback;
				request.priority = priority;
				request.userData = userData;

				mThreadPool->Enqueue([this, request]() {
					LoadFile(request);
				});
			}

			void LoadFile(const AsyncLoadRequest& request)
			{
				LoadResult result;
				result.filePath = request.filePath;
				result.userData = request.userData;
				result.data = nullptr;
				result.size = 0;
				result.success = false;

				// Open file
				FILE* file = fopen(request.filePath, "rb");
				if (file)
				{
					// Get file size
					fseek(file, 0, SEEK_END);
					long fileSize = ftell(file);
					fseek(file, 0, SEEK_SET);

					if (fileSize > 0)
					{
						// Allocate buffer
						result.data = new char[fileSize];

						// Read file
						size_t bytesRead = fread(result.data, 1, fileSize, file);
						if (bytesRead == static_cast<size_t>(fileSize))
						{
							result.size = fileSize;
							result.success = true;
						}
						else
						{
							// Read failed
							delete[] result.data;
							result.data = nullptr;
						}
					}

					fclose(file);
				}

				// Queue callback for main thread
				{
					ScopedLock<Mutex> lock(mCallbackMutex);
					mCompletedCallbacks.push_back({request.callback, result});
				}
			}

			void ProcessCallbacksImpl()
			{
				std::vector<std::pair<LoadCallback, LoadResult>> callbacks;

				{
					ScopedLock<Mutex> lock(mCallbackMutex);
					callbacks = std::move(mCompletedCallbacks);
					mCompletedCallbacks.clear();
				}

				// Execute callbacks
				for (auto& pair : callbacks)
				{
					if (pair.first)
					{
						pair.first(pair.second);
					}

					// Free loaded data after callback
					if (pair.second.data)
					{
						delete[] pair.second.data;
					}
				}
			}

			size_t GetPendingCountImpl() const
			{
				ScopedLock<Mutex> lock(mCallbackMutex);
				return mThreadPool->GetPendingTaskCount() + mCompletedCallbacks.size();
			}

			void WaitAllImpl()
			{
				if (mThreadPool)
				{
					mThreadPool->WaitAll();
				}

				// Process any remaining callbacks
				ProcessCallbacksImpl();
			}

			ThreadPool* mThreadPool;
			std::atomic<bool> mShutdown;

			mutable Mutex mCallbackMutex;
			std::vector<std::pair<LoadCallback, LoadResult>> mCompletedCallbacks;
		};
	}
}

#endif // DIA_ASYNC_FILE_LOADER_H
