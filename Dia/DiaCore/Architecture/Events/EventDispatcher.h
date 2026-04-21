#ifndef DIA_EVENT_DISPATCHER_H
#define DIA_EVENT_DISPATCHER_H

#include "DiaCore/Architecture/Events/Event.h"
#include "DiaCore/Architecture/Events/EventQueue.h"
#include "DiaCore/Architecture/Events/Delegate.h"
#include "DiaCore/Threading/Mutex.h"
#include <unordered_map>
#include <vector>

namespace Dia
{
	namespace Core
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Event Dispatcher
			//
			// Central system for routing events to registered handlers.
			// Supports immediate dispatch and queued dispatch.
			//
			// USAGE:
			//   EventDispatcher dispatcher;
			//
			//   // Subscribe to event type
			//   dispatcher.Subscribe<MyEvent>([](MyEvent* event) {
			//       // Handle event
			//   });
			//
			//   // Dispatch immediately
			//   MyEvent event;
			//   dispatcher.Dispatch(&event);
			//
			//   // Or queue for later
			//   dispatcher.QueueEvent(new MyEvent());
			//   dispatcher.ProcessQueue();
			//
			// FEATURES:
			//   - Type-safe event handlers
			//   - Immediate or queued dispatch
			//   - Multiple handlers per event type
			//   - Event propagation control (can stop)
			//   - Category filtering
			//---------------------------------------------------------------------------------------------------------------------------------

			class EventDispatcher
			{
			public:
				using EventHandler = std::function<void(Event*)>;
				using HandlerID = int;

				EventDispatcher()
					: mNextHandlerId(1)
				{}

				~EventDispatcher()
				{
					Clear();
				}

				// Subscribe to specific event type
				template <typename T>
				HandlerID Subscribe(std::function<void(T*)> handler)
				{
					EventTypeID typeId = T::GetStaticType();

					// Wrap typed handler
					EventHandler wrappedHandler = [handler](Event* event) {
						T* typedEvent = EventCast<T>(event);
						if (typedEvent)
						{
							handler(typedEvent);
						}
					};

					return SubscribeInternal(typeId, wrappedHandler);
				}

				// Subscribe to all events
				HandlerID SubscribeAll(EventHandler handler)
				{
					ScopedLock<Mutex> lock(mMutex);

					HandlerID id = mNextHandlerId++;
					mGlobalHandlers[id] = handler;
					return id;
				}

				// Unsubscribe
				void Unsubscribe(HandlerID id)
				{
					ScopedLock<Mutex> lock(mMutex);

					// Check global handlers
					auto globalIt = mGlobalHandlers.find(id);
					if (globalIt != mGlobalHandlers.end())
					{
						mGlobalHandlers.erase(globalIt);
						return;
					}

					// Check type-specific handlers
					for (auto& pair : mHandlers)
					{
						auto it = pair.second.find(id);
						if (it != pair.second.end())
						{
							pair.second.erase(it);
							return;
						}
					}
				}

				// Dispatch event immediately (synchronous)
				void Dispatch(Event* event)
				{
					if (!event) return;

					// Copy handlers to avoid lock during callbacks
					std::unordered_map<HandlerID, EventHandler> typeHandlers;
					std::unordered_map<HandlerID, EventHandler> globalHandlers;

					{
						ScopedLock<Mutex> lock(mMutex);

						auto it = mHandlers.find(event->GetEventType());
						if (it != mHandlers.end())
						{
							typeHandlers = it->second;
						}

						globalHandlers = mGlobalHandlers;
					}

					// Call type-specific handlers
					for (auto& pair : typeHandlers)
					{
						if (event->IsHandled()) break;
						pair.second(event);
					}

					// Call global handlers
					for (auto& pair : globalHandlers)
					{
						if (event->IsHandled()) break;
						pair.second(event);
					}
				}

				// Queue event for later processing (takes ownership)
				void QueueEvent(Event* event, EventPriority priority = EventPriority::Normal)
				{
					mEventQueue.Push(event, priority);
				}

				// Process all queued events
				void ProcessQueue()
				{
					std::vector<Event*> events;
					mEventQueue.PopAll(events);

					for (Event* event : events)
					{
						Dispatch(event);
						delete event;
					}
				}

				// Process limited number of events
				void ProcessQueue(int maxEvents)
				{
					for (int i = 0; i < maxEvents && !mEventQueue.IsEmpty(); ++i)
					{
						Event* event = mEventQueue.Pop();
						if (event)
						{
							Dispatch(event);
							delete event;
						}
					}
				}

				// Get pending event count
				size_t GetQueuedEventCount() const
				{
					return mEventQueue.Size();
				}

				// Clear all handlers
				void Clear()
				{
					ScopedLock<Mutex> lock(mMutex);
					mHandlers.clear();
					mGlobalHandlers.clear();
					mEventQueue.Clear();
				}

				// Clear event queue only
				void ClearQueue()
				{
					mEventQueue.Clear();
				}

			private:
				HandlerID SubscribeInternal(EventTypeID typeId, EventHandler handler)
				{
					ScopedLock<Mutex> lock(mMutex);

					HandlerID id = mNextHandlerId++;
					mHandlers[typeId][id] = handler;
					return id;
				}

				std::unordered_map<EventTypeID, std::unordered_map<HandlerID, EventHandler>> mHandlers;
				std::unordered_map<HandlerID, EventHandler> mGlobalHandlers;
				HandlerID mNextHandlerId;
				EventQueue mEventQueue;
				mutable Mutex mMutex;
			};

		}
	}
}

#endif // DIA_EVENT_DISPATCHER_H
