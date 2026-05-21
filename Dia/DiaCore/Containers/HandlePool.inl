#pragma once

namespace Dia
{
	namespace Core
	{
		template<typename T, uint32_t kCapacity>
		HandlePool<T, kCapacity>::HandlePool()
			: mFreeListHead(0)
			, mLiveCount(0)
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				mGeneration[i] = 0;
				mNextFreeSlot[i] = i + 1;
			}
			mNextFreeSlot[kCapacity - 1] = kInvalidIndex;
		}

		template<typename T, uint32_t kCapacity>
		HandlePool<T, kCapacity>::~HandlePool()
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (IsSlotLive(i))
					SlotPtr(i)->~T();
			}
		}

		template<typename T, uint32_t kCapacity>
		Handle<T> HandlePool<T, kCapacity>::Allocate()
		{
			DIA_ASSERT(mFreeListHead != kInvalidIndex, "HandlePool::Allocate — pool is full");
			if (mFreeListHead == kInvalidIndex)
				return Handle<T>::Invalid();

			uint32_t index = mFreeListHead;
			mFreeListHead = mNextFreeSlot[index];

			// Even generation → bump to odd (live). Covers never-used (0→1) and post-free (2→3, 4→5 …).
			if ((mGeneration[index] & 1u) == 0u)
				++mGeneration[index];

			::new (SlotPtr(index)) T();
			++mLiveCount;

			return Handle<T>(index, mGeneration[index]);
		}

		template<typename T, uint32_t kCapacity>
		template<typename... Args>
		Handle<T> HandlePool<T, kCapacity>::Allocate(Args&&... args)
		{
			DIA_ASSERT(mFreeListHead != kInvalidIndex, "HandlePool::Allocate — pool is full");
			if (mFreeListHead == kInvalidIndex)
				return Handle<T>::Invalid();

			uint32_t index = mFreeListHead;
			mFreeListHead = mNextFreeSlot[index];

			if ((mGeneration[index] & 1u) == 0u)
				++mGeneration[index];

			::new (SlotPtr(index)) T(static_cast<Args&&>(args)...);
			++mLiveCount;

			return Handle<T>(index, mGeneration[index]);
		}

		template<typename T, uint32_t kCapacity>
		bool HandlePool<T, kCapacity>::Free(Handle<T> handle)
		{
			if (!IsValid(handle))
				return false;

			uint32_t index = handle.GetIndex();
			SlotPtr(index)->~T();

			// Odd→even transition marks slot dead. Assert before wrap to 0.
			DIA_ASSERT(mGeneration[index] != 0xFFFFFFFFu, "HandlePool::Free — generation counter about to wrap");
			++mGeneration[index];

			mNextFreeSlot[index] = mFreeListHead;
			mFreeListHead = index;
			--mLiveCount;

			return true;
		}

		template<typename T, uint32_t kCapacity>
		T* HandlePool<T, kCapacity>::Get(Handle<T> handle)
		{
			if (!IsValid(handle))
				return nullptr;
			return SlotPtr(handle.GetIndex());
		}

		template<typename T, uint32_t kCapacity>
		const T* HandlePool<T, kCapacity>::Get(Handle<T> handle) const
		{
			if (!IsValid(handle))
				return nullptr;
			return SlotPtr(handle.GetIndex());
		}

		template<typename T, uint32_t kCapacity>
		bool HandlePool<T, kCapacity>::IsValid(Handle<T> handle) const
		{
			if (!handle.IsValid())
				return false;
			uint32_t index = handle.GetIndex();
			if (index >= kCapacity)
				return false;
			return mGeneration[index] == handle.GetGeneration();
		}

		template<typename T, uint32_t kCapacity>
		uint32_t HandlePool<T, kCapacity>::GetSize() const
		{
			return mLiveCount;
		}

		template<typename T, uint32_t kCapacity>
		bool HandlePool<T, kCapacity>::IsFull() const
		{
			return mFreeListHead == kInvalidIndex;
		}

		template<typename T, uint32_t kCapacity>
		bool HandlePool<T, kCapacity>::IsEmpty() const
		{
			return mLiveCount == 0;
		}

		template<typename T, uint32_t kCapacity>
		template<typename Visitor>
		void HandlePool<T, kCapacity>::ForEach(const Visitor& visitor)
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (IsSlotLive(i))
					visitor(Handle<T>(i, mGeneration[i]), *SlotPtr(i));
			}
		}

		template<typename T, uint32_t kCapacity>
		template<typename Visitor>
		void HandlePool<T, kCapacity>::ForEach(const Visitor& visitor) const
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (IsSlotLive(i))
					visitor(Handle<T>(i, mGeneration[i]), *SlotPtr(i));
			}
		}

		template<typename T, uint32_t kCapacity>
		T* HandlePool<T, kCapacity>::SlotPtr(uint32_t index)
		{
			return reinterpret_cast<T*>(&mStorage[sizeof(T) * index]);
		}

		template<typename T, uint32_t kCapacity>
		const T* HandlePool<T, kCapacity>::SlotPtr(uint32_t index) const
		{
			return reinterpret_cast<const T*>(&mStorage[sizeof(T) * index]);
		}

		// Odd generation = live (Allocate bumps even→odd; Free bumps odd→even). 0 = never allocated.
		template<typename T, uint32_t kCapacity>
		bool HandlePool<T, kCapacity>::IsSlotLive(uint32_t index) const
		{
			return (mGeneration[index] & 1u) == 1u;
		}
	}
}
