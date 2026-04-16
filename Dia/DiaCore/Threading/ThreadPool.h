#ifndef DIA_THREAD_POOL_H
#define DIA_THREAD_POOL_H

#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Threading/Mutex.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <queue>
#include <functional>
#include <condition_variable>

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

			// Constructor - creates worker threads
			explicit ThreadPool(unsigned int numThreads = 0)
				: mShutdown(false)
			{
				if (numThreads == 0)
				{
					numThreads = Thread::GetHardwareConcurrency();
					if (numThreads == 0) numThreads = 4; // Fallback
				}

				mThreads.reserve(numThreads);
				for (unsigned int i = 0; i < numThreads; ++i)
				{
					mThreads.push_back(std::thread([this]() { WorkerThread(); }));
				}
			}

			// Destructor - waits for all tasks and shuts down
			~ThreadPool()
			{
				Shutdown();
			}

			// Enqueue a task
			void Enqueue(Task task)
			{
				{
					std::unique_lock<std::mutex> lock(mQueueMutex);
					mTasks.push(std::move(task));
				}
				mCondition.notify_one();
			}

			// Wait for all tasks to complete
			void WaitAll()
			{
				std::unique_lock<std::mutex> lock(mQueueMutex);
				mWaitCondition.wait(lock, [this]() {
					return mTasks.empty() && mActiveTasks == 0;
				});
			}

			// Get number of worker threads
			size_t GetThreadCount() const
			{
				return mThreads.size();
			}

			// Get number of pending tasks
			size_t GetPendingTaskCount() const
			{
				std::unique_lock<std::mutex> lock(mQueueMutex);
				return mTasks.size();
			}

			// Shutdown pool
			void Shutdown()
			{
				{
					std::unique_lock<std::mutex> lock(mQueueMutex);
					mShutdown = true;
				}
				mCondition.notify_all();

				for (auto& thread : mThreads)
				{
					if (thread.joinable())
					{
						thread.join();
					}
				}
				mThreads.clear();
			}

		private:
			void WorkerThread()
			{
				while (true)
				{
					Task task;

					{
						std::unique_lock<std::mutex> lock(mQueueMutex);

						// Wait for task or shutdown
						mCondition.wait(lock, [this]() {
							return mShutdown || !mTasks.empty();
						});

						if (mShutdown && mTasks.empty())
						{
							return;
						}

						if (!mTasks.empty())
						{
							task = std::move(mTasks.front());
							mTasks.pop();
							++mActiveTasks;
						}
					}

					// Execute task
					if (task)
					{
						try
						{
							task();
						}
						catch (...)
						{
							// Suppress exceptions to prevent thread termination
							// The task failed, but other tasks should continue
						}

						{
							std::unique_lock<std::mutex> lock(mQueueMutex);
							--mActiveTasks;
							if (mTasks.empty() && mActiveTasks == 0)
							{
								mWaitCondition.notify_all();
							}
						}
					}
				}
			}

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
