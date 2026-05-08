#ifndef DIA_UNIQUE_PTR_H
#define DIA_UNIQUE_PTR_H

#include "DiaCore/Core/Assert.h"
#include <utility>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Unique Pointer
		//
		// Smart pointer with unique ownership semantics (like std::unique_ptr).
		// Cannot be copied, only moved.
		//
		// USAGE:
		//   UniquePtr<MyClass> ptr = MakeUnique<MyClass>(args...);
		//   UniquePtr<MyClass> ptr2 = std::move(ptr);  // Transfer ownership
		//
		// FEATURES:
		//   - Move-only (no copying)
		//   - Zero overhead (same as raw pointer)
		//   - Custom deleters supported
		//   - Array specialization
		//---------------------------------------------------------------------------------------------------------------------------------

		// Forward declarations
		template <typename T>
		struct DefaultDeleter;

		template <typename T, typename Deleter = DefaultDeleter<T>>
		class UniquePtr;

		//-----------------------------------------------------------------------------
		// Default deleter
		//-----------------------------------------------------------------------------
		template <typename T>
		struct DefaultDeleter
		{
			void operator()(T* ptr) const
			{
				delete ptr;
			}
		};

		// Array specialization
		template <typename T>
		struct DefaultDeleter<T[]>
		{
			void operator()(T* ptr) const
			{
				delete[] ptr;
			}
		};

		//-----------------------------------------------------------------------------
		// UniquePtr implementation
		//-----------------------------------------------------------------------------
		template <typename T, typename Deleter>
		class UniquePtr
		{
		public:
			// Constructors
			UniquePtr() noexcept
				: mPtr(nullptr)
			{}

			explicit UniquePtr(T* ptr) noexcept
				: mPtr(ptr)
			{}

			UniquePtr(T* ptr, const Deleter& deleter)
				: mPtr(ptr)
				, mDeleter(deleter)
			{}

			// Move constructor
			UniquePtr(UniquePtr&& other) noexcept
				: mPtr(other.mPtr)
				, mDeleter(std::move(other.mDeleter))
			{
				other.mPtr = nullptr;
			}

			template <typename U, typename E>
			UniquePtr(UniquePtr<U, E>&& other) noexcept
				: mPtr(other.Release())
				, mDeleter(std::move(other.GetDeleter()))
			{}

			// Destructor
			~UniquePtr()
			{
				Reset();
			}

			// Move assignment
			UniquePtr& operator=(UniquePtr&& other) noexcept
			{
				if (this != &other)
				{
					Reset(other.Release());
					mDeleter = std::move(other.mDeleter);
				}
				return *this;
			}

			template <typename U, typename E>
			UniquePtr& operator=(UniquePtr<U, E>&& other) noexcept
			{
				Reset(other.Release());
				mDeleter = std::move(other.GetDeleter());
				return *this;
			}

			// Deleted copy operations
			UniquePtr(const UniquePtr&) = delete;
			UniquePtr& operator=(const UniquePtr&) = delete;

			// Access operators
			T* operator->() const
			{
				DIA_ASSERT(mPtr != nullptr, "Dereferencing null UniquePtr");
				return mPtr;
			}

			T& operator*() const
			{
				DIA_ASSERT(mPtr != nullptr, "Dereferencing null UniquePtr");
				return *mPtr;
			}

			// Comparison operators
			bool operator==(const UniquePtr& other) const { return mPtr == other.mPtr; }
			bool operator!=(const UniquePtr& other) const { return mPtr != other.mPtr; }
			bool operator==(const T* ptr) const { return mPtr == ptr; }
			bool operator!=(const T* ptr) const { return mPtr != ptr; }
			bool operator<(const UniquePtr& other) const { return mPtr < other.mPtr; }

			// Bool conversion
			explicit operator bool() const { return mPtr != nullptr; }

			// Get raw pointer (does not transfer ownership)
			T* Get() const { return mPtr; }

			// Get deleter
			Deleter& GetDeleter() { return mDeleter; }
			const Deleter& GetDeleter() const { return mDeleter; }

			// Release ownership and return raw pointer
			T* Release()
			{
				T* ptr = mPtr;
				mPtr = nullptr;
				return ptr;
			}

			// Reset to null, deleting current object
			void Reset()
			{
				if (mPtr)
				{
					mDeleter(mPtr);
					mPtr = nullptr;
				}
			}

			// Reset to new pointer
			void Reset(T* ptr)
			{
				if (mPtr != ptr)
				{
					if (mPtr)
					{
						mDeleter(mPtr);
					}
					mPtr = ptr;
				}
			}

			// Swap with another UniquePtr
			void Swap(UniquePtr& other) noexcept
			{
				T* tempPtr = mPtr;
				mPtr = other.mPtr;
				other.mPtr = tempPtr;

				Deleter tempDeleter = std::move(mDeleter);
				mDeleter = std::move(other.mDeleter);
				other.mDeleter = std::move(tempDeleter);
			}

		private:
			T* mPtr;
			Deleter mDeleter;

			template <typename U, typename E>
			friend class UniquePtr;
		};

		//-----------------------------------------------------------------------------
		// Array specialization
		//-----------------------------------------------------------------------------
		template <typename T, typename Deleter>
		class UniquePtr<T[], Deleter>
		{
		public:
			UniquePtr() noexcept : mPtr(nullptr) {}
			explicit UniquePtr(T* ptr) noexcept : mPtr(ptr) {}
			UniquePtr(T* ptr, const Deleter& deleter) : mPtr(ptr), mDeleter(deleter) {}

			UniquePtr(UniquePtr&& other) noexcept
				: mPtr(other.mPtr), mDeleter(std::move(other.mDeleter))
			{
				other.mPtr = nullptr;
			}

			~UniquePtr() { Reset(); }

			UniquePtr& operator=(UniquePtr&& other) noexcept
			{
				if (this != &other)
				{
					Reset(other.Release());
					mDeleter = std::move(other.mDeleter);
				}
				return *this;
			}

			UniquePtr(const UniquePtr&) = delete;
			UniquePtr& operator=(const UniquePtr&) = delete;

			// Array access
			T& operator[](size_t index) const
			{
				DIA_ASSERT(mPtr != nullptr, "Accessing null UniquePtr array");
				return mPtr[index];
			}

			explicit operator bool() const { return mPtr != nullptr; }
			T* Get() const { return mPtr; }
			Deleter& GetDeleter() { return mDeleter; }
			const Deleter& GetDeleter() const { return mDeleter; }

			T* Release()
			{
				T* ptr = mPtr;
				mPtr = nullptr;
				return ptr;
			}

			void Reset()
			{
				if (mPtr)
				{
					mDeleter(mPtr);
					mPtr = nullptr;
				}
			}

			void Reset(T* ptr)
			{
				if (mPtr != ptr)
				{
					if (mPtr) mDeleter(mPtr);
					mPtr = ptr;
				}
			}

		private:
			T* mPtr;
			Deleter mDeleter;
		};

		//-----------------------------------------------------------------------------
		// Helper function to create UniquePtr
		//-----------------------------------------------------------------------------
		template <typename T, typename... Args>
		UniquePtr<T> MakeUnique(Args&&... args)
		{
			return UniquePtr<T>(new T(std::forward<Args>(args)...));
		}

		// Array version
		template <typename T>
		UniquePtr<T[]> MakeUniqueArray(size_t size)
		{
			return UniquePtr<T[]>(new T[size]);
		}
	}
}

#endif // DIA_UNIQUE_PTR_H
