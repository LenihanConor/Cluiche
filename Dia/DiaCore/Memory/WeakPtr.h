#ifndef DIA_WEAK_PTR_H
#define DIA_WEAK_PTR_H

#include "DiaCore/Memory/RefPtr.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Weak Pointer
		//
		// Non-owning observer pointer that works with RefPtr.
		// Does not keep the object alive, but can detect when it's been destroyed.
		//
		// USAGE:
		//   RefPtr<MyClass> strong = MakeRef<MyClass>();
		//   WeakPtr<MyClass> weak = strong;
		//   if (RefPtr<MyClass> locked = weak.Lock()) {
		//       // Object is still alive, use locked
		//   }
		//
		// THREAD SAFETY:
		//   - Thread-safe to create/destroy WeakPtr
		//   - Lock() is thread-safe
		//   - Safe to use across threads
		//---------------------------------------------------------------------------------------------------------------------------------

		// Forward declaration
		template <typename T>
		class WeakPtr;

		//-----------------------------------------------------------------------------
		// Control block for weak references
		// Separate from the object so it can outlive the object
		//-----------------------------------------------------------------------------
		class WeakRefControl
		{
		public:
			WeakRefControl()
				: mObjectAlive(true)
				, mWeakCount(0)
			{}

			void AddWeakRef()
			{
				mWeakCount.fetch_add(1, std::memory_order_relaxed);
			}

			void ReleaseWeakRef()
			{
				int oldCount = mWeakCount.fetch_sub(1, std::memory_order_acq_rel);
				if (oldCount == 1)
				{
					// Last weak reference released, delete control block
					delete this;
				}
			}

			void ObjectDestroyed()
			{
				mObjectAlive.store(false, std::memory_order_release);
			}

			bool IsObjectAlive() const
			{
				return mObjectAlive.load(std::memory_order_acquire);
			}

			int GetWeakCount() const
			{
				return mWeakCount.load(std::memory_order_relaxed);
			}

		private:
			std::atomic<bool> mObjectAlive;
			std::atomic<int> mWeakCount;
		};

		//-----------------------------------------------------------------------------
		// Extended RefCounted with weak reference support
		//-----------------------------------------------------------------------------
		class RefCountedWeakable : public RefCounted
		{
		public:
			RefCountedWeakable()
				: mWeakControl(nullptr)
			{}

			virtual ~RefCountedWeakable()
			{
				if (mWeakControl)
				{
					mWeakControl->ObjectDestroyed();
					mWeakControl->ReleaseWeakRef();
				}
			}

			WeakRefControl* GetWeakControl() const
			{
				if (!mWeakControl)
				{
					mWeakControl = new WeakRefControl();
					mWeakControl->AddWeakRef(); // Control block ref
				}
				return mWeakControl;
			}

		private:
			mutable WeakRefControl* mWeakControl;
		};

		//-----------------------------------------------------------------------------
		// Weak pointer implementation
		//-----------------------------------------------------------------------------
		template <typename T>
		class WeakPtr
		{
		public:
			// Constructors
			WeakPtr()
				: mPtr(nullptr)
				, mControl(nullptr)
			{}

			WeakPtr(const RefPtr<T>& strong)
				: mPtr(strong.Get())
				, mControl(nullptr)
			{
				if (mPtr)
				{
					// Get or create control block
					RefCountedWeakable* weakable = dynamic_cast<RefCountedWeakable*>(mPtr);
					if (weakable)
					{
						mControl = weakable->GetWeakControl();
						mControl->AddWeakRef();
					}
				}
			}

			WeakPtr(const WeakPtr& other)
				: mPtr(other.mPtr)
				, mControl(other.mControl)
			{
				if (mControl)
				{
					mControl->AddWeakRef();
				}
			}

			template <typename U>
			WeakPtr(const WeakPtr<U>& other)
				: mPtr(other.mPtr)
				, mControl(other.mControl)
			{
				if (mControl)
				{
					mControl->AddWeakRef();
				}
			}

			// Move constructor
			WeakPtr(WeakPtr&& other) noexcept
				: mPtr(other.mPtr)
				, mControl(other.mControl)
			{
				other.mPtr = nullptr;
				other.mControl = nullptr;
			}

			// Destructor
			~WeakPtr()
			{
				Release();
			}

			// Assignment
			WeakPtr& operator=(const WeakPtr& other)
			{
				if (this != &other)
				{
					Release();
					mPtr = other.mPtr;
					mControl = other.mControl;
					if (mControl)
					{
						mControl->AddWeakRef();
					}
				}
				return *this;
			}

			WeakPtr& operator=(const RefPtr<T>& strong)
			{
				Release();
				mPtr = strong.Get();
				if (mPtr)
				{
					RefCountedWeakable* weakable = dynamic_cast<RefCountedWeakable*>(mPtr);
					if (weakable)
					{
						mControl = weakable->GetWeakControl();
						mControl->AddWeakRef();
					}
				}
				return *this;
			}

			WeakPtr& operator=(WeakPtr&& other) noexcept
			{
				if (this != &other)
				{
					Release();
					mPtr = other.mPtr;
					mControl = other.mControl;
					other.mPtr = nullptr;
					other.mControl = nullptr;
				}
				return *this;
			}

			// Try to lock and get a strong reference
			// Returns null RefPtr if object has been destroyed
			RefPtr<T> Lock() const
			{
				if (mControl && mControl->IsObjectAlive())
				{
					return RefPtr<T>(mPtr);
				}
				return RefPtr<T>();
			}

			// Check if the object is still alive
			bool IsExpired() const
			{
				return !mControl || !mControl->IsObjectAlive();
			}

			// Check if this is valid (has a control block)
			bool IsValid() const
			{
				return mControl != nullptr;
			}

			// Reset to null
			void Reset()
			{
				Release();
			}

			// Get weak reference count
			int GetWeakCount() const
			{
				return mControl ? mControl->GetWeakCount() : 0;
			}

			// Comparison
			bool operator==(const WeakPtr& other) const
			{
				return mPtr == other.mPtr;
			}

			bool operator!=(const WeakPtr& other) const
			{
				return mPtr != other.mPtr;
			}

		private:
			void Release()
			{
				if (mControl)
				{
					mControl->ReleaseWeakRef();
					mControl = nullptr;
				}
				mPtr = nullptr;
			}

			T* mPtr;
			WeakRefControl* mControl;

			template <typename U>
			friend class WeakPtr;
		};
	}
}

#endif // DIA_WEAK_PTR_H
