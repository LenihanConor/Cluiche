#ifndef DIA_EVENT_QUEUE_H
#define DIA_EVENT_QUEUE_H

#include "DiaCore/Architecture/Events/Event.h"
#include "DiaCore/Threading/Mutex.h"
#include <queue>
#include <vector>

namespace Dia
{
	namespace Core
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Event Queue
			//
			// Thread-safe queue for storing and retrieving events.
			// Supports priority ordering and batch processing.
			//
			// USAGE:
			//   EventQueue queue;
			//   queue.Push(new MyEvent());
			//
			//   // Later, process events
			//   std::vector<Event*> events;
			//   queue.PopAll(events);
			//   for (Event* event : events) {
			//       // Handle event
			//       delete event;
			//   }
			//
			// FEATURES:
			//   - Thread-safe push/pop
			//   - Priority ordering (optional)
			//   - Batch processing (PopAll)
			//   - Event filtering by type/category
			//---------------------------------------------------------------------------------------------------------------------------------

			//-----------------------------------------------------------------------------
			// Event Priority
			//-----------------------------------------------------------------------------
			enum class EventPriority
			{
				Low = 0,
				Normal = 1,
				High = 2,
				Immediate = 3
			};

			//-----------------------------------------------------------------------------
			// Event Queue Entry
			//-----------------------------------------------------------------------------
			struct EventQueueEntry
			{
				Event* event;
				EventPriority priority;

				EventQueueEntry()
					: event(nullptr)
					, priority(EventPriority::Normal)
				{}

				EventQueueEntry(Event* e, EventPriority p = EventPriority::Normal)
					: event(e)
					, priority(p)
				{}

				// For priority queue ordering (higher priority = lower value in queue)
				bool operator<(const EventQueueEntry& other) const
				{
					return static_cast<int>(priority) < static_cast<int>(other.priority);
				}
			};

			//-----------------------------------------------------------------------------
			// Event Queue
			//-----------------------------------------------------------------------------
			class EventQueue
			{
			public:
				EventQueue()
					: mUsePriority(false)
				{}

				~EventQueue()
				{
					Clear();
				}

				// Enable/disable priority ordering
				void SetUsePriority(bool usePriority)
				{
					mUsePriority = usePriority;
				}

				// Push event (takes ownership)
				void Push(Event* event, EventPriority priority = EventPriority::Normal)
				{
					if (!event) return;

					ScopedLock<Mutex> lock(mMutex);

					if (mUsePriority)
					{
						mPriorityQueue.push(EventQueueEntry(event, priority));
					}
					else
					{
						mQueue.push(EventQueueEntry(event, priority));
					}
				}

				// Pop single event (returns nullptr if empty)
				Event* Pop()
				{
					ScopedLock<Mutex> lock(mMutex);

					if (mUsePriority)
					{
						if (mPriorityQueue.empty()) return nullptr;

						EventQueueEntry entry = mPriorityQueue.top();
						mPriorityQueue.pop();
						return entry.event;
					}
					else
					{
						if (mQueue.empty()) return nullptr;

						EventQueueEntry entry = mQueue.front();
						mQueue.pop();
						return entry.event;
					}
				}

				// Pop all events into vector
				void PopAll(std::vector<Event*>& events)
				{
					ScopedLock<Mutex> lock(mMutex);

					if (mUsePriority)
					{
						while (!mPriorityQueue.empty())
						{
							events.push_back(mPriorityQueue.top().event);
							mPriorityQueue.pop();
						}
					}
					else
					{
						while (!mQueue.empty())
						{
							events.push_back(mQueue.front().event);
							mQueue.pop();
						}
					}
				}

				// Peek at front event (doesn't remove)
				Event* Peek()
				{
					ScopedLock<Mutex> lock(mMutex);

					if (mUsePriority)
					{
						if (mPriorityQueue.empty()) return nullptr;
						return mPriorityQueue.top().event;
					}
					else
					{
						if (mQueue.empty()) return nullptr;
						return mQueue.front().event;
					}
				}

				// Check if empty
				bool IsEmpty() const
				{
					ScopedLock<Mutex> lock(mMutex);
					return mUsePriority ? mPriorityQueue.empty() : mQueue.empty();
				}

				// Get queue size
				size_t Size() const
				{
					ScopedLock<Mutex> lock(mMutex);
					return mUsePriority ? mPriorityQueue.size() : mQueue.size();
				}

				// Clear all events (deletes them)
				void Clear()
				{
					ScopedLock<Mutex> lock(mMutex);

					if (mUsePriority)
					{
						while (!mPriorityQueue.empty())
						{
							delete mPriorityQueue.top().event;
							mPriorityQueue.pop();
						}
					}
					else
					{
						while (!mQueue.empty())
						{
							delete mQueue.front().event;
							mQueue.pop();
						}
					}
				}

				// Filter events by type
				void PopByType(EventTypeID typeId, std::vector<Event*>& events)
				{
					ScopedLock<Mutex> lock(mMutex);

					std::queue<EventQueueEntry> tempQueue;

					while (!mQueue.empty())
					{
						EventQueueEntry entry = mQueue.front();
						mQueue.pop();

						if (entry.event->GetEventType() == typeId)
						{
							events.push_back(entry.event);
						}
						else
						{
							tempQueue.push(entry);
						}
					}

					mQueue = tempQueue;
				}

				// Filter events by category
				void PopByCategory(EventCategory category, std::vector<Event*>& events)
				{
					ScopedLock<Mutex> lock(mMutex);

					std::queue<EventQueueEntry> tempQueue;

					while (!mQueue.empty())
					{
						EventQueueEntry entry = mQueue.front();
						mQueue.pop();

						if (entry.event->IsInCategory(category))
						{
							events.push_back(entry.event);
						}
						else
						{
							tempQueue.push(entry);
						}
					}

					mQueue = tempQueue;
				}

			private:
				// Prevent copying
				EventQueue(const EventQueue&) = delete;
				EventQueue& operator=(const EventQueue&) = delete;

				std::queue<EventQueueEntry> mQueue;
				std::priority_queue<EventQueueEntry> mPriorityQueue;
				bool mUsePriority;
				mutable Mutex mMutex;
			};
		}
	}
}

#endif // DIA_EVENT_QUEUE_H
