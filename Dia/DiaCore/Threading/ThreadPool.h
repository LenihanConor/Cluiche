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

			// Optional per-worker lifecycle hooks. Both run on the worker thread itself
			// with the worker's index (0..N-1). onStart runs before the worker takes any
			// task; onStop runs after the shutdown drain, just before the thread exits.
			// Exceptions thrown by hooks are swallowed.
			struct ThreadHooks
			{
				std::function<void(unsigned int workerIndex)> onStart;
				std::function<void(unsigned int workerIndex)> onStop;
			};

			explicit ThreadPool(unsigned int numThreads = 0);
			ThreadPool(unsigned int numThreads, ThreadHooks hooks);
			~ThreadPool();

			void Enqueue(Task task);
			void WaitAll();
			size_t GetThreadCount() const;
			size_t GetPendingTaskCount() const;
			size_t GetQueueDepth() const { return GetPendingTaskCount(); }
			int    GetActiveTaskCount() const { return mActiveTasks.load(std::memory_order_acquire); }
			uint64_t GetSubmittedCount() const { return mSubmittedCount.load(std::memory_order_relaxed); }
			uint64_t GetCompletedCount() const { return mCompletedCount.load(std::memory_order_relaxed); }
			void Shutdown();

		private:
			void WorkerThread(unsigned int workerIndex);

			ThreadHooks mHooks;
			std::vector<std::thread> mThreads;
			std::queue<Task> mTasks;

			mutable std::mutex mQueueMutex;
			std::condition_variable mCondition;
			std::condition_variable mWaitCondition;

			std::atomic<int>      mActiveTasks{0};
			std::atomic<uint64_t> mSubmittedCount{0};
			std::atomic<uint64_t> mCompletedCount{0};
			bool mShutdown;
		};
	}
}

#endif // DIA_THREAD_POOL_H
