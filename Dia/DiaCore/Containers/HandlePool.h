#pragma once

#include <cstdint>
#include <new>
#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		template<typename T, uint32_t kCapacity>
		class HandlePool
		{
		public:
			static_assert(kCapacity > 0, "HandlePool requires non-zero capacity");

			static const uint32_t kInvalidIndex = Handle<T>::kInvalidIndex;

			HandlePool();
			~HandlePool();

			HandlePool(const HandlePool&) = delete;
			HandlePool& operator=(const HandlePool&) = delete;
			HandlePool(HandlePool&&) = delete;
			HandlePool& operator=(HandlePool&&) = delete;

			Handle<T> Allocate();

			template<typename... Args>
			Handle<T> Allocate(Args&&... args);

			bool Free(Handle<T> handle);

			T*       Get(Handle<T> handle);
			const T* Get(Handle<T> handle) const;

			bool IsValid(Handle<T> handle) const;

			uint32_t GetSize() const;
			constexpr uint32_t GetCapacity() const { return kCapacity; }
			bool IsFull() const;
			bool IsEmpty() const;

			template<typename Visitor>
			void ForEach(const Visitor& visitor);

			template<typename Visitor>
			void ForEach(const Visitor& visitor) const;

		private:
			alignas(T) unsigned char mStorage[sizeof(T) * kCapacity];

			uint32_t mGeneration[kCapacity];
			uint32_t mNextFreeSlot[kCapacity];
			uint32_t mFreeListHead;
			uint32_t mLiveCount;

			T*       SlotPtr(uint32_t index);
			const T* SlotPtr(uint32_t index) const;
			bool     IsSlotLive(uint32_t index) const;
		};
	}
}

#include "DiaCore/Containers/HandlePool.inl"
