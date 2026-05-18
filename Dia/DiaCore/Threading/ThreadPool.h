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
		// Pool of worker threads pulling from a single FIFO queue.
		//
		// USAGE:
		//   ThreadPool pool(4);  // 4 worker threads, 0 = hardware concurrency
		//   pool.Enqueue([]() { /* work */ });
		//   pool.WaitAll();      // wait until queue is empty + no active tasks
		//
		// SHUTDOWN CONTRACT:
		//   ~ThreadPool / Shutdown() drains: workers keep pulling tasks until the
		//   queue is empty, then exit. Tasks submitted before Shutdown are
		//   guaranteed to run. Submitting after Shutdown is undefined — the task
		//   will sit in the queue with no workers to drain it.
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
