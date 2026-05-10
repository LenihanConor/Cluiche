////////////////////////////////////////////////////////////////////////////////
// Filename: MessageBus.h
//
// Unified message bus for inter-module communication.
// Provides thread-safe, type-erased messaging between modules.
//
// Usage:
//   - Modules subscribe to message types via SubscribeToMessage()
//   - Send immediate (synchronous) via SendImmediate()
//   - Post to queue (asynchronous) via PostMessage()
//   - ProcessingUnit calls ProcessQueue() each update
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _MESSAGEBUS_H_
#define _MESSAGEBUS_H_

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <vector>
#include <cstring>

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Struct name: Message
		// Description: Message envelope containing type-erased data
		////////////////////////////////////////////////////////////////////////////////
		struct Message
		{
			Dia::Core::StringCRC type;          // Message type ID
			Dia::Core::StringCRC senderId;      // Module that sent it
			Dia::Core::TimeAbsolute timestamp;  // When sent
			const void* data;                   // Payload (type-erased)
			size_t dataSize;                    // Size of payload in bytes

			Message()
				: type("")
				, senderId("")
				, timestamp(Dia::Core::TimeAbsolute::Zero())
				, data(nullptr)
				, dataSize(0)
			{}

			Message(const Dia::Core::StringCRC& t,
			        const Dia::Core::StringCRC& sender,
			        const void* d,
			        size_t size)
				: type(t)
				, senderId(sender)
				, timestamp(Dia::Core::TimeAbsolute::GetSystemTime())
				, data(d)
				, dataSize(size)
			{}

			// Type-safe data access
			template<typename T>
			const T* GetData() const
			{
				if (dataSize != sizeof(T))
				{
					return nullptr;  // Size mismatch
				}
				return static_cast<const T*>(data);
			}
		};

		////////////////////////////////////////////////////////////////////////////////
		// Type alias: MessageHandler
		// Description: Callback function for message handling
		////////////////////////////////////////////////////////////////////////////////
		using MessageHandler = std::function<void(const Message&)>;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: MessageBus
		// Description: Thread-safe message bus for module communication
		////////////////////////////////////////////////////////////////////////////////
		class MessageBus
		{
		public:
			MessageBus();
			~MessageBus();

			// Subscribe to message type (thread-safe)
			// Multiple subscribers can register for the same message type
			void Subscribe(const Dia::Core::StringCRC& messageType,
			              const Dia::Core::StringCRC& subscriberId,
			              MessageHandler handler);

			// Unsubscribe from message type
			void Unsubscribe(const Dia::Core::StringCRC& messageType,
			                const Dia::Core::StringCRC& subscriberId);

			// Send message immediately (synchronous)
			// Calls all subscribed handlers right now
			void SendImmediate(const Dia::Core::StringCRC& messageType,
			                  const Dia::Core::StringCRC& senderId,
			                  const void* data,
			                  size_t dataSize);

			// Post message to queue (asynchronous, thread-safe)
			// Message will be dispatched on next ProcessQueue() call
			void PostMessage(const Dia::Core::StringCRC& messageType,
			                const Dia::Core::StringCRC& senderId,
			                const void* data,
			                size_t dataSize);

			// Process all queued messages (called by ProcessingUnit::DoUpdate)
			// Dispatches messages in FIFO order
			void ProcessQueue();

			// Type-safe template helpers
			template<typename T>
			void SendImmediate(const Dia::Core::StringCRC& messageType,
			                  const Dia::Core::StringCRC& senderId,
			                  const T& data)
			{
				SendImmediate(messageType, senderId, &data, sizeof(T));
			}

			template<typename T>
			void PostMessage(const Dia::Core::StringCRC& messageType,
			                const Dia::Core::StringCRC& senderId,
			                const T& data)
			{
				PostMessage(messageType, senderId, &data, sizeof(T));
			}

			// Statistics
			size_t GetQueuedMessageCount() const;
			size_t GetSubscriberCount(const Dia::Core::StringCRC& messageType) const;

		private:
			// Internal message data (owns payload copy)
			struct MessageData
			{
				Dia::Core::StringCRC type;
				Dia::Core::StringCRC senderId;
				Dia::Core::TimeAbsolute timestamp;
				std::vector<char> payload;  // Owns copied data

				MessageData() : timestamp(Dia::Core::TimeAbsolute::Zero()) {}

				MessageData(const Dia::Core::StringCRC& t,
				           const Dia::Core::StringCRC& sender,
				           const void* data,
				           size_t size)
					: type(t)
					, senderId(sender)
					, timestamp(Dia::Core::TimeAbsolute::GetSystemTime())
					, payload(size)
				{
					if (data && size > 0)
					{
						std::memcpy(payload.data(), data, size);
					}
				}

				// Get message view (non-owning)
				Message GetMessage() const
				{
					Message msg;
					msg.type = type;
					msg.senderId = senderId;
					msg.timestamp = timestamp;
					msg.data = payload.empty() ? nullptr : payload.data();
					msg.dataSize = payload.size();
					return msg;
				}
			};

			// Subscription entry
			struct Subscription
			{
				Dia::Core::StringCRC subscriberId;
				MessageHandler handler;

				Subscription() {}

				Subscription(const Dia::Core::StringCRC& id, MessageHandler h)
					: subscriberId(id), handler(h)
				{}
			};

			// Hash function for StringCRC in unordered_map
			struct StringCRCHash
			{
				size_t operator()(const Dia::Core::StringCRC& crc) const
				{
					return static_cast<size_t>(crc.Value());
				}
			};

			// Dispatch message to all subscribers
			void DispatchMessage(const Message& message);

			// Thread-safety
			mutable std::mutex mMutex;

			// Subscriptions: messageType -> list of subscribers
			std::unordered_map<Dia::Core::StringCRC, std::vector<Subscription>, StringCRCHash> mSubscriptions;

			// Message queue (posted messages waiting to be processed)
			std::deque<MessageData> mMessageQueue;
		};
	}
}

#endif // _MESSAGEBUS_H_
