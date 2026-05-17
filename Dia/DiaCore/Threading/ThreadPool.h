#ifndef DIA_THREAD_POOL_H
#define DIA_THREAD_POOL_H

#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <queue>
#include <functional>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Thread Pool
		//
		// Pool of worker threads for executing tasks concurrently.
		//
		// USAGE:
		//   ThreadPool pool(4);  // 4 worker threads
		//   pool.Enqueue([]() {
		//       // Work here
		//   });
		//   pool.WaitAll();  // Wait for all tasks to complete
		//
		// FEATURES:
		//   - Fixed number of worker threads
		//   - Work-stealing task queue
		//   - Automatic load balancing
		//---------------------------------------------------------------------------------------------------------------------------------

		class ThreadPool
		{
		public:
			using Task = std::function<void()>;

			explicit ThreadPool(unsigned int numThreads = 0);
			~ThreadPool();

			void Enqueue(Task task);
			void WaitAll();
			size_t GetThreadCount() const;
			size_t GetPendingTaskCount() const;
			void Shutdown();

		private:
			void WorkerThread();

			std::vector<std::thread> mThreads;
			std::queue<Task> mTasks;

			mutable std::mutex mQueueMutex;
			std::condition_variable mCondition;
			std::condition_variable mWaitCondition;

			std::atomic<int> mActiveTasks{0};
			bool mShutdown;
		};
	}
}

#endif // DIA_THREAD_POOL_H
