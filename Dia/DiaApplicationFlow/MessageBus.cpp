////////////////////////////////////////////////////////////////////////////////
// Filename: MessageBus.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/MessageBus.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Application
	{
		//---------------------------------------------------------------------------------------------------------
		MessageBus::MessageBus()
		{
		}

		//---------------------------------------------------------------------------------------------------------
		MessageBus::~MessageBus()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mSubscriptions.clear();
			mMessageQueue.clear();
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::Subscribe(const Dia::Core::StringCRC& messageType,
		                          const Dia::Core::StringCRC& subscriberId,
		                          MessageHandler handler)
		{
			DIA_ASSERT(handler != nullptr, "Cannot subscribe with null handler");

			std::lock_guard<std::mutex> lock(mMutex);

			// Get or create subscription list for this message type
			auto& subscriptions = mSubscriptions[messageType];

			// Check if already subscribed
			for (const auto& sub : subscriptions)
			{
				if (sub.subscriberId == subscriberId)
				{
					// Already subscribed - update handler
					const_cast<Subscription&>(sub).handler = handler;
					return;
				}
			}

			// Add new subscription
			subscriptions.push_back(Subscription(subscriberId, handler));
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::Unsubscribe(const Dia::Core::StringCRC& messageType,
		                             const Dia::Core::StringCRC& subscriberId)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			auto it = mSubscriptions.find(messageType);
			if (it == mSubscriptions.end())
			{
				return;  // No subscriptions for this message type
			}

			auto& subscriptions = it->second;

			// Remove subscription with matching ID
			for (auto subIt = subscriptions.begin(); subIt != subscriptions.end(); ++subIt)
			{
				if (subIt->subscriberId == subscriberId)
				{
					subscriptions.erase(subIt);
					break;
				}
			}

			// Clean up empty subscription lists
			if (subscriptions.empty())
			{
				mSubscriptions.erase(it);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::SendImmediate(const Dia::Core::StringCRC& messageType,
		                              const Dia::Core::StringCRC& senderId,
		                              const void* data,
		                              size_t dataSize)
		{
			// Create message (non-owning, points to caller's data)
			Message message(messageType, senderId, data, dataSize);

			// Dispatch immediately
			DispatchMessage(message);
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::PostMessage(const Dia::Core::StringCRC& messageType,
		                            const Dia::Core::StringCRC& senderId,
		                            const void* data,
		                            size_t dataSize)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			// Create message data (copies payload for safety)
			MessageData msgData(messageType, senderId, data, dataSize);

			// Add to queue
			mMessageQueue.push_back(std::move(msgData));
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::ProcessQueue()
		{
			// Move all queued messages to temporary list for processing
			// This allows new messages to be posted during processing
			std::deque<MessageData> messagesToProcess;

			{
				std::lock_guard<std::mutex> lock(mMutex);
				messagesToProcess.swap(mMessageQueue);
			}

			// Process messages in FIFO order (outside lock to avoid deadlock)
			for (const auto& msgData : messagesToProcess)
			{
				Message msg = msgData.GetMessage();
				DispatchMessage(msg);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		size_t MessageBus::GetQueuedMessageCount() const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return mMessageQueue.size();
		}

		//---------------------------------------------------------------------------------------------------------
		size_t MessageBus::GetSubscriberCount(const Dia::Core::StringCRC& messageType) const
		{
			std::lock_guard<std::mutex> lock(mMutex);

			auto it = mSubscriptions.find(messageType);
			if (it == mSubscriptions.end())
			{
				return 0;
			}

			return it->second.size();
		}

		//---------------------------------------------------------------------------------------------------------
		void MessageBus::DispatchMessage(const Message& message)
		{
			// Get copy of subscriptions (avoid holding lock during callbacks)
			std::vector<Subscription> subscriptions;

			{
				std::lock_guard<std::mutex> lock(mMutex);

				auto it = mSubscriptions.find(message.type);
				if (it == mSubscriptions.end())
				{
					return;  // No subscribers for this message type
				}

				subscriptions = it->second;
			}

			// Call all handlers (outside lock to avoid deadlock if handler posts messages)
			for (const auto& sub : subscriptions)
			{
				if (sub.handler)
				{
					try
					{
						sub.handler(message);
					}
					catch (...)
					{
						// Catch exceptions from handlers to prevent one bad handler from breaking others
						DIA_LOG_WARNING("Application", "MessageBus handler threw exception for message type %s from sender %s",
						                                 message.type.AsChar(),
						                                 message.senderId.AsChar());
					}
				}
			}
		}
	}
}
