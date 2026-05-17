#include "DiaCore/Threading/ThreadPool.h"

namespace Dia
{
	namespace Core
	{
		ThreadPool::ThreadPool(unsigned int numThreads)
			: mShutdown(false)
		{
			if (numThreads == 0)
			{
				numThreads = Thread::GetHardwareConcurrency();
				if (numThreads == 0) numThreads = 4;
			}

			mThreads.reserve(numThreads);
			for (unsigned int i = 0; i < numThreads; ++i)
			{
				mThreads.push_back(std::thread([this]() { WorkerThread(); }));
			}
		}

		ThreadPool::~ThreadPool()
		{
			Shutdown();
		}

		void ThreadPool::Enqueue(Task task)
		{
			{
				std::unique_lock<std::mutex> lock(mQueueMutex);
				mTasks.push(std::move(task));
			}
			mCondition.notify_one();
		}

		void ThreadPool::WaitAll()
		{
			std::unique_lock<std::mutex> lock(mQueueMutex);
			mWaitCondition.wait(lock, [this]() {
				return mTasks.empty() && mActiveTasks == 0;
			});
		}

		size_t ThreadPool::GetThreadCount() const
		{
			return mThreads.size();
		}

		size_t ThreadPool::GetPendingTaskCount() const
		{
			std::unique_lock<std::mutex> lock(mQueueMutex);
			return mTasks.size();
		}

		void ThreadPool::Shutdown()
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

		void ThreadPool::WorkerThread()
		{
			while (true)
			{
				Task task;

				{
					std::unique_lock<std::mutex> lock(mQueueMutex);

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

				if (task)
				{
					try
					{
						task();
					}
					catch (...)
					{
						// Suppress
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
	}
}
