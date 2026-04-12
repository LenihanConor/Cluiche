#ifndef DIA_ATOMIC_H
#define DIA_ATOMIC_H

#include <atomic>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Atomic Operations
		//
		// Lock-free atomic operations for thread-safe access to shared variables.
		//
		// USAGE:
		//   Atomic<int> counter(0);
		//   counter.Increment();  // Thread-safe
		//   int value = counter.Load();
		//---------------------------------------------------------------------------------------------------------------------------------

		template <typename T>
		class Atomic
		{
		public:
			Atomic() : mValue() {}
			explicit Atomic(T value) : mValue(value) {}

			// Load value (thread-safe read)
			T Load(std::memory_order order = std::memory_order_seq_cst) const
			{
				return mValue.load(order);
			}

			// Store value (thread-safe write)
			void Store(T value, std::memory_order order = std::memory_order_seq_cst)
			{
				mValue.store(value, order);
			}

			// Exchange (atomic swap)
			T Exchange(T value, std::memory_order order = std::memory_order_seq_cst)
			{
				return mValue.exchange(value, order);
			}

			// Compare and exchange (CAS operation)
			bool CompareExchange(T& expected, T desired,
				std::memory_order success = std::memory_order_seq_cst,
				std::memory_order failure = std::memory_order_seq_cst)
			{
				return mValue.compare_exchange_weak(expected, desired, success, failure);
			}

			bool CompareExchangeStrong(T& expected, T desired,
				std::memory_order success = std::memory_order_seq_cst,
				std::memory_order failure = std::memory_order_seq_cst)
			{
				return mValue.compare_exchange_strong(expected, desired, success, failure);
			}

			// Arithmetic operations (for integral types)
			T FetchAdd(T value, std::memory_order order = std::memory_order_seq_cst)
			{
				return mValue.fetch_add(value, order);
			}

			T FetchSub(T value, std::memory_order order = std::memory_order_seq_cst)
			{
				return mValue.fetch_sub(value, order);
			}

			// Increment/Decrement helpers
			T Increment(std::memory_order order = std::memory_order_seq_cst)
			{
				return mValue.fetch_add(1, order) + 1;
			}

			T Decrement(std::memory_order order = std::memory_order_seq_cst)
			{
				return mValue.fetch_sub(1, order) - 1;
			}

			// Operators
			operator T() const { return Load(); }
			T operator=(T value) { Store(value); return value; }
			T operator++() { return Increment(); }
			T operator++(int) { return FetchAdd(1); }
			T operator--() { return Decrement(); }
			T operator--(int) { return FetchSub(1); }
			T operator+=(T value) { return FetchAdd(value) + value; }
			T operator-=(T value) { return FetchSub(value) - value; }

		private:
			std::atomic<T> mValue;
		};

		//-----------------------------------------------------------------------------
		// Specialized atomic counter
		//-----------------------------------------------------------------------------
		class AtomicCounter
		{
		public:
			AtomicCounter() : mValue(0) {}
			explicit AtomicCounter(int value) : mValue(value) {}

			int Increment() { return mValue.fetch_add(1, std::memory_order_relaxed) + 1; }
			int Decrement() { return mValue.fetch_sub(1, std::memory_order_relaxed) - 1; }
			int Get() const { return mValue.load(std::memory_order_relaxed); }
			void Set(int value) { mValue.store(value, std::memory_order_relaxed); }

		private:
			std::atomic<int> mValue;
		};

		//-----------------------------------------------------------------------------
		// Atomic flag (lock-free boolean)
		//-----------------------------------------------------------------------------
		class AtomicFlag
		{
		public:
			AtomicFlag() = default;

			// Test and set (returns previous value)
			bool TestAndSet(std::memory_order order = std::memory_order_seq_cst)
			{
				return mFlag.test_and_set(order);
			}

			// Clear flag
			void Clear(std::memory_order order = std::memory_order_seq_cst)
			{
				mFlag.clear(order);
			}

		private:
			AtomicFlag(const AtomicFlag&) = delete;
			AtomicFlag& operator=(const AtomicFlag&) = delete;

			std::atomic_flag mFlag;
		};

		//-----------------------------------------------------------------------------
		// Spin Lock - Lock using atomic flag (use for very short critical sections)
		//-----------------------------------------------------------------------------
		class SpinLock
		{
		public:
			SpinLock() = default;

			void Lock()
			{
				while (mLocked.TestAndSet(std::memory_order_acquire))
				{
					// Spin wait
				}
			}

			bool TryLock()
			{
				return !mLocked.TestAndSet(std::memory_order_acquire);
			}

			void Unlock()
			{
				mLocked.Clear(std::memory_order_release);
			}

		private:
			SpinLock(const SpinLock&) = delete;
			SpinLock& operator=(const SpinLock&) = delete;

			AtomicFlag mLocked;
		};

		//-----------------------------------------------------------------------------
		// Memory ordering helpers
		//-----------------------------------------------------------------------------
		namespace MemoryOrder
		{
			constexpr auto Relaxed = std::memory_order_relaxed;
			constexpr auto Acquire = std::memory_order_acquire;
			constexpr auto Release = std::memory_order_release;
			constexpr auto AcqRel = std::memory_order_acq_rel;
			constexpr auto SeqCst = std::memory_order_seq_cst;
		}
	}
}

#endif // DIA_ATOMIC_H
