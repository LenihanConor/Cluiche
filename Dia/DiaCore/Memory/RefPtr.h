#ifndef DIA_REF_PTR_H
#define DIA_REF_PTR_H

#include "DiaCore/Core/Assert.h"
#include <atomic>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Reference Counted Pointer
		//
		// Thread-safe reference counted smart pointer for shared ownership.
		// Uses atomic operations for thread-safe ref counting.
		//
		// USAGE:
		//   class MyClass : public RefCounted { ... };
		//   RefPtr<MyClass> ptr = new MyClass();
		//   RefPtr<MyClass> ptr2 = ptr;  // Shares ownership
		//
		// THREAD SAFETY:
		//   - Safe to copy/assign RefPtr across threads
		//   - Reference count is atomic
		//   - Destruction is thread-safe
		//---------------------------------------------------------------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		// Base class for reference counted objects
		// Inherit from this to make your class reference countable
		//-----------------------------------------------------------------------------
		class RefCounted
		{
		public:
			RefCounted()
				: mRefCount(0)
			{}

			virtual ~RefCounted()
			{
				DIA_ASSERT(mRefCount == 0, "Object destroyed with non-zero ref count");
			}

			// Increment reference count (thread-safe)
			void AddRef() const
			{
				mRefCount.fetch_add(1, std::memory_order_relaxed);
			}

			// Decrement reference count and return new value (thread-safe)
			int Release() const
			{
				int oldCount = mRefCount.fetch_sub(1, std::memory_order_acq_rel);
				DIA_ASSERT(oldCount > 0, "Releasing object with zero ref count");
				return oldCount - 1;
			}

			// Get current reference count (thread-safe read)
			int GetRefCount() const
			{
				return mRefCount.load(std::memory_order_relaxed);
			}

		private:
			// Prevent copying
			RefCounted(const RefCounted&) = delete;
			RefCounted& operator=(const RefCounted&) = delete;

			mutable std::atomic<int> mRefCount;
		};

		//-----------------------------------------------------------------------------
		// Reference counted smart pointer
		//-----------------------------------------------------------------------------
		template <typename T>
		class RefPtr
		{
		public:
			// Constructors
			RefPtr()
				: mPtr(nullptr)
			{}

			RefPtr(T* ptr)
				: mPtr(ptr)
			{
				if (mPtr)
				{
					mPtr->AddRef();
				}
			}

			RefPtr(const RefPtr& other)
				: mPtr(other.mPtr)
			{
				if (mPtr)
				{
					mPtr->AddRef();
				}
			}

			template <typename U>
			RefPtr(const RefPtr<U>& other)
				: mPtr(other.Get())
			{
				if (mPtr)
				{
					mPtr->AddRef();
				}
			}

			// Move constructor
			RefPtr(RefPtr&& other) noexcept
				: mPtr(other.mPtr)
			{
				other.mPtr = nullptr;
			}

			template <typename U>
			RefPtr(RefPtr<U>&& other) noexcept
				: mPtr(other.Get())
			{
				other.mPtr = nullptr;
			}

			// Destructor
			~RefPtr()
			{
				Release();
			}

			// Assignment operators
			RefPtr& operator=(const RefPtr& other)
			{
				if (this != &other)
				{
					Release();
					mPtr = other.mPtr;
					if (mPtr)
					{
						mPtr->AddRef();
					}
				}
				return *this;
			}

			template <typename U>
			RefPtr& operator=(const RefPtr<U>& other)
			{
				Release();
				mPtr = other.Get();
				if (mPtr)
				{
					mPtr->AddRef();
				}
				return *this;
			}

			RefPtr& operator=(RefPtr&& other) noexcept
			{
				if (this != &other)
				{
					Release();
					mPtr = other.mPtr;
					other.mPtr = nullptr;
				}
				return *this;
			}

			template <typename U>
			RefPtr& operator=(RefPtr<U>&& other) noexcept
			{
				Release();
				mPtr = other.Get();
				other.mPtr = nullptr;
				return *this;
			}

			RefPtr& operator=(T* ptr)
			{
				if (mPtr != ptr)
				{
					Release();
					mPtr = ptr;
					if (mPtr)
					{
						mPtr->AddRef();
					}
				}
				return *this;
			}

			// Access operators
			T* operator->() const
			{
				DIA_ASSERT(mPtr != nullptr, "Dereferencing null RefPtr");
				return mPtr;
			}

			T& operator*() const
			{
				DIA_ASSERT(mPtr != nullptr, "Dereferencing null RefPtr");
				return *mPtr;
			}

			// Comparison operators
			bool operator==(const RefPtr& other) const { return mPtr == other.mPtr; }
			bool operator!=(const RefPtr& other) const { return mPtr != other.mPtr; }
			bool operator==(const T* ptr) const { return mPtr == ptr; }
			bool operator!=(const T* ptr) const { return mPtr != ptr; }
			bool operator<(const RefPtr& other) const { return mPtr < other.mPtr; }

			// Bool conversion
			explicit operator bool() const { return mPtr != nullptr; }

			// Get raw pointer
			T* Get() const { return mPtr; }

			// Reset to null, releasing current reference
			void Reset()
			{
				Release();
				mPtr = nullptr;
			}

			// Reset to new pointer
			void Reset(T* ptr)
			{
				if (mPtr != ptr)
				{
					Release();
					mPtr = ptr;
					if (mPtr)
					{
						mPtr->AddRef();
					}
				}
			}

			// Swap with another RefPtr
			void Swap(RefPtr& other)
			{
				T* temp = mPtr;
				mPtr = other.mPtr;
				other.mPtr = temp;
			}

		private:
			void Release()
			{
				if (mPtr)
				{
					if (mPtr->Release() == 0)
					{
						delete mPtr;
					}
					mPtr = nullptr;
				}
			}

			T* mPtr;

			template <typename U>
			friend class RefPtr;
		};

		// Helper function to create RefPtr
		template <typename T, typename... Args>
		RefPtr<T> MakeRef(Args&&... args)
		{
			return RefPtr<T>(new T(std::forward<Args>(args)...));
		}

		// Static cast for RefPtr
		template <typename T, typename U>
		RefPtr<T> StaticRefCast(const RefPtr<U>& ptr)
		{
			return RefPtr<T>(static_cast<T*>(ptr.Get()));
		}

		// Dynamic cast for RefPtr
		template <typename T, typename U>
		RefPtr<T> DynamicRefCast(const RefPtr<U>& ptr)
		{
			return RefPtr<T>(dynamic_cast<T*>(ptr.Get()));
		}
	}
}

#endif // DIA_REF_PTR_H
