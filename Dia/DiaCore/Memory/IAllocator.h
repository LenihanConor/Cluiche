#ifndef DIA_IALLOCATOR_H
#define DIA_IALLOCATOR_H

#include "DiaCore/Core/Assert.h"
#include <cstddef>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Allocator Interface
		//
		// Base interface for custom memory allocators.
		// All allocators must implement this interface.
		//
		// USAGE:
		//   IAllocator* allocator = GetPoolAllocator();
		//   void* ptr = allocator->Allocate(size, alignment);
		//   allocator->Deallocate(ptr);
		//---------------------------------------------------------------------------------------------------------------------------------

		class IAllocator
		{
		public:
			virtual ~IAllocator() {}

			// Allocate memory with specified size and alignment
			virtual void* Allocate(size_t size, size_t alignment = alignof(max_align_t)) = 0;

			// Deallocate memory
			virtual void Deallocate(void* ptr) = 0;

			// Reset allocator (if supported)
			virtual void Reset() {}

			// Get total allocated bytes
			virtual size_t GetAllocatedSize() const { return 0; }

			// Get total memory used
			virtual size_t GetUsedSize() const { return 0; }

			// Get number of allocations
			virtual size_t GetAllocationCount() const { return 0; }
		};

		//-----------------------------------------------------------------------------
		// Allocator Statistics
		//-----------------------------------------------------------------------------
		struct AllocatorStats
		{
			size_t totalAllocated;    // Total bytes allocated
			size_t totalUsed;          // Bytes currently in use
			size_t allocationCount;    // Number of active allocations
			size_t peakUsed;           // Peak memory usage
			size_t totalAllocations;   // Total allocations made (lifetime)
			size_t totalDeallocations; // Total deallocations made (lifetime)

			AllocatorStats()
				: totalAllocated(0)
				, totalUsed(0)
				, allocationCount(0)
				, peakUsed(0)
				, totalAllocations(0)
				, totalDeallocations(0)
			{}
		};

		//-----------------------------------------------------------------------------
		// Alignment helpers
		//-----------------------------------------------------------------------------
		inline size_t AlignUp(size_t value, size_t alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		inline size_t AlignDown(size_t value, size_t alignment)
		{
			return value & ~(alignment - 1);
		}

		inline bool IsAligned(size_t value, size_t alignment)
		{
			return (value & (alignment - 1)) == 0;
		}

		inline void* AlignPointer(void* ptr, size_t alignment)
		{
			return reinterpret_cast<void*>(AlignUp(reinterpret_cast<size_t>(ptr), alignment));
		}
	}
}

#endif // DIA_IALLOCATOR_H
