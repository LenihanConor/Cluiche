#ifndef DIA_DELEGATE_H
#define DIA_DELEGATE_H

#include "DiaCore/Threading/Mutex.h"
#include <functional>
#include <vector>

namespace Dia
{
	namespace Core
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Delegate
			//
			// Multi-cast delegate system for callbacks.
			// Similar to C# delegates or Qt signals/slots.
			//
			// USAGE:
			//   Delegate<int, float> onDamage;
			//
			//   // Subscribe
			//   int id = onDamage.Add([](int damage, float multiplier) {
			//       // Handle damage
			//   });
			//
			//   // Invoke all subscribers
			//   onDamage.Invoke(10, 1.5f);
			//
			//   // Unsubscribe
			//   onDamage.Remove(id);
			//
			// FEATURES:
			//   - Multiple subscribers
			//   - Thread-safe add/remove
			//   - Lambda support
			//   - Member function binding
			//   - Return subscription ID for unsubscribing
			//---------------------------------------------------------------------------------------------------------------------------------

			//-----------------------------------------------------------------------------
			// Delegate with no parameters
			//-----------------------------------------------------------------------------
			template <typename... Args>
			class Delegate
			{
			public:
				using CallbackFunc = std::function<void(Args...)>;
				using CallbackID = int;

				Delegate()
					: mNextId(1)
				{}

				~Delegate()
				{
					Clear();
				}

				// Add callback, returns ID for later removal
				CallbackID Add(CallbackFunc callback)
				{
					if (!callback) return 0;

					ScopedLock<Mutex> lock(mMutex);

					CallbackID id = mNextId++;
					mCallbacks.push_back({id, callback});
					return id;
				}

				// Remove callback by ID
				bool Remove(CallbackID id)
				{
					ScopedLock<Mutex> lock(mMutex);

					for (auto it = mCallbacks.begin(); it != mCallbacks.end(); ++it)
					{
						if (it->id == id)
						{
							mCallbacks.erase(it);
							return true;
						}
					}
					return false;
				}

				// Clear all callbacks
				void Clear()
				{
					ScopedLock<Mutex> lock(mMutex);
					mCallbacks.clear();
				}

				// Invoke all callbacks
				void Invoke(Args... args)
				{
					std::vector<CallbackEntry> callbacks;

					{
						ScopedLock<Mutex> lock(mMutex);
						callbacks = mCallbacks;
					}

					for (auto& entry : callbacks)
					{
						if (entry.callback)
						{
							entry.callback(args...);
						}
					}
				}

				// Operator() overload for convenient invocation
				void operator()(Args... args)
				{
					Invoke(args...);
				}

				// Get number of subscribers
				size_t GetSubscriberCount() const
				{
					ScopedLock<Mutex> lock(mMutex);
					return mCallbacks.size();
				}

				// Check if has any subscribers
				bool HasSubscribers() const
				{
					return GetSubscriberCount() > 0;
				}

			private:
				struct CallbackEntry
				{
					CallbackID id;
					CallbackFunc callback;
				};

				std::vector<CallbackEntry> mCallbacks;
				CallbackID mNextId;
				mutable Mutex mMutex;
			};

			//-----------------------------------------------------------------------------
			// Helper macro for member function binding
			//-----------------------------------------------------------------------------
			#define BIND_MEMBER_FUNC(func, obj) \
				[obj](auto&&... args) { return (obj)->func(std::forward<decltype(args)>(args)...); }

			//-----------------------------------------------------------------------------
			// Event-specific delegate (takes Event*)
			//-----------------------------------------------------------------------------
			class Event;
			using EventDelegate = Delegate<Event*>;

			//-----------------------------------------------------------------------------
			// Scoped Delegate Connection (RAII)
			// Automatically unsubscribes when destroyed
			//-----------------------------------------------------------------------------
			template <typename... Args>
			class ScopedDelegateConnection
			{
			public:
				ScopedDelegateConnection()
					: mDelegate(nullptr)
					, mId(0)
				{}

				ScopedDelegateConnection(Delegate<Args...>* delegate, typename Delegate<Args...>::CallbackID id)
					: mDelegate(delegate)
					, mId(id)
				{}

				~ScopedDelegateConnection()
				{
					Disconnect();
				}

				// Move semantics
				ScopedDelegateConnection(ScopedDelegateConnection&& other) noexcept
					: mDelegate(other.mDelegate)
					, mId(other.mId)
				{
					other.mDelegate = nullptr;
					other.mId = 0;
				}

				ScopedDelegateConnection& operator=(ScopedDelegateConnection&& other) noexcept
				{
					if (this != &other)
					{
						Disconnect();
						mDelegate = other.mDelegate;
						mId = other.mId;
						other.mDelegate = nullptr;
						other.mId = 0;
					}
					return *this;
				}

				// No copying
				ScopedDelegateConnection(const ScopedDelegateConnection&) = delete;
				ScopedDelegateConnection& operator=(const ScopedDelegateConnection&) = delete;

				void Disconnect()
				{
					if (mDelegate && mId != 0)
					{
						mDelegate->Remove(mId);
						mDelegate = nullptr;
						mId = 0;
					}
				}

				bool IsConnected() const
				{
					return mDelegate != nullptr && mId != 0;
				}

			private:
				Delegate<Args...>* mDelegate;
				typename Delegate<Args...>::CallbackID mId;
			};
		}
	}
}

#endif // DIA_DELEGATE_H
