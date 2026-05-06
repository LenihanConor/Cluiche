#ifndef DIA_MUTEX_H
#define DIA_MUTEX_H

#include <mutex>
#include <shared_mutex>
#include <sal.h>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Mutex - Mutual Exclusion Lock
		//
		// Thread synchronization primitive for protecting shared data.
		//
		// USAGE:
		//   Mutex mutex;
		//   {
		//       ScopedLock lock(mutex);
		//       // Critical section - only one thread at a time
		//   } // Automatically unlocks
		//---------------------------------------------------------------------------------------------------------------------------------

		class Mutex
		{
		public:
			Mutex() = default;
			~Mutex() = default;

			_Acquires_lock_(mMutex)
			void Lock()
			{
				mMutex.lock();
			}

			_When_(return, _Acquires_lock_(mMutex))
			bool TryLock()
			{
				return mMutex.try_lock();
			}

			_Releases_lock_(mMutex)
			void Unlock()
			{
				mMutex.unlock();
			}

			std::mutex& GetNative() { return mMutex; }

		private:
			Mutex(const Mutex&) = delete;
			Mutex& operator=(const Mutex&) = delete;

			std::mutex mMutex;
		};

		//-----------------------------------------------------------------------------
		// Recursive Mutex - Can be locked multiple times by same thread
		//-----------------------------------------------------------------------------
		class RecursiveMutex
		{
		public:
			RecursiveMutex() = default;
			~RecursiveMutex() = default;

			_Acquires_lock_(mMutex)
			void Lock()
			{
				mMutex.lock();
			}

			_When_(return, _Acquires_lock_(mMutex))
			bool TryLock()
			{
				return mMutex.try_lock();
			}

			_Releases_lock_(mMutex)
			void Unlock()
			{
				mMutex.unlock();
			}

		private:
			RecursiveMutex(const RecursiveMutex&) = delete;
			RecursiveMutex& operator=(const RecursiveMutex&) = delete;

			std::recursive_mutex mMutex;
		};

		//-----------------------------------------------------------------------------
		// RAII Lock Guard
		//-----------------------------------------------------------------------------
		template <typename MutexType>
		class ScopedLock
		{
		public:
			explicit ScopedLock(MutexType& mutex)
				: mMutex(mutex)
			{
				mMutex.Lock();
			}

			~ScopedLock()
			{
				mMutex.Unlock();
			}

		private:
			ScopedLock(const ScopedLock&) = delete;
			ScopedLock& operator=(const ScopedLock&) = delete;

			MutexType& mMutex;
		};

		//-----------------------------------------------------------------------------
		// Read-Write Lock - Multiple readers OR single writer
		//-----------------------------------------------------------------------------
		class RWLock
		{
		public:
			RWLock() = default;
			~RWLock() = default;

			void LockRead()
			{
				mMutex.lock_shared();
			}

			void UnlockRead()
			{
				mMutex.unlock_shared();
			}

			void LockWrite()
			{
				mMutex.lock();
			}

			void UnlockWrite()
			{
				mMutex.unlock();
			}

			bool TryLockRead()
			{
				return mMutex.try_lock_shared();
			}

			bool TryLockWrite()
			{
				return mMutex.try_lock();
			}

		private:
			RWLock(const RWLock&) = delete;
			RWLock& operator=(const RWLock&) = delete;

			std::shared_mutex mMutex;
		};

		//-----------------------------------------------------------------------------
		// RAII Read Lock
		//-----------------------------------------------------------------------------
		class ScopedReadLock
		{
		public:
			explicit ScopedReadLock(RWLock& lock)
				: mLock(lock)
			{
				mLock.LockRead();
			}

			~ScopedReadLock()
			{
				mLock.UnlockRead();
			}

		private:
			ScopedReadLock(const ScopedReadLock&) = delete;
			ScopedReadLock& operator=(const ScopedReadLock&) = delete;

			RWLock& mLock;
		};

		//-----------------------------------------------------------------------------
		// RAII Write Lock
		//-----------------------------------------------------------------------------
		class ScopedWriteLock
		{
		public:
			explicit ScopedWriteLock(RWLock& lock)
				: mLock(lock)
			{
				mLock.LockWrite();
			}

			~ScopedWriteLock()
			{
				mLock.UnlockWrite();
			}

		private:
			ScopedWriteLock(const ScopedWriteLock&) = delete;
			ScopedWriteLock& operator=(const ScopedWriteLock&) = delete;

			RWLock& mLock;
		};
	}
}

#endif // DIA_MUTEX_H
