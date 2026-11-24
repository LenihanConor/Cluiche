#ifndef DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H
#define DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from size_t
#pragma warning(disable: 4244) // conversion, possible loss of data
#pragma warning(disable: 4127) // conditional expression is constant
#endif

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_CPP11_STL_
#endif
#ifndef _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_THREAD_
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Windows headers pulled in by ASIO define macros that clash with DiaCore
#ifdef Yield
#undef Yield
#endif

#include "DiaWebSocket/Message.h"
#include "DiaWebSocket/Error.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include <cstring>
#include <memory>

namespace Dia
{
	namespace WebSocket
	{
		namespace Internal
		{
			using WsppServer = websocketpp::server<websocketpp::config::asio>;
			using WsppClient = websocketpp::client<websocketpp::config::asio_client>;
			using ConnectionHdl = websocketpp::connection_hdl;

			// Heap-backed, size-to-fit message payload (replaces the old fixed 8 KB array).
			// The bytes are immutable once built and shared by reference count, so copying a
			// MessageBuffer (the fixed-capacity DynamicArrayC queues copy on Add) is O(1) and the
			// pointer returned by Data() stays valid for the lifetime of any copy. The reference
			// count is atomic, so copies may be destroyed on different threads (websocketpp
			// thread produces, Dia thread consumes). Data() is never null and always has at
			// least one readable byte; text payloads are stored null terminated.
			class MessageBuffer
			{
			public:
				MessageBuffer() = default;

				// DynamicArrayC explicitly calls ~T() on removed slots and later assigns into them
				// again, so the destructor must leave the object in a valid empty state.
				~MessageBuffer() { Reset(); }
				MessageBuffer(const MessageBuffer&) = default;
				MessageBuffer(MessageBuffer&&) = default;
				MessageBuffer& operator=(const MessageBuffer&) = default;
				MessageBuffer& operator=(MessageBuffer&&) = default;

				// Copies `length` bytes; when `nullTerminate` is set one extra '\0' byte is stored after them.
				void Assign(const void* data, size_t length, bool nullTerminate)
				{
					const size_t total = length + (nullTerminate ? 1 : 0);
					std::shared_ptr<char> storage(new char[total > 0 ? total : 1], std::default_delete<char[]>());
					if (length > 0)
					{
						std::memcpy(storage.get(), data, length);
					}
					if (nullTerminate)
					{
						storage.get()[length] = '\0';
					}
					mStorage = std::move(storage);
					mSize = total;
				}

				void Reset() { mStorage.reset(); mSize = 0; }

				const char* Data() const { return mStorage ? mStorage.get() : ""; }
				size_t Size() const { return mSize; }

			private:
				std::shared_ptr<char> mStorage;
				size_t mSize = 0;
			};

			struct QueuedEvent
			{
				enum class Type
				{
					kMessage,
					kConnected,
					kDisconnected,
					kError
				};

				Type eventType;
				int connectionId;
				Message message;
				Error error;
				MessageBuffer dataBuffer;
			};

			struct OutgoingMessage
			{
				int connectionId; // -1 = broadcast
				MessageType type;
				MessageBuffer dataBuffer;
				size_t dataLength;
			};

			static const size_t kDefaultMaxMessageSize = 1024 * 1024; // 1MB
			static const int kDefaultMaxConnections = 16;
			static const int kDefaultQueueCapacity = 64;
		}
	}
}

#endif // DIA_WEBSOCKET_INTERNAL_WEBSOCKETPP_WRAPPER_H
